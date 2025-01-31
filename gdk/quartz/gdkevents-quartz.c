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

#include "gdkscreen.h"
#include "gdkkeysyms.h"
#include "gdkquartz.h"
#include "gdkquartzdisplay.h"
#include "gdkprivate-quartz.h"
#include "gdkinternal-quartz.h"
#include "gdkquartz-cocoa-access.h"
#include "gdkquartzdevicemanager-core.h"
#include "gdkquartzkeys.h"
#include "gdkkeys-quartz.h"

#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15
#define GDK_LION_RESIZE 5
#define TABLET_AXES 5

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
#define NSEventTypeRotate 13
#define NSEventTypeMagnify 30
#endif

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)


/* This is the window corresponding to the key window */
static GdkWindow   *current_keyboard_window;


static void append_event                        (GdkEvent  *event,
                                                 gboolean   windowing);

static GdkWindow *find_toplevel_under_pointer   (GdkDisplay *display,
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
  GdkEvent new_event;

  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (_gdk_screen);
  new_event.setting.send_event = FALSE;
  new_event.setting.action = GDK_SETTING_ACTION_CHANGED;
  new_event.setting.name = NULL;

  /* Translate name */
  if (CFStringCompare (name,
                       CFSTR("AppleNoRedisplayAppearancePreferenceChanged"),
                       0) == kCFCompareEqualTo)
    new_event.setting.name = "gtk-primary-button-warps-slider";

  if (!new_event.setting.name)
    return;

  gdk_event_put (&new_event);
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
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);
  gdk_seat_ungrab (seat);
}

static void
fixup_event (GdkEvent *event)
{
  if (event->any.window)
    g_object_ref (event->any.window);
  if (((event->any.type == GDK_ENTER_NOTIFY) ||
       (event->any.type == GDK_LEAVE_NOTIFY)) &&
      (event->crossing.subwindow != NULL))
    g_object_ref (event->crossing.subwindow);
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

  if (nsflags & GDK_QUARTZ_ALPHA_SHIFT_KEY_MASK)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & GDK_QUARTZ_SHIFT_KEY_MASK)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & GDK_QUARTZ_CONTROL_KEY_MASK)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & GDK_QUARTZ_ALTERNATE_KEY_MASK)
    modifiers |= GDK_MOD1_MASK;
  if (nsflags & GDK_QUARTZ_COMMAND_KEY_MASK)
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
    case GDK_QUARTZ_LEFT_MOUSE_DOWN:
    case GDK_QUARTZ_RIGHT_MOUSE_DOWN:
    case GDK_QUARTZ_OTHER_MOUSE_DOWN:
      return GDK_BUTTON_PRESS_MASK;
    case GDK_QUARTZ_LEFT_MOUSE_UP:
    case GDK_QUARTZ_RIGHT_MOUSE_UP:
    case GDK_QUARTZ_OTHER_MOUSE_UP:
      return GDK_BUTTON_RELEASE_MASK;
    case GDK_QUARTZ_MOUSE_MOVED:
      return GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;
    case GDK_QUARTZ_SCROLL_WHEEL:
      /* Since applications that want button press events can get
       * scroll events on X11 (since scroll wheel events are really
       * button press events there), we need to use GDK_BUTTON_PRESS_MASK too.
       */
      return GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK;
    case GDK_QUARTZ_LEFT_MOUSE_DRAGGED:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | 
	      GDK_BUTTON1_MASK);
    case GDK_QUARTZ_RIGHT_MOUSE_DRAGGED:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK | 
	      GDK_BUTTON3_MASK);
    case GDK_QUARTZ_OTHER_MOUSE_DRAGGED:
      {
	GdkEventMask mask;

	mask = (GDK_POINTER_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK |
		GDK_BUTTON_MOTION_MASK);

	if (get_mouse_button_from_ns_event (nsevent) == 2)
	  mask |= (GDK_BUTTON2_MOTION_MASK | GDK_BUTTON2_MOTION_MASK | 
		   GDK_BUTTON2_MASK);

	return mask;
      }
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      return GDK_TOUCHPAD_GESTURE_MASK;
    case GDK_QUARTZ_KEY_DOWN:
    case GDK_QUARTZ_KEY_UP:
    case GDK_QUARTZ_FLAGS_CHANGED:
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

    case GDK_QUARTZ_MOUSE_ENTERED:
      return GDK_ENTER_NOTIFY_MASK;

    case GDK_QUARTZ_MOUSE_EXITED:
      return GDK_LEAVE_NOTIFY_MASK;

    default:
      g_assert_not_reached ();
    }

  return 0;
}

