/* gdkevents-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2005-2008 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>

#include <gdk/gdkdisplayprivate.h>

#include "gdkkeysyms.h"
#include "gdkquartz.h"
#include "gdkquartzdisplay.h"
#include "gdkprivate-quartz.h"
#include "gdkquartzdevicemanager-core.h"

#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15
#define GDK_LION_RESIZE 5

#define SURFACE_IS_TOPLEVEL(window)      TRUE

/* This is the window corresponding to the key window */
static GdkSurface   *current_keyboard_window;


static void append_event                        (GdkEvent  *event,
                                                 gboolean   windowing);

static GdkSurface *find_toplevel_under_pointer   (GdkDisplay *display,
                                                 NSPoint     screen_point,
                                                 gint       *x,
                                                 gint       *y);


static void
gdk_quartz_ns_notification_callback (CFNotificationCenterRef  center,
                                     void                    *observer,
                                     CFStringRef              name,
                                     const void              *object,
                                     CFDictionaryRef          userInfo)
{
  const char *setting = NULL;

  /* Translate name */
  if (CFStringCompare (name,
                       CFSTR("AppleNoRedisplayAppearancePreferenceChanged"),
                       0) == kCFCompareEqualTo)
    setting = "gtk-primary-button-warps-slider";

  if (!setting)
    return;

  gdk_display_setting_changed (_gdk_display, setting);
}

static void
gdk_quartz_events_init_notifications (void)
{
  static gboolean notifications_initialized = FALSE;

  if (notifications_initialized)
    return;
  notifications_initialized = TRUE;

  /* Initialize any handlers for notifications we want to push to GTK
   * through GdkEventSettings.
   */

  /* This is an undocumented *distributed* notification to listen for changes
   * in scrollbar jump behavior. It is used by LibreOffice and WebKit as well.
   */
  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   NULL,
                                   &gdk_quartz_ns_notification_callback,
                                   CFSTR ("AppleNoRedisplayAppearancePreferenceChanged"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);
}

void
_gdk_quartz_events_init (void)
{
  _gdk_quartz_event_loop_init ();
  gdk_quartz_events_init_notifications ();

  current_keyboard_window = g_object_ref (_gdk_root);
}

gboolean
_gdk_quartz_display_has_pending (GdkDisplay *display)
{
  return (_gdk_event_queue_find_first (display) ||
         (_gdk_quartz_event_loop_check_pending ()));
}

void
_gdk_quartz_events_break_all_grabs (guint32 time)
{
  GList *devices = NULL, *l;
  GdkSeat *seat;

  seat = gdk_display_get_default_seat (_gdk_display);

  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
  devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

  for (l = devices; l; l = l->next)
    {
      GdkDeviceGrabInfo *grab;

      grab = _gdk_display_get_last_device_grab (_gdk_display, l->data);
      if (grab)
        {
          grab->serial_end = 0;
          grab->implicit_ungrab = TRUE;
        }

      _gdk_display_device_grab_update (_gdk_display, l->data, NULL, 0);
    }

  g_list_free (devices);
}

static void
fixup_event (GdkEvent *event)
{
  if (event->any.surface)
    g_object_ref (event->any.surface);
  if (((event->any.type == GDK_ENTER_NOTIFY) ||
       (event->any.type == GDK_LEAVE_NOTIFY)) &&
      (event->crossing.child_window != NULL))
    g_object_ref (event->crossing.child_window);
  event->any.send_event = FALSE;
}

static void
append_event (GdkEvent *event,
              gboolean  windowing)
{
  GList *node;

  fixup_event (event);
  node = _gdk_event_queue_append (_gdk_display, event);

  if (windowing)
    _gdk_windowing_got_event (_gdk_display, node, event, 0);
}

static gint
gdk_event_apply_filters (NSEvent *nsevent,
			 GdkEvent *event,
			 GList **filters)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = *filters;

  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      GList *node;

      if ((filter->flags & GDK_EVENT_FILTER_REMOVED) != 0)
        {
          tmp_list = tmp_list->next;
          continue;
        }

      filter->ref_count++;
      result = filter->function (nsevent, event, filter->data);

      /* get the next node after running the function since the
         function may add or remove a next node */
      node = tmp_list;
      tmp_list = tmp_list->next;

      filter->ref_count--;
      if (filter->ref_count == 0)
        {
          *filters = g_list_remove_link (*filters, node);
          g_list_free_1 (node);
          g_free (filter);
        }

      if (result !=  GDK_FILTER_CONTINUE)
	return result;
    }

  return GDK_FILTER_CONTINUE;
}

static guint32
get_time_from_ns_event (NSEvent *event)
{
  double time = [event timestamp];

  /* cast via double->uint64 conversion to make sure that it is
   * wrapped on 32-bit machines when it overflows
   */
  return (guint32) (guint64) (time * 1000.0);
}

static int
get_mouse_button_from_ns_event (NSEvent *event)
{
  NSInteger button;

  button = [event buttonNumber];

  switch (button)
    {
    case 0:
      return 1;
    case 1:
      return 3;
    case 2:
      return 2;
    default:
      return button + 1;
    }
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_buttons (NSUInteger nsbuttons)
{
  GdkModifierType modifiers = 0;

  if (nsbuttons & (1 << 0))
    modifiers |= GDK_BUTTON1_MASK;
  if (nsbuttons & (1 << 1))
    modifiers |= GDK_BUTTON3_MASK;
  if (nsbuttons & (1 << 2))
    modifiers |= GDK_BUTTON2_MASK;
  if (nsbuttons & (1 << 3))
    modifiers |= GDK_BUTTON4_MASK;
  if (nsbuttons & (1 << 4))
    modifiers |= GDK_BUTTON5_MASK;

  return modifiers;
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_event (NSEvent *event)
{
  int button;
  GdkModifierType state = 0;

  /* This maps buttons 1 to 5 to GDK_BUTTON[1-5]_MASK */
  button = get_mouse_button_from_ns_event (event);
  if (button >= 1 && button <= 5)
    state = (1 << (button + 7));

  return state;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_flags (NSUInteger nsflags)
{
  GdkModifierType modifiers = 0;

  if (nsflags & NSAlphaShiftKeyMask)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & NSShiftKeyMask)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & NSControlKeyMask)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & NSAlternateKeyMask)
    modifiers |= GDK_MOD1_MASK;
  if (nsflags & NSCommandKeyMask)
    modifiers |= GDK_MOD2_MASK;

  return modifiers;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_event (NSEvent *nsevent)
{
  return get_keyboard_modifiers_from_ns_flags ([nsevent modifierFlags]);
}

/* Return an event mask from an NSEvent */
static GdkEventMask
get_event_mask_from_ns_event (NSEvent *nsevent)
{
  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      return GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      return GDK_BUTTON_RELEASE_MASK;
    case NSMouseMoved:
      return GDK_POINTER_MOTION_MASK;
    case NSScrollWheel:
      /* Since applications that want button press events can get
       * scroll events on X11 (since scroll wheel events are really
       * button press events there), we need to use GDK_BUTTON_PRESS_MASK too.
       */
      return GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseDragged:
      return (GDK_POINTER_MOTION_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | 
	      GDK_BUTTON1_MASK);
    case NSRightMouseDragged:
      return (GDK_POINTER_MOTION_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK | 
	      GDK_BUTTON3_MASK);
    case NSOtherMouseDragged:
      {
	GdkEventMask mask;

	mask = (GDK_POINTER_MOTION_MASK | GDK_BUTTON_MOTION_MASK);

	if (get_mouse_button_from_ns_event (nsevent) == 2)
	  mask |= (GDK_BUTTON2_MOTION_MASK | GDK_BUTTON2_MOTION_MASK | 
		   GDK_BUTTON2_MASK);

	return mask;
      }
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      return GDK_TOUCHPAD_GESTURE_MASK;
    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        switch (_gdk_quartz_keys_event_type (nsevent))
	  {
	  case GDK_KEY_PRESS:
	    return GDK_KEY_PRESS_MASK;
	  case GDK_KEY_RELEASE:
	    return GDK_KEY_RELEASE_MASK;
	  case GDK_NOTHING:
	    return 0;
	  default:
	    g_assert_not_reached ();
	  }
      }
      break;

    case NSMouseEntered:
      return GDK_ENTER_NOTIFY_MASK;

    case NSMouseExited:
      return GDK_LEAVE_NOTIFY_MASK;

    default:
      g_assert_not_reached ();
    }

  return 0;
}

static void
get_window_point_from_screen_point (GdkSurface *window,
                                    NSPoint    screen_point,
                                    gint      *x,
                                    gint      *y)
{
  NSPoint point;
  NSWindow *nswindow;

  nswindow = ((GdkSurfaceImplQuartz *)window->impl)->toplevel;

  point = [nswindow convertScreenToBase:screen_point];

  *x = point.x;
  *y = window->height - point.y;
}

static gboolean
is_mouse_button_press_event (NSEventType type)
{
  switch (type)
    {
      case NSLeftMouseDown:
      case NSRightMouseDown:
      case NSOtherMouseDown:
        return TRUE;
    }

  return FALSE;
}

static GdkSurface *
get_toplevel_from_ns_event (NSEvent *nsevent,
                            NSPoint *screen_point,
                            gint    *x,
                            gint    *y)
{
  GdkSurface *toplevel = NULL;

  if ([nsevent window])
    {
      GdkQuartzView *view;
      NSPoint point, view_point;
      NSRect view_frame;

      view = (GdkQuartzView *)[[nsevent window] contentView];

      toplevel = [view gdkSurface];

      point = [nsevent locationInWindow];
      view_point = [view convertPoint:point fromView:nil];
      view_frame = [view frame];

      /* NSEvents come in with a window set, but with window coordinates
       * out of window bounds. For e.g. moved events this is fine, we use
       * this information to properly handle enter/leave notify and motion
       * events. For mouse button press/release, we want to avoid forwarding
       * these events however, because the window they relate to is not the
       * window set in the event. This situation appears to occur when button
       * presses come in just before (or just after?) a window is resized and
       * also when a button press occurs on the OS X window titlebar.
       *
       * By setting toplevel to NULL, we do another attempt to get the right
       * toplevel window below.
       */
      if (is_mouse_button_press_event ([nsevent type]) &&
          (view_point.x < view_frame.origin.x ||
           view_point.x >= view_frame.origin.x + view_frame.size.width ||
           view_point.y < view_frame.origin.y ||
           view_point.y >= view_frame.origin.y + view_frame.size.height))
        {
          toplevel = NULL;

          /* This is a hack for button presses to break all grabs. E.g. if
           * a menu is open and one clicks on the title bar (or anywhere
           * out of window bounds), we really want to pop down the menu (by
           * breaking the grabs) before OS X handles the action of the title
           * bar button.
           *
           * Because we cannot ingest this event into GDK, we have to do it
           * here, not very nice.
           */
          _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));
        }
      else
        {
          *screen_point = [[nsevent window] convertBaseToScreen:point];

          *x = point.x;
          *y = toplevel->height - point.y;
        }
    }

  if (!toplevel)
    {
      /* Fallback used when no NSWindow set.  This happens e.g. when
       * we allow motion events without a window set in gdk_event_translate()
       * that occur immediately after the main menu bar was clicked/used.
       * This fallback will not return coordinates contained in a window's
       * titlebar.
       */
      *screen_point = [NSEvent mouseLocation];
      toplevel = find_toplevel_under_pointer (_gdk_display,
                                              *screen_point,
                                              x, y);
    }

  return toplevel;
}