static void
get_window_point_from_screen_point (GdkWindow *window,
                                    NSPoint    screen_point,
                                    gint      *x,
                                    gint      *y)
{
  NSPoint point;
  GdkQuartzNSWindow *nswindow;

  nswindow = (GdkQuartzNSWindow*)gdk_quartz_window_get_nswindow (window);
  point = [nswindow convertPointFromScreen:screen_point];
  *x = point.x;
  *y = window->height - point.y;
}

static gboolean
is_mouse_button_press_event (NSEventType type)
{
  switch ((int)type)
    {
      case GDK_QUARTZ_LEFT_MOUSE_DOWN:
      case GDK_QUARTZ_RIGHT_MOUSE_DOWN:
      case GDK_QUARTZ_OTHER_MOUSE_DOWN:
        return TRUE;
    default:
      return FALSE;
    }

  return FALSE;
}

static GdkWindow *
get_toplevel_from_ns_event (NSEvent *nsevent,
                            NSPoint *screen_point,
                            gint    *x,
                            gint    *y)
{
  GdkWindow *toplevel = NULL;
  NSWindow* nswindow = [nsevent window];

  if (nswindow)
    {
      GdkQuartzView *view;
      NSPoint point, view_point;
      NSRect view_bounds;

      view = (GdkQuartzView *)[[nsevent window] contentView];

      toplevel = [view gdkWindow];

      point = [nsevent locationInWindow];
      view_point = [view convertPoint:point fromView:nil];
      view_bounds = [view bounds];

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
          (view_point.x < view_bounds.origin.x ||
           view_point.x >= view_bounds.origin.x + view_bounds.size.width ||
           view_point.y < view_bounds.origin.y ||
           view_point.y >= view_bounds.origin.y + view_bounds.size.height))
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

          /* Check if the event occurred on the titlebar. If it did,
           * explicitly return NULL to prevent going through the
           * fallback path, which could match the window that is
           * directly under the titlebar.
           */
          if (view_point.y > view_bounds.origin.y + view_bounds.size.height &&
              view_point.x >= view_bounds.origin.x &&
              view_point.x < view_bounds.origin.x + view_bounds.size.width)
            {
              NSRect window_frame = [view convertRect: [nswindow frame]
                                     fromView: nil];
              if (view_point.y <=
                  view_bounds.origin.y + window_frame.size.height)
                {
                  return NULL;
                }
            }
        }
      else
        {
	  *screen_point = [(GdkQuartzNSWindow*)nswindow convertPointToScreen:point];
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
create_focus_event (GdkWindow *window,
		    gboolean   in)
{
  GdkEvent *event;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkSeat *seat = gdk_display_get_default_seat (display);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
  gdk_event_set_seat (event, seat);

  return event;
}


static void
generate_motion_event (GdkWindow *window)
{
  NSPoint screen_point;
  GdkEvent *event;
  gint x, y, x_root, y_root;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkSeat *seat = gdk_display_get_default_seat (display);

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->any.window = NULL;
  event->any.send_event = TRUE;

  screen_point = [NSEvent mouseLocation];

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &x_root, &y_root);
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
  event->motion.is_hint = FALSE;
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_seat (event, seat);

  append_event (event, TRUE);
}

/* Note: Used to both set a new focus window and to unset the old one. */
void
_gdk_quartz_events_update_focus_window (GdkWindow *window,
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
_gdk_quartz_events_send_map_event (GdkWindow *window)
{
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window->impl);

  if (!impl->toplevel)
    return;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    {
      GdkEvent event;

      event.any.type = GDK_MAP;
      event.any.window = window;
  
      gdk_event_put (&event);
    }
}