static GdkEvent *
create_focus_event (GdkSurface *window,
		    gboolean   in)
{
  GdkEvent *event;
  GdkQuartzDeviceManagerCore *device_manager;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  gdk_event_set_device (event, device_manager->core_keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_keyboard));

  return event;
}


static void
generate_motion_event (GdkSurface *window)
{
  NSPoint screen_point;
  GdkEvent *event;
  gint x, y, x_root, y_root;
  GdkQuartzDeviceManagerCore *device_manager;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->any.surface = NULL;
  event->any.send_event = TRUE;

  screen_point = [NSEvent mouseLocation];

  _gdk_quartz_surface_nspoint_to_gdk_xy (screen_point, &x_root, &y_root);
  get_window_point_from_screen_point (window, screen_point, &x, &y);

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event ([NSApp currentEvent]);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = _gdk_quartz_events_get_current_keyboard_modifiers () |
                        _gdk_quartz_events_get_current_mouse_modifiers ();
  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  event->motion.device = device_manager->core_pointer;
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));

  append_event (event, TRUE);
}

/* Note: Used to both set a new focus window and to unset the old one. */
void
_gdk_quartz_events_update_focus_window (GdkSurface *window,
					gboolean   got_focus)
{
  GdkEvent *event;

  if (got_focus && window == current_keyboard_window)
    return;

  /* FIXME: Don't do this when grabbed? Or make GdkQuartzNSWindow
   * disallow it in the first place instead?
   */
  
  if (!got_focus && window == current_keyboard_window)
    {
      event = create_focus_event (current_keyboard_window, FALSE);
      append_event (event, FALSE);
      g_object_unref (current_keyboard_window);
      current_keyboard_window = NULL;
    }

  if (got_focus)
    {
      if (current_keyboard_window)
	{
	  event = create_focus_event (current_keyboard_window, FALSE);
	  append_event (event, FALSE);
	  g_object_unref (current_keyboard_window);
	  current_keyboard_window = NULL;
	}
      
      event = create_focus_event (window, TRUE);
      append_event (event, FALSE);
      current_keyboard_window = g_object_ref (window);

      /* We just became the active window.  Unlike X11, Mac OS X does
       * not send us motion events while the window does not have focus
       * ("is not key").  We send a dummy motion notify event now, so that
       * everything in the window is set to correct state.
       */
      generate_motion_event (window);
    }
}

void
_gdk_quartz_events_send_map_event (GdkSurface *window)
{
  GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

  if (!impl->toplevel)
    return;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    {
      GdkEvent event;

      event.any.type = GDK_MAP;
      event.any.surface = window;
  
      gdk_event_put (&event);
    }
}

static GdkSurface *
find_toplevel_under_pointer (GdkDisplay *display,
                             NSPoint     screen_point,
                             gint       *x,
                             gint       *y)
{
  GdkSurface *toplevel;
  GdkPointerSurfaceInfo *info;

  info = _gdk_display_get_pointer_info (display, GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager)->core_pointer);
  toplevel = info->toplevel_under_pointer;
  if (toplevel && SURFACE_IS_TOPLEVEL (toplevel))
    get_window_point_from_screen_point (toplevel, screen_point, x, y);

  if (toplevel)
    {
      /* If the coordinates are out of window bounds, this toplevel is not
       * under the pointer and we thus return NULL. This can occur when
       * toplevel under pointer has not yet been updated due to a very recent
       * window resize. Alternatively, we should no longer be relying on
       * the toplevel_under_pointer value which is maintained in gdksurface.c.
       */
      if (*x < 0 || *y < 0 || *x >= toplevel->width || *y >= toplevel->height)
        return NULL;
    }

  return toplevel;
}

static GdkSurface *
find_toplevel_for_keyboard_event (NSEvent *nsevent)
{
  GList *devices = NULL, *l;
  GdkSurface *window;
  GdkDisplay *display;
  GdkQuartzView *view;
  GdkSeat *seat;

  view = (GdkQuartzView *)[[nsevent window] contentView];
  window = [view gdkSurface];

  display = gdk_surface_get_display (window);

  seat = gdk_display_get_default_seat (display);

  devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
  devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

  for (l = devices; l; l = l->next)
    {
      GdkDeviceGrabInfo *grab;
      GdkDevice *device = l->data;

      if (gdk_device_get_source(device) != GDK_SOURCE_KEYBOARD)
        continue;

      grab = _gdk_display_get_last_device_grab (display, device);
      if (grab && grab->window && !grab->owner_events)
        {
          window = gdk_surface_get_toplevel (grab->window);
          break;
        }
    }

  g_list_free (devices);

  return window;
}

static GdkSurface *
find_toplevel_for_mouse_event (NSEvent    *nsevent,
                               gint       *x,
                               gint       *y)
{
  NSPoint screen_point;
  NSEventType event_type;
  GdkSurface *toplevel;
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;

  toplevel = get_toplevel_from_ns_event (nsevent, &screen_point, x, y);

  display = gdk_surface_get_display (toplevel);

  event_type = [nsevent type];

  /* From the docs for XGrabPointer:
   *
   * If owner_events is True and if a generated pointer event
   * would normally be reported to this client, it is reported
   * as usual. Otherwise, the event is reported with respect to
   * the grab_window and is reported only if selected by
   * event_mask. For either value of owner_events, unreported
   * events are discarded.
   */
  grab = _gdk_display_get_last_device_grab (display,
                                            GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager)->core_pointer);
  if (SURFACE_IS_TOPLEVEL (toplevel) && grab)
    {
      /* Implicit grabs do not go through XGrabPointer and thus the
       * event mask should not be checked.
       */
      if (!grab->implicit
          && (grab->event_mask & get_event_mask_from_ns_event (nsevent)) == 0)
        return NULL;

      if (grab->owner_events)
        {
          /* For owner events, we need to use the toplevel under the
           * pointer, not the window from the NSEvent, since that is
           * reported with respect to the key window, which could be
           * wrong.
           */
          GdkSurface *toplevel_under_pointer;
          gint x_tmp, y_tmp;

          toplevel_under_pointer = find_toplevel_under_pointer (display,
                                                                screen_point,
                                                                &x_tmp, &y_tmp);
          if (toplevel_under_pointer)
            {
              toplevel = toplevel_under_pointer;
              *x = x_tmp;
              *y = y_tmp;
            }

          return toplevel;
        }
      else
        {
          /* Finally check the grab window. */
          GdkSurface *grab_toplevel;

          grab_toplevel = gdk_surface_get_toplevel (grab->window);
          get_window_point_from_screen_point (grab_toplevel, screen_point,
                                              x, y);

          return grab_toplevel;
        }

      return NULL;
    }
  else 
    {
      /* The non-grabbed case. */
      GdkSurface *toplevel_under_pointer;
      gint x_tmp, y_tmp;

      /* Ignore all events but mouse moved that might be on the title
       * bar (above the content view). The reason is that otherwise
       * gdk gets confused about getting e.g. button presses with no
       * window (the title bar is not known to it).
       */
      if (event_type != NSMouseMoved)
        if (*y < 0)
          return NULL;

      /* As for owner events, we need to use the toplevel under the
       * pointer, not the window from the NSEvent.
       */
      toplevel_under_pointer = find_toplevel_under_pointer (display,
                                                            screen_point,
                                                            &x_tmp, &y_tmp);
      if (toplevel_under_pointer
          && SURFACE_IS_TOPLEVEL (toplevel_under_pointer))
        {
          GdkSurfaceImplQuartz *toplevel_impl;

          toplevel = toplevel_under_pointer;

          toplevel_impl = (GdkSurfaceImplQuartz *)toplevel->impl;

          *x = x_tmp;
          *y = y_tmp;
        }

      return toplevel;
    }

  return NULL;
}

/* This function finds the correct window to send an event to, taking
 * into account grabs, event propagation, and event masks.
 */
static GdkSurface *
find_window_for_ns_event (NSEvent *nsevent, 
                          gint    *x, 
                          gint    *y,
                          gint    *x_root,
                          gint    *y_root)
{
  GdkQuartzView *view;
  GdkSurface *toplevel;
  NSPoint screen_point;
  NSEventType event_type;

  view = (GdkQuartzView *)[[nsevent window] contentView];

  toplevel = get_toplevel_from_ns_event (nsevent, &screen_point, x, y);
  if (!toplevel)
    return NULL;
  _gdk_quartz_surface_nspoint_to_gdk_xy (screen_point, x_root, y_root);

  event_type = [nsevent type];

  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
    case NSMouseMoved:
    case NSScrollWheel:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      return find_toplevel_for_mouse_event (nsevent, x, y);
      
    case NSMouseEntered:
    case NSMouseExited:
      /* Only handle our own entered/exited events, not the ones for the
       * titlebar buttons.
       */
      if ([view trackingRect] == [nsevent trackingNumber])
        return toplevel;
      else
        return NULL;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      return find_toplevel_for_keyboard_event (nsevent);

    default:
      /* Ignore everything else. */
      break;
    }

  return NULL;
}

static void
fill_crossing_event (GdkSurface       *toplevel,
                     GdkEvent        *event,
                     NSEvent         *nsevent,
                     gint             x,
                     gint             y,
                     gint             x_root,
                     gint             y_root,
                     GdkEventType     event_type,
                     GdkCrossingMode  mode,
                     GdkNotifyType    detail)
{
  GdkQuartzDeviceManagerCore *device_manager;

  event->any.type = event_type;
  event->crossing.window = toplevel;
  event->crossing.child_window = NULL;
  event->crossing.time = get_time_from_ns_event (nsevent);
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = x_root;
  event->crossing.y_root = y_root;
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                         _gdk_quartz_events_get_current_mouse_modifiers ();

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  gdk_event_set_device (event, device_manager->core_pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));

  /* FIXME: Focus and button state? */
}