static GdkWindow *
find_toplevel_under_pointer (GdkDisplay *display,
                             NSPoint     screen_point,
                             gint       *x,
                             gint       *y)
{
  GdkWindow *toplevel;
  GdkPointerWindowInfo *info;
  GdkSeat *seat = gdk_display_get_default_seat (display);

  info = _gdk_display_get_pointer_info (display, gdk_seat_get_pointer (seat));
  toplevel = info->toplevel_under_pointer;

  if (!(toplevel && WINDOW_IS_TOPLEVEL (toplevel)))
    {
      gint gdk_x = 0, gdk_y = 0;
      GdkDevice *pointer = gdk_seat_get_pointer(seat);
      _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &gdk_x, &gdk_y);
      toplevel = gdk_device_get_window_at_position (pointer, &gdk_x, &gdk_y);

      if (toplevel && ! WINDOW_IS_TOPLEVEL (toplevel))
        toplevel = gdk_window_get_toplevel (toplevel);

      if (toplevel)
        info->toplevel_under_pointer = g_object_ref (toplevel);
      else
        info->toplevel_under_pointer = NULL;

    }

  /* If the stored toplevel is NULL or _gdk_root it's not useful,
   * return NULL to regenerate.
   */
  if (toplevel == NULL || toplevel == _gdk_root )
    return NULL;

  get_window_point_from_screen_point (toplevel, screen_point, x, y);
  /* If the coordinates are out of window bounds, this toplevel is not
    * under the pointer and we thus return NULL. This can occur when
    * toplevel under pointer has not yet been updated due to a very recent
    * window resize. Alternatively, we should no longer be relying on
    * the toplevel_under_pointer value which is maintained in gdkwindow.c.
    */
  if (*x < 0 || *y < 0 || *x >= toplevel->width || *y >= toplevel->height)
    return NULL;

  return toplevel;
}

static GdkWindow *
find_toplevel_for_keyboard_event (NSEvent *nsevent)
{
  GdkQuartzView *view = (GdkQuartzView *)[[nsevent window] contentView];
  GdkWindow *window  = [view gdkWindow];
  GdkDisplay *display = gdk_window_get_display (window);
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkDevice *device = gdk_seat_get_keyboard (seat);
  GdkDeviceGrabInfo *grab = _gdk_display_get_last_device_grab (display, device);

  if (grab && grab->window && !grab->owner_events)
    window = gdk_window_get_effective_toplevel (grab->window);

  return window;
}