/* fill_pinch_event handles the conversion from the two OSX gesture events
   NSEventTypeMagnfiy and NSEventTypeRotate to the GDK_TOUCHPAD_PINCH event.
   The normal behavior of the OSX events is that they produce as sequence of
     1 x NSEventPhaseBegan,
     n x NSEventPhaseChanged,
     1 x NSEventPhaseEnded
   This can happen for both the Magnify and the Rotate events independently.
   As both events are summarized in one GDK_TOUCHPAD_PINCH event sequence, a
   little state machine handles the case of two NSEventPhaseBegan events in
   a sequence, e.g. Magnify(Began), Magnify(Changed)..., Rotate(Began)...
   such that PINCH(STARTED), PINCH(UPDATE).... will not show a second
   PINCH(STARTED) event.
*/
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_8_AND_LATER
static void
fill_pinch_event (GdkSurface *window,
                  GdkEvent  *event,
                  NSEvent   *nsevent,
                  gint       x,
                  gint       y,
                  gint       x_root,
                  gint       y_root)
{
  static double last_scale = 1.0;
  static enum {
    FP_STATE_IDLE,
    FP_STATE_UPDATE
  } last_state = FP_STATE_IDLE;
  GdkQuartzDeviceManagerCore *device_manager;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);

  event->any.type = GDK_TOUCHPAD_PINCH;
  event->touchpad_pinch.window = window;
  event->touchpad_pinch.time = get_time_from_ns_event (nsevent);
  event->touchpad_pinch.x = x;
  event->touchpad_pinch.y = y;
  event->touchpad_pinch.x_root = x_root;
  event->touchpad_pinch.y_root = y_root;
  event->touchpad_pinch.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->touchpad_pinch.n_fingers = 2;
  event->touchpad_pinch.dx = 0.0;
  event->touchpad_pinch.dy = 0.0;
  gdk_event_set_device (event, device_manager->core_pointer);

  switch ([nsevent phase])
    {
    case NSEventPhaseBegan:
      switch (last_state)
        {
        case FP_STATE_IDLE:
          event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
          last_state = FP_STATE_UPDATE;
          last_scale = 1.0;
          break;
        case FP_STATE_UPDATE:
          /* We have already received a PhaseBegan event but no PhaseEnded
             event. This can happen, e.g. Magnify(Began), Magnify(Change)...
             Rotate(Began), Rotate (Change),...., Magnify(End) Rotate(End)
          */
          event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
          break;
        }
      break;
    case NSEventPhaseChanged:
      event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
      break;
    case NSEventPhaseEnded:
      event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_END;
      switch (last_state)
        {
        case FP_STATE_IDLE:
          /* We are idle but have received a second PhaseEnded event.
             This can happen because we have Magnify and Rotate OSX
             event sequences. We just send a second end GDK_PHASE_END.
          */
          break;
        case FP_STATE_UPDATE:
          last_state = FP_STATE_IDLE;
          break;
        }
      break;
    case NSEventPhaseCancelled:
      event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      last_state = FP_STATE_IDLE;
      break;
    case NSEventPhaseMayBegin:
    case NSEventPhaseStationary:
      event->touchpad_pinch.phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  switch ([nsevent type])
    {
    case NSEventTypeMagnify:
      last_scale *= [nsevent magnification] + 1.0;
      event->touchpad_pinch.angle_delta = 0.0;
      break;
    case NSEventTypeRotate:
      event->touchpad_pinch.angle_delta = - [nsevent rotation] * G_PI / 180.0;
      break;
    default:
      g_assert_not_reached ();
    }
  event->touchpad_pinch.scale = last_scale;
}
#endif /* OSX Version >= 10.8 */

static void
fill_button_event (GdkSurface *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkEventType type;
  gint state;
  GdkQuartzDeviceManagerCore *device_manager;

  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_quartz_events_get_current_mouse_modifiers ();

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      type = GDK_BUTTON_PRESS;
      state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      type = GDK_BUTTON_RELEASE;
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    default:
      g_assert_not_reached ();
    }

  event->any.type = type;
  event->button.window = window;
  event->button.time = get_time_from_ns_event (nsevent);
  event->button.x = x;
  event->button.y = y;
  event->button.x_root = x_root;
  event->button.y_root = y_root;
  /* FIXME event->axes */
  event->button.state = state;
  event->button.button = get_mouse_button_from_ns_event (nsevent);
  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  event->button.device = device_manager->core_pointer;
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));
}

static void
fill_motion_event (GdkSurface *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkQuartzDeviceManagerCore *device_manager;

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event (nsevent);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                        _gdk_quartz_events_get_current_mouse_modifiers ();
  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  event->motion.device = device_manager->core_pointer;
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));
}

static void
fill_scroll_event (GdkSurface          *window,
                   GdkEvent           *event,
                   NSEvent            *nsevent,
                   gint                x,
                   gint                y,
                   gint                x_root,
                   gint                y_root,
                   gdouble             delta_x,
                   gdouble             delta_y,
                   GdkScrollDirection  direction)
{
  GdkQuartzDeviceManagerCore *device_manager;
  NSPoint point;

  point = [nsevent locationInWindow];
  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);

  event->any.type = GDK_SCROLL;
  event->scroll.window = window;
  event->scroll.time = get_time_from_ns_event (nsevent);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.x_root = x_root;
  event->scroll.y_root = y_root;
  event->scroll.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->scroll.direction = direction;
  event->scroll.device = device_manager->core_pointer;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_pointer));
}

static void
fill_key_event (GdkSurface    *window,
                GdkEvent     *event,
                NSEvent      *nsevent,
                GdkEventType  type)
{
  GdkEventPrivate *priv;
  GdkQuartzDeviceManagerCore *device_manager;

  priv = (GdkEventPrivate *) event;
  priv->windowing_data = [nsevent retain];

  event->any.type = type;
  event->key.window = window;
  event->key.time = get_time_from_ns_event (nsevent);
  event->key.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->key.hardware_keycode = [nsevent keyCode];
  gdk_event_set_scancode (event, [nsevent keyCode]);
  event->key.group = ([nsevent modifierFlags] & NSAlternateKeyMask) ? 1 : 0;
  event->key.keyval = GDK_KEY_VoidSymbol;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  gdk_event_set_device (event, device_manager->core_keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_keyboard));
  
  gdk_keymap_translate_keyboard_state (gdk_display_get_keymap (_gdk_display),
				       event->key.hardware_keycode,
				       event->key.state, 
				       event->key.group,
				       &event->key.keyval,
				       NULL, NULL, NULL);

  event->key.is_modifier = _gdk_quartz_keys_is_modifier (event->key.hardware_keycode);

  /* If the key press is a modifier, the state should include the mask
   * for that modifier but only for releases, not presses. This
   * matches the X11 backend behavior.
   */
  if (event->key.is_modifier)
    {
      int mask = 0;

      switch (event->key.keyval)
        {
        case GDK_KEY_Meta_R:
        case GDK_KEY_Meta_L:
          mask = GDK_MOD2_MASK;
          break;
        case GDK_KEY_Shift_R:
        case GDK_KEY_Shift_L:
          mask = GDK_SHIFT_MASK;
          break;
        case GDK_KEY_Caps_Lock:
          mask = GDK_LOCK_MASK;
          break;
        case GDK_KEY_Alt_R:
        case GDK_KEY_Alt_L:
          mask = GDK_MOD1_MASK;
          break;
        case GDK_KEY_Control_R:
        case GDK_KEY_Control_L:
          mask = GDK_CONTROL_MASK;
          break;
        default:
          mask = 0;
        }

      if (type == GDK_KEY_PRESS)
        event->key.state &= ~mask;
      else if (type == GDK_KEY_RELEASE)
        event->key.state |= mask;
    }

  event->key.state |= _gdk_quartz_events_get_current_mouse_modifiers ();

  /* The X11 backend adds the first virtual modifier MOD2..MOD5 are
   * mapped to. Since we only have one virtual modifier in the quartz
   * backend, calling the standard function will do.
   */
  gdk_keymap_add_virtual_modifiers (gdk_display_get_keymap (_gdk_display),
                                    &event->key.state);

  GDK_NOTE(EVENTS,
    g_message ("key %s:\t\twindow: %p  key: %12s  %d",
	  type == GDK_KEY_PRESS ? "press" : "release",
	  event->key.window,
	  event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
	  event->key.keyval));
}