static GdkWindow *
find_toplevel_for_mouse_event (NSEvent    *nsevent,
                               gint       *x,
                               gint       *y)
{
  NSPoint screen_point;
  NSEventType event_type;
  GdkWindow *toplevel;
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;
  GdkSeat *seat;

  toplevel = get_toplevel_from_ns_event (nsevent, &screen_point, x, y);

  display = gdk_window_get_display (toplevel);
  seat = gdk_display_get_default_seat (_gdk_display);
  
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
                                            gdk_seat_get_pointer (seat));
  if (WINDOW_IS_TOPLEVEL (toplevel) && grab)
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
          GdkWindow *toplevel_under_pointer;
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
          GdkWindow *grab_toplevel;

          grab_toplevel = gdk_window_get_effective_toplevel (grab->window);
          get_window_point_from_screen_point (grab_toplevel, screen_point,
                                              x, y);

          return grab_toplevel;
        }

      return NULL;
    }
  else 
    {
      /* The non-grabbed case. */
      GdkWindow *toplevel_under_pointer;
      gint x_tmp, y_tmp;

      /* Ignore all events but mouse moved that might be on the title
       * bar (above the content view). The reason is that otherwise
       * gdk gets confused about getting e.g. button presses with no
       * window (the title bar is not known to it).
       */
      if (event_type != GDK_QUARTZ_MOUSE_MOVED)
        if (*y < 0)
          return NULL;

      /* As for owner events, we need to use the toplevel under the
       * pointer, not the window from the NSEvent.
       */
      toplevel_under_pointer = find_toplevel_under_pointer (display,
                                                            screen_point,
                                                            &x_tmp, &y_tmp);
      if (toplevel_under_pointer
          && WINDOW_IS_TOPLEVEL (toplevel_under_pointer))
        {
          toplevel = toplevel_under_pointer;
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
static GdkWindow *
find_window_for_ns_event (NSEvent *nsevent, 
                          gint    *x, 
                          gint    *y,
                          gint    *x_root,
                          gint    *y_root)
{
  GdkQuartzView *view;
  GdkWindow *toplevel;
  NSPoint screen_point;
  NSEventType event_type;

  view = (GdkQuartzView *)[[nsevent window] contentView];

  toplevel = get_toplevel_from_ns_event (nsevent, &screen_point, x, y);
  if (!toplevel)
    return NULL;
  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, x_root, y_root);

  event_type = [nsevent type];

  switch (event_type)
    {
    case GDK_QUARTZ_LEFT_MOUSE_DOWN:
    case GDK_QUARTZ_RIGHT_MOUSE_DOWN:
    case GDK_QUARTZ_OTHER_MOUSE_DOWN:
    case GDK_QUARTZ_LEFT_MOUSE_UP:
    case GDK_QUARTZ_RIGHT_MOUSE_UP:
    case GDK_QUARTZ_OTHER_MOUSE_UP:
    case GDK_QUARTZ_MOUSE_MOVED:
    case GDK_QUARTZ_SCROLL_WHEEL:
    case GDK_QUARTZ_LEFT_MOUSE_DRAGGED:
    case GDK_QUARTZ_RIGHT_MOUSE_DRAGGED:
    case GDK_QUARTZ_OTHER_MOUSE_DRAGGED:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      return find_toplevel_for_mouse_event (nsevent, x, y);

    case GDK_QUARTZ_MOUSE_ENTERED:
    case GDK_QUARTZ_MOUSE_EXITED:
      /* Only handle our own entered/exited events, not the ones for the
       * titlebar buttons.
       */
      if ([view trackingRect] == nsevent.trackingNumber)
          return toplevel;

      /* MacOS 13 isn't sending the trackingArea events so we have to
       * rely on the cursorRect events that we discarded in earlier
       * macOS versions. These trigger 4 pixels out from the window's
       * frame so we obtain that rect and adjust it for hit testing.
       */
      if (!nsevent.trackingArea && gdk_quartz_osx_version() >= GDK_OSX_VENTURA)
        {
          static const int border_width = 4;
          NSRect frame = nsevent.window.frame;
          gboolean inside, at_edge;

          frame.origin.x -= border_width;
          frame.origin.y -= border_width;
          frame.size.width += 2 * border_width;
          frame.size.height += 2 * border_width;
          inside =
               screen_point.x >= frame.origin.x &&
               screen_point.x <= frame.origin.x + frame.size.width &&
               screen_point.y >= frame.origin.y &&
               screen_point.y <= frame.origin.y + frame.size.height;
          at_edge =
               screen_point.x >= frame.origin.x - 1 &&
               screen_point.x <= frame.origin.x + frame.size.width + 1 &&
               screen_point.y >= frame.origin.y - 1 &&
               screen_point.y <= frame.origin.y + frame.size.height + 1;

          if ((event_type == GDK_QUARTZ_MOUSE_ENTERED && inside) ||
              at_edge)
            return toplevel;
          else
            return NULL;
        }

      return NULL;

    case GDK_QUARTZ_KEY_DOWN:
    case GDK_QUARTZ_KEY_UP:
    case GDK_QUARTZ_FLAGS_CHANGED:
      return find_toplevel_for_keyboard_event (nsevent);

    default:
      /* Ignore everything else. */
      break;
    }

  return NULL;
}

static void
fill_crossing_event (GdkWindow       *toplevel,
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
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

  event->any.type = event_type;
  event->crossing.window = toplevel;
  event->crossing.subwindow = NULL;
  event->crossing.time = get_time_from_ns_event (nsevent);
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = x_root;
  event->crossing.y_root = y_root;
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                         _gdk_quartz_events_get_current_mouse_modifiers ();

  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_seat (event, seat);

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
fill_pinch_event (GdkWindow *window,
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
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

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
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));

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
fill_button_event (GdkWindow *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkEventType type;
  GdkDevice *event_device = NULL;
  gdouble *axes = NULL;
  gint state;
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_quartz_events_get_current_mouse_modifiers ();

  switch ((int)[nsevent type])
    {
    case GDK_QUARTZ_LEFT_MOUSE_DOWN:
    case GDK_QUARTZ_RIGHT_MOUSE_DOWN:
    case GDK_QUARTZ_OTHER_MOUSE_DOWN:
      type = GDK_BUTTON_PRESS;
      state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case GDK_QUARTZ_LEFT_MOUSE_UP:
    case GDK_QUARTZ_RIGHT_MOUSE_UP:
    case GDK_QUARTZ_OTHER_MOUSE_UP:
      type = GDK_BUTTON_RELEASE;
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    default:
      g_assert_not_reached ();
    }

  event_device = _gdk_quartz_device_manager_core_device_for_ns_event (gdk_display_get_device_manager (_gdk_display),
                                                                      nsevent);

  if ([nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_TABLET_POINT)
    {
      axes = g_new (gdouble, TABLET_AXES);

      axes[0] = x;
      axes[1] = y;
      axes[2] = [nsevent pressure];
      axes[3] = [nsevent tilt].x;
      axes[4] = -[nsevent tilt].y;
    }

  event->any.type = type;
  event->button.window = window;
  event->button.time = get_time_from_ns_event (nsevent);
  event->button.x = x;
  event->button.y = y;
  event->button.x_root = x_root;
  event->button.y_root = y_root;
  event->button.axes = axes;
  event->button.state = state;
  event->button.button = get_mouse_button_from_ns_event (nsevent);

  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_source_device (event, event_device);
  gdk_event_set_seat (event, seat);
}

static void
fill_motion_event (GdkWindow *window,
                   GdkEvent  *event,
                   NSEvent   *nsevent,
                   gint       x,
                   gint       y,
                   gint       x_root,
                   gint       y_root)
{
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);
  GdkDevice *event_device = NULL;
  gdouble *axes = NULL;

  event_device = _gdk_quartz_device_manager_core_device_for_ns_event (gdk_display_get_device_manager (_gdk_display),
                                                                      nsevent);

  if ([nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_TABLET_POINT)
    {
      axes = g_new (gdouble, TABLET_AXES);

      axes[0] = x;
      axes[1] = y;
      axes[2] = [nsevent pressure];
      axes[3] = [nsevent tilt].x;
      axes[4] = -[nsevent tilt].y;
    }

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event (nsevent);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  event->motion.axes = axes;
  event->motion.state = get_keyboard_modifiers_from_ns_event (nsevent) |
                        _gdk_quartz_events_get_current_mouse_modifiers ();
  event->motion.is_hint = FALSE;
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_source_device (event, event_device);

  gdk_event_set_seat (event, seat);
}

static void
fill_scroll_event (GdkWindow          *window,
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
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

  event->any.type = GDK_SCROLL;
  event->scroll.window = window;
  event->scroll.time = get_time_from_ns_event (nsevent);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.x_root = x_root;
  event->scroll.y_root = y_root;
  event->scroll.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->scroll.direction = direction;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_seat (event, seat);
}

static void
fill_key_event (GdkWindow    *window,
                GdkEvent     *event,
                NSEvent      *nsevent,
                GdkEventType  type)
{
  GdkEventPrivate *priv;
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);
  gchar buf[7];
  gunichar c = 0;

  priv = (GdkEventPrivate *) event;
  priv->windowing_data = [nsevent retain];

  event->any.type = type;
  event->key.window = window;
  event->key.time = get_time_from_ns_event (nsevent);
  event->key.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->key.hardware_keycode = [nsevent keyCode];
  gdk_event_set_scancode (event, [nsevent keyCode]);
  event->key.group = ([nsevent modifierFlags] & GDK_QUARTZ_ALTERNATE_KEY_MASK) ? 1 : 0;
  event->key.keyval = GDK_KEY_VoidSymbol;

  gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
  gdk_event_set_seat (event, seat);

  gdk_keymap_translate_keyboard_state (gdk_keymap_get_for_display (_gdk_display),
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
  gdk_keymap_add_virtual_modifiers (gdk_keymap_get_for_display (_gdk_display),
                                    &event->key.state);

  event->key.string = NULL;

  /* Fill in ->string since apps depend on it, taken from the x11 backend. */
  if (event->key.keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';
      
      event->key.string = g_locale_from_utf8 (buf, len,
					      NULL, &bytes_written,
					      NULL);
      if (event->key.string)
	event->key.length = bytes_written;
    }
  else if (event->key.keyval == GDK_KEY_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_KEY_Return ||
	  event->key.keyval == GDK_KEY_KP_Enter)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\r");
    }

  if (!event->key.string)
    {
      event->key.length = 0;
      event->key.string = g_strdup ("");
    }

  GDK_NOTE(EVENTS,
    g_message ("key %s:\t\twindow: %p  key: %12s  %d",
	  type == GDK_KEY_PRESS ? "press" : "release",
	  event->key.window,
	  event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
	  event->key.keyval));
}

static gboolean
synthesize_crossing_event (GdkWindow *window,
                           GdkEvent  *event,
                           NSEvent   *nsevent,
                           gint       x,
                           gint       y,
                           gint       x_root,
                           gint       y_root)
{
  switch ([nsevent type])
    {
    case GDK_QUARTZ_MOUSE_ENTERED:
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

    case GDK_QUARTZ_MOUSE_EXITED:
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
_gdk_quartz_synthesize_null_key_event (GdkWindow *window)
{
  GdkEvent *event;
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

  event = gdk_event_new (GDK_KEY_PRESS);
  event->any.type = GDK_KEY_PRESS;
  event->key.window = window;
  event->key.state = 0;
  event->key.hardware_keycode = 0;
  event->key.group = 0;
  event->key.keyval = GDK_KEY_VoidSymbol;

  gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
  gdk_event_set_seat (event, seat);
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
  NSUInteger buttons = 0;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
  if (gdk_quartz_osx_version () >= GDK_OSX_SNOW_LEOPARD)
    buttons = [NSClassFromString(@"NSEvent") pressedMouseButtons];
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
  else
#endif
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    buttons = GetCurrentButtonState ();
#endif
  return get_mouse_button_modifiers_from_ns_buttons (buttons);
}

/* Detect window resizing */

static gboolean
test_resize (NSEvent *event, GdkWindow *toplevel, gint x, gint y)
{
  GdkWindowImplQuartz *toplevel_impl;
  gboolean lion;

  /* Resizing from the resize indicator only begins if an GDK_QUARTZ_LEFT_MOUSE_BUTTON
   * event is received in the resizing area.
   */
  toplevel_impl = GDK_WINDOW_IMPL_QUARTZ (toplevel->impl);
  if ([toplevel_impl->toplevel showsResizeIndicator])
  if ([event type] == GDK_QUARTZ_LEFT_MOUSE_DOWN &&
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
   * do receive, for instance, a right mouse down event for a GDK window
   * for x-coordinate range [-3, 0], but we do not want to forward this
   * into GDK. Forwarding such events into GDK will confuse the pointer
   * window finding code, because there are no GdkWindows present in
   * the range [-3, 0].
   */
  lion = gdk_quartz_osx_version () >= GDK_OSX_LION;
  if (lion &&
      ([event type] == GDK_QUARTZ_LEFT_MOUSE_DOWN ||
       [event type] == GDK_QUARTZ_RIGHT_MOUSE_DOWN ||
       [event type] == GDK_QUARTZ_OTHER_MOUSE_DOWN))
    {
      if (x < GDK_LION_RESIZE ||
          x > toplevel->width - GDK_LION_RESIZE ||
          y > toplevel->height - GDK_LION_RESIZE)
        return TRUE;
    }

  return FALSE;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101200
#define GDK_QUARTZ_APP_KIT_DEFINED NSAppKitDefined
#define GDK_QUARTZ_APPLICATION_DEACTIVATED NSApplicationDeactivatedEventType
#else
#define GDK_QUARTZ_APP_KIT_DEFINED NSEventTypeAppKitDefined
#define GDK_QUARTZ_APPLICATION_DEACTIVATED NSEventSubtypeApplicationDeactivated
#endif

static gboolean
gdk_event_translate (GdkEvent *event,
                     NSEvent  *nsevent)
{
  NSEventType event_type;
  NSWindow *nswindow;
  GdkWindow *window;
  int x, y;
  int x_root, y_root;
  gboolean return_val;

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  event_type = [nsevent type];
  if (event_type == GDK_QUARTZ_APP_KIT_DEFINED)
    {
      if ([nsevent subtype] ==  GDK_QUARTZ_APPLICATION_DEACTIVATED)
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

  /* We need to register the proximity event from any point on the screen
   * to properly register the devices
   */
  if (event_type == GDK_QUARTZ_EVENT_TABLET_PROXIMITY)
    {
      _gdk_quartz_device_manager_register_device_for_ns_event (gdk_display_get_device_manager (_gdk_display),
                                                               nsevent);
    }

  nswindow = [nsevent window];

  /* Ignore events for windows not created by GDK. */
  if (nswindow && ![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
    return FALSE;

  /* Ignore events for ones with no windows */
  if (!nswindow)
    {
      GdkWindow *toplevel = NULL;

      if (event_type == GDK_QUARTZ_MOUSE_MOVED)
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

  /* Find the right GDK window to send the event to, taking grabs and
   * event masks into consideration.
   */
  window = find_window_for_ns_event (nsevent, &x, &y, &x_root, &y_root);
  if (!window)
    return FALSE;

  /* Quartz handles resizing on its own, so we want to stay out of the way. */
  if (test_resize (nsevent, window, x, y))
    return FALSE;

  /* Apply any window filters. */
  if (GDK_IS_WINDOW (window))
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
  if ((event_type == GDK_QUARTZ_RIGHT_MOUSE_DOWN ||
       event_type == GDK_QUARTZ_OTHER_MOUSE_DOWN ||
       event_type == GDK_QUARTZ_LEFT_MOUSE_DOWN))
    {
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window->impl);

      if (![NSApp isActive])
        {
          [NSApp activateIgnoringOtherApps:YES];
          return FALSE;
        }
      else if (![impl->toplevel isKeyWindow])
        {
          GdkDeviceGrabInfo *grab;
          GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);

          grab = _gdk_display_get_last_device_grab (_gdk_display,
                                                    gdk_seat_get_pointer (seat));
          if (!grab)
            [impl->toplevel makeKeyWindow];

        }
    }

  return_val = TRUE;

  switch (event_type)
    {
    case GDK_QUARTZ_LEFT_MOUSE_DOWN:
    case GDK_QUARTZ_RIGHT_MOUSE_DOWN:
    case GDK_QUARTZ_OTHER_MOUSE_DOWN:
    case GDK_QUARTZ_LEFT_MOUSE_UP:
    case GDK_QUARTZ_RIGHT_MOUSE_UP:
    case GDK_QUARTZ_OTHER_MOUSE_UP:
      fill_button_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case GDK_QUARTZ_LEFT_MOUSE_DRAGGED:
    case GDK_QUARTZ_RIGHT_MOUSE_DRAGGED:
    case GDK_QUARTZ_OTHER_MOUSE_DRAGGED:
    case GDK_QUARTZ_MOUSE_MOVED:
      fill_motion_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case GDK_QUARTZ_SCROLL_WHEEL:
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
    case GDK_QUARTZ_MOUSE_EXITED:
      if (WINDOW_IS_TOPLEVEL (window))
          [[NSCursor arrowCursor] set];
      /* fall through */
    case GDK_QUARTZ_MOUSE_ENTERED:
      return_val = synthesize_crossing_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case GDK_QUARTZ_KEY_DOWN:
    case GDK_QUARTZ_KEY_UP:
    case GDK_QUARTZ_FLAGS_CHANGED:
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
      if (event->any.window)
	g_object_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	g_object_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
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

      event->any.window = NULL;
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
	  gdk_event_free (event);

          gdk_threads_leave ();
          [NSApp sendEvent:nsevent];
          gdk_threads_enter ();
        }

      _gdk_quartz_event_loop_release_event (nsevent);
    }
}

void
_gdk_quartz_screen_broadcast_client_message (GdkScreen *screen,
                                             GdkEvent  *event)
{
  /* Not supported. */
}

gboolean
_gdk_quartz_screen_get_setting (GdkScreen   *screen,
                                const gchar *name,
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