static gboolean
synthesize_crossing_event (GdkSurface *window,
                           GdkEvent  *event,
                           NSEvent   *nsevent,
                           gint       x,
                           gint       y,
                           gint       x_root,
                           gint       y_root)
{
  switch ([nsevent type])
    {
    case NSMouseEntered:
      /* Enter events are considered always to be from another toplevel
       * window, this shouldn't negatively affect any app or gtk code,
       * and is the only way to make GtkMenu work. EEK EEK EEK.
       */
      if (!(window->event_mask & GDK_ENTER_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_ENTER_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_NONLINEAR);
      return TRUE;

    case NSMouseExited:
      /* See above */
      if (!(window->event_mask & GDK_LEAVE_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_LEAVE_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_NONLINEAR);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

void
_gdk_quartz_synthesize_null_key_event (GdkSurface *window)
{
  GdkEvent *event;
  GdkQuartzDeviceManagerCore *device_manager;

  event = gdk_event_new (GDK_KEY_PRESS);
  event->any.type = GDK_KEY_PRESS;
  event->key.window = window;
  event->key.state = 0;
  event->key.hardware_keycode = 0;
  event->key.group = 0;
  event->key.keyval = GDK_KEY_VoidSymbol;
  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager);
  gdk_event_set_device (event, device_manager->core_keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (device_manager->core_keyboard));
  append_event(event, FALSE);
}

GdkModifierType
_gdk_quartz_events_get_current_keyboard_modifiers (void)
{
  if (gdk_quartz_osx_version () >= GDK_OSX_SNOW_LEOPARD)
    {
      return get_keyboard_modifiers_from_ns_flags ([NSClassFromString(@"NSEvent") modifierFlags]);
    }
  else
    {
      guint carbon_modifiers = GetCurrentKeyModifiers ();
      GdkModifierType modifiers = 0;

      if (carbon_modifiers & alphaLock)
        modifiers |= GDK_LOCK_MASK;
      if (carbon_modifiers & shiftKey)
        modifiers |= GDK_SHIFT_MASK;
      if (carbon_modifiers & controlKey)
        modifiers |= GDK_CONTROL_MASK;
      if (carbon_modifiers & optionKey)
        modifiers |= GDK_MOD1_MASK;
      if (carbon_modifiers & cmdKey)
        modifiers |= GDK_MOD2_MASK;

      return modifiers;
    }
}

GdkModifierType
_gdk_quartz_events_get_current_mouse_modifiers (void)
{
  if (gdk_quartz_osx_version () >= GDK_OSX_SNOW_LEOPARD)
    {
      return get_mouse_button_modifiers_from_ns_buttons ([NSClassFromString(@"NSEvent") pressedMouseButtons]);
    }
  else
    {
      return get_mouse_button_modifiers_from_ns_buttons (GetCurrentButtonState ());
    }
}

/* Detect window resizing */

static gboolean
test_resize (NSEvent *event, GdkSurface *toplevel, gint x, gint y)
{
  GdkSurfaceImplQuartz *toplevel_impl;
  gboolean lion;

  /* Resizing from the resize indicator only begins if an NSLeftMouseButton
   * event is received in the resizing area.
   */
  toplevel_impl = (GdkSurfaceImplQuartz *)toplevel->impl;
  if ([toplevel_impl->toplevel showsResizeIndicator])
  if ([event type] == NSLeftMouseDown &&
      [toplevel_impl->toplevel showsResizeIndicator])
    {
      NSRect frame;

      /* If the resize indicator is visible and the event
       * is in the lower right 15x15 corner, we leave these
       * events to Cocoa as to be handled as resize events.
       * Applications may have widgets in this area.  These
       * will most likely be larger than 15x15 and for
       * scroll bars there are also other means to move
       * the scroll bar.  Since the resize indicator is
       * the only way of resizing windows on Mac OS, it
       * is too important to not make functional.
       */
      frame = [toplevel_impl->view bounds];
      if (x > frame.size.width - GRIP_WIDTH &&
          x < frame.size.width &&
          y > frame.size.height - GRIP_HEIGHT &&
          y < frame.size.height)
        return TRUE;
     }

  /* If we're on Lion and within 5 pixels of an edge,
   * then assume that the user wants to resize, and
   * return NULL to let Quartz get on with it. We check
   * the selector isRestorable to see if we're on 10.7.
   * This extra check is in case the user starts
   * dragging before GDK recognizes the grab.
   *
   * We perform this check for a button press of all buttons, because we
   * do receive, for instance, a right mouse down event for a GDK surface
   * for x-coordinate range [-3, 0], but we do not want to forward this
   * into GDK. Forwarding such events into GDK will confuse the pointer
   * window finding code, because there are no GdkSurfaces present in
   * the range [-3, 0].
   */
  lion = gdk_quartz_osx_version () >= GDK_OSX_LION;
  if (lion &&
      ([event type] == NSLeftMouseDown ||
       [event type] == NSRightMouseDown ||
       [event type] == NSOtherMouseDown))
    {
      if (x < GDK_LION_RESIZE ||
          x > toplevel->width - GDK_LION_RESIZE ||
          y > toplevel->height - GDK_LION_RESIZE)
        return TRUE;
    }

  return FALSE;
}

static gboolean
gdk_event_translate (GdkEvent *event,
                     NSEvent  *nsevent)
{
  NSEventType event_type;
  NSWindow *nswindow;
  GdkSurface *window;
  int x, y;
  int x_root, y_root;
  gboolean return_val;

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  event_type = [nsevent type];
  if (event_type == NSAppKitDefined)
    {
      if ([nsevent subtype] == NSApplicationDeactivatedEventType)
        _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */

      /* Leave all AppKit events to AppKit. */
      return FALSE;
    }

  if (_gdk_default_filters)
    {
      /* Apply global filters */
      GdkFilterReturn result;

      result = gdk_event_apply_filters (nsevent, event, &_gdk_default_filters);
      if (result != GDK_FILTER_CONTINUE)
        {
          return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
          goto done;
        }
    }

  nswindow = [nsevent window];

  /* Ignore events for windows not created by GDK. */
  if (nswindow && ![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
    return FALSE;

  /* Ignore events for ones with no windows */
  if (!nswindow)
    {
      GdkSurface *toplevel = NULL;

      if (event_type == NSMouseMoved)
        {
          /* Motion events received after clicking the menu bar do not have the
           * window field set.  Instead of giving up on the event immediately,
           * we first check whether this event is within our window bounds.
           */
          NSPoint screen_point = [NSEvent mouseLocation];
          gint x_tmp, y_tmp;

          toplevel = find_toplevel_under_pointer (_gdk_display,
                                                  screen_point,
                                                  &x_tmp, &y_tmp);
        }

      if (!toplevel)
        return FALSE;
    }

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([(GdkQuartzNSWindow *)nswindow isInMove])
    {
      _gdk_quartz_events_break_all_grabs (get_time_from_ns_event (nsevent));
      return FALSE;
    }

  /* Also when in a manual resize or move , we ignore events so that
   * these are pushed to GdkQuartzNSWindow's sendEvent handler.
   */
  if ([(GdkQuartzNSWindow *)nswindow isInManualResizeOrMove])
    return FALSE;

  /* Find the right GDK surface to send the event to, taking grabs and
   * event masks into consideration.
   */
  window = find_window_for_ns_event (nsevent, &x, &y, &x_root, &y_root);
  if (!window)
    return FALSE;

  /* Quartz handles resizing on its own, so we want to stay out of the way. */
  if (test_resize (nsevent, window, x, y))
    return FALSE;

  /* Apply any window filters. */
  if (GDK_IS_SURFACE (window))
    {
      GdkFilterReturn result;

      if (window->filters)
	{
	  g_object_ref (window);

	  result = gdk_event_apply_filters (nsevent, event, &window->filters);

	  g_object_unref (window);

	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	      goto done;
	    }
	}
    }

  /* If the app is not active leave the event to AppKit so the window gets
   * focused correctly and don't do click-through (so we behave like most
   * native apps). If the app is active, we focus the window and then handle
   * the event, also to match native apps.
   */
  if ((event_type == NSRightMouseDown ||
       event_type == NSOtherMouseDown ||
       event_type == NSLeftMouseDown))
    {
      GdkSurfaceImplQuartz *impl = GDK_SURFACE_IMPL_QUARTZ (window->impl);

      if (![NSApp isActive])
        {
          [NSApp activateIgnoringOtherApps:YES];
          return FALSE;
        }
      else if (![impl->toplevel isKeyWindow])
        {
          GdkDeviceGrabInfo *grab;

          grab = _gdk_display_get_last_device_grab (_gdk_display,
                                                    GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_device_manager)->core_pointer);
          if (!grab)
            [impl->toplevel makeKeyWindow];
        }
    }

  return_val = TRUE;

  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      fill_button_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSMouseMoved:
      fill_motion_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case NSScrollWheel:
      {
        GdkScrollDirection direction;
	float dx;
	float dy;
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
	if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
	    [nsevent hasPreciseScrollingDeltas])
	  {
	    dx = [nsevent scrollingDeltaX];
	    dy = [nsevent scrollingDeltaY];
            direction = GDK_SCROLL_SMOOTH;

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                               -dx, -dy, direction);

            /* Fall through for scroll buttons emulation */
	  }
#endif
        dx = [nsevent deltaX];
        dy = [nsevent deltaY];

        if (dy != 0.0)
          {
            if (dy < 0.0)
              direction = GDK_SCROLL_DOWN;
            else
              direction = GDK_SCROLL_UP;

            dy = fabs (dy);
            dx = 0.0;
          }
        else if (dx != 0.0)
          {
            if (dx < 0.0)
              direction = GDK_SCROLL_RIGHT;
            else
              direction = GDK_SCROLL_LEFT;

            dx = fabs (dx);
            dy = 0.0;
          }

        if (dx != 0.0 || dy != 0.0)
          {
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
	    if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
		[nsevent hasPreciseScrollingDeltas])
              {
                GdkEvent *emulated_event;

                emulated_event = gdk_event_new (GDK_SCROLL);
                gdk_event_set_pointer_emulated (emulated_event, TRUE);
                fill_scroll_event (window, emulated_event, nsevent,
                                   x, y, x_root, y_root,
                                   dx, dy, direction);
                append_event (emulated_event, TRUE);
              }
            else
#endif
              fill_scroll_event (window, event, nsevent,
                                 x, y, x_root, y_root,
                                 dx, dy, direction);
          }
      }
      break;
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_8_AND_LATER
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      /* Event handling requires [NSEvent phase] which was introduced in 10.7 */
      /* However - Tests on 10.7 showed that phase property does not work     */
      if (gdk_quartz_osx_version () >= GDK_OSX_MOUNTAIN_LION)
        fill_pinch_event (window, event, nsevent, x, y, x_root, y_root);
      else
        return_val = FALSE;
      break;
#endif
    case NSMouseExited:
      if (SURFACE_IS_TOPLEVEL (window))
          [[NSCursor arrowCursor] set];
      /* fall through */
    case NSMouseEntered:
      return_val = synthesize_crossing_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        GdkEventType type;

        type = _gdk_quartz_keys_event_type (nsevent);
        if (type == GDK_NOTHING)
          return_val = FALSE;
        else
          fill_key_event (window, event, nsevent, type);
      }
      break;

    default:
      /* Ignore everything elsee. */
      return_val = FALSE;
      break;
    }

 done:
  if (return_val)
    {
      if (event->any.surface)
	g_object_ref (event->any.surface);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.child_window != NULL))
	g_object_ref (event->crossing.child_window);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.surface = NULL;
      event->any.type = GDK_NOTHING;
    }

  return return_val;
}

void
_gdk_quartz_display_queue_events (GdkDisplay *display)
{  
  NSEvent *nsevent;

  nsevent = _gdk_quartz_event_loop_get_pending ();
  if (nsevent)
    {
      GdkEvent *event;
      GList *node;

      event = gdk_event_new (GDK_NOTHING);

      event->any.surface = NULL;
      event->any.send_event = FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      node = _gdk_event_queue_append (display, event);

      if (gdk_event_translate (event, nsevent))
        {
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
          _gdk_windowing_got_event (display, node, event, 0);
        }
      else
        {
	  _gdk_event_queue_remove_link (display, node);
	  g_list_free_1 (node);
	  g_object_unref (event);

          [NSApp sendEvent:nsevent];
        }

      _gdk_quartz_event_loop_release_event (nsevent);
    }
}

gboolean
_gdk_quartz_get_setting (const gchar *name,
                         GValue      *value)
{
  if (strcmp (name, "gtk-double-click-time") == 0)
    {
      NSUserDefaults *defaults;
      float t;

      GDK_QUARTZ_ALLOC_POOL;

      defaults = [NSUserDefaults standardUserDefaults];
            
      t = [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
      if (t == 0.0)
	{
	  /* No user setting, use the default in OS X. */
	  t = 0.5;
	}

      GDK_QUARTZ_RELEASE_POOL;

      g_value_set_int (value, t * 1000);

      return TRUE;
    }
  else if (strcmp (name, "gtk-font-name") == 0)
    {
      NSString *name;
      char *str;
      gint size;

      GDK_QUARTZ_ALLOC_POOL;

      name = [[NSFont systemFontOfSize:0] familyName];
      size = (gint)[[NSFont userFontOfSize:0] pointSize];

      /* Let's try to use the "views" font size (12pt) by default. This is
       * used for lists/text/other "content" which is the largest parts of
       * apps, using the "regular control" size (13pt) looks a bit out of
       * place. We might have to tweak this.
       */

      /* The size has to be hardcoded as there doesn't seem to be a way to
       * get the views font size programmatically.
       */
      str = g_strdup_printf ("%s %d", [name UTF8String], size);
      g_value_set_string (value, str);
      g_free (str);

      GDK_QUARTZ_RELEASE_POOL;

      return TRUE;
    }
  else if (strcmp (name, "gtk-primary-button-warps-slider") == 0)
    {
      GDK_QUARTZ_ALLOC_POOL;

      BOOL setting = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];

      /* If the Apple property is YES, it means "warp" */
      g_value_set_boolean (value, setting == YES);

      GDK_QUARTZ_RELEASE_POOL;

      return TRUE;
    }
  else if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    {
      GDK_QUARTZ_ALLOC_POOL;

      g_value_set_boolean (value, TRUE);

      GDK_QUARTZ_RELEASE_POOL;

      return TRUE;
    }
  
  /* FIXME: Add more settings */

  return FALSE;
}

void
_gdk_quartz_display_event_data_copy (GdkDisplay     *display,
                                     const GdkEvent *src,
                                     GdkEvent       *dst)
{
  GdkEventPrivate *priv_src = (GdkEventPrivate *) src;
  GdkEventPrivate *priv_dst = (GdkEventPrivate *) dst;

  if (priv_src->windowing_data)
    {
      priv_dst->windowing_data = priv_src->windowing_data;
      [(NSEvent *)priv_dst->windowing_data retain];
    }
}

void
_gdk_quartz_display_event_data_free (GdkDisplay *display,
                                     GdkEvent   *event)
{
  GdkEventPrivate *priv = (GdkEventPrivate *) event;

  if (priv->windowing_data)
    {
      [(NSEvent *)priv->windowing_data release];
      priv->windowing_data = NULL;
    }
}
