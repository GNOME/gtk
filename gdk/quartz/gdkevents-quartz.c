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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "gdkquartzdisplay.h"
#include "gdkprivate-quartz.h"
#include "gdkquartzdevicemanager-core.h"

#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

/* This is the window corresponding to the key window */
static GdkWindow   *current_keyboard_window;

/* This is the event mask and button state from the last event */
static GdkEventMask current_event_mask;
static int          current_button_state;

static void append_event                        (GdkEvent  *event,
                                                 gboolean   windowing);

void
_gdk_quartz_events_init (void)
{
  _gdk_quartz_event_loop_init ();

  current_keyboard_window = g_object_ref (_gdk_root);
}

gboolean
_gdk_quartz_display_has_pending (GdkDisplay *display)
{
  return (_gdk_event_queue_find_first (display) ||
         (_gdk_quartz_event_loop_check_pending ()));
}

static void
break_all_grabs (guint32 time)
{
  GList *list, *l;
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (_gdk_display);
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_MASTER);
  for (l = list; l; l = l->next)
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

  g_list_free (list);
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
  
  return time * 1000.0;
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
get_keyboard_modifiers_from_ns_event (NSEvent *nsevent)
{
  GdkModifierType modifiers = 0;
  int nsflags;

  nsflags = [nsevent modifierFlags];
  
  if (nsflags & NSAlphaShiftKeyMask)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & NSShiftKeyMask)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & NSControlKeyMask)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & NSCommandKeyMask)
    modifiers |= GDK_MOD1_MASK;

  return modifiers;
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
      return GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;
    case NSScrollWheel:
      /* Since applications that want button press events can get
       * scroll events on X11 (since scroll wheel events are really
       * button press events there), we need to use GDK_BUTTON_PRESS_MASK too.
       */
      return GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseDragged:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | 
	      GDK_BUTTON1_MASK);
    case NSRightMouseDragged:
      return (GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
	      GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK | 
	      GDK_BUTTON3_MASK);
    case NSOtherMouseDragged:
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

static GdkEvent *
create_focus_event (GdkWindow *window,
		    gboolean   in)
{
  GdkEvent *event;
  GdkQuartzDeviceManagerCore *device_manager;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_display->device_manager);
  gdk_event_set_device (event, device_manager->core_keyboard);

  return event;
}


static void
generate_motion_event (GdkWindow *window)
{
  NSPoint point;
  NSPoint screen_point;
  NSWindow *nswindow;
  GdkQuartzView *view;
  GdkEvent *event;
  gint x, y, x_root, y_root;
  GdkDisplay *display;

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->any.window = NULL;
  event->any.send_event = TRUE;

  nswindow = ((GdkWindowImplQuartz *)window->impl)->toplevel;
  view = (GdkQuartzView *)[nswindow contentView];

  display = gdk_window_get_display (window);

  screen_point = [NSEvent mouseLocation];

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &x_root, &y_root);

  point = [nswindow convertScreenToBase:screen_point];

  x = point.x;
  y = window->height - point.y;

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = GDK_CURRENT_TIME;
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = 0;
  event->motion.is_hint = FALSE;
  event->motion.device = _gdk_display->core_pointer;

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
_gdk_quartz_events_send_enter_notify_event (GdkWindow *window)
{
  NSPoint point;
  NSPoint screen_point;
  NSWindow *nswindow;
  GdkEvent *event;
  gint x, y, x_root, y_root;

  event = gdk_event_new (GDK_ENTER_NOTIFY);
  event->any.window = NULL;
  event->any.send_event = FALSE;

  nswindow = ((GdkWindowImplQuartz *)window->impl)->toplevel;

  screen_point = [NSEvent mouseLocation];

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, &x_root, &y_root);

  point = [nswindow convertScreenToBase:screen_point];

  x = point.x;
  y = window->height - point.y;

  event->crossing.window = window;
  event->crossing.subwindow = NULL;
  event->crossing.time = GDK_CURRENT_TIME;
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = x_root;
  event->crossing.y_root = y_root;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.state = 0;

  gdk_event_set_device (event, _gdk_display->core_pointer);

  append_event (event, TRUE);
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

  info = _gdk_display_get_pointer_info (display, display->core_pointer);
  toplevel = info->toplevel_under_pointer;
  if (toplevel && WINDOW_IS_TOPLEVEL (toplevel))
    {
      NSWindow *nswindow;
      NSPoint point;

      nswindow = ((GdkWindowImplQuartz *)toplevel->impl)->toplevel;

      point = [nswindow convertScreenToBase:screen_point];

      *x = point.x;
      *y = toplevel->height - point.y;
    }

  return toplevel;
}

static GdkWindow *
find_toplevel_for_keyboard_event (NSEvent *nsevent)
{
  GList *list, *l;
  GdkWindow *window;
  GdkDisplay *display;
  GdkQuartzView *view;
  GdkDeviceManager *device_manager;

  view = (GdkQuartzView *)[[nsevent window] contentView];
  window = [view gdkWindow];

  display = gdk_window_get_display (window);

  device_manager = gdk_display_get_device_manager (display);
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_MASTER);
  for (l = list; l; l = l->next)
    {
      GdkDeviceGrabInfo *grab;
      GdkDevice *device = l->data;

      if (gdk_device_get_source(device) != GDK_SOURCE_KEYBOARD)
        continue;

      grab = _gdk_display_get_last_device_grab (display, device);
      if (grab && grab->window && !grab->owner_events)
        {
          window = gdk_window_get_effective_toplevel (grab->window);
          break;
        }
    }

  g_list_free (list);

  return window;
}

static GdkWindow *
find_toplevel_for_mouse_event (NSEvent    *nsevent,
                               gint       *x,
                               gint       *y)
{
  NSPoint point;
  NSPoint screen_point;
  NSEventType event_type;
  GdkWindow *toplevel;
  GdkQuartzView *view;
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;

  view = (GdkQuartzView *)[[nsevent window] contentView];
  toplevel = [view gdkWindow];

  display = gdk_window_get_display (toplevel);

  event_type = [nsevent type];
  point = [nsevent locationInWindow];
  screen_point = [[nsevent window] convertBaseToScreen:point];

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
                                            display->core_pointer);
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
          NSWindow *grab_nswindow;

          grab_toplevel = gdk_window_get_effective_toplevel (grab->window);

          grab_nswindow = ((GdkWindowImplQuartz *)grab_toplevel->impl)->toplevel;
          point = [grab_nswindow convertScreenToBase:screen_point];

          /* Note: x_root and y_root are already right. */
          *x = point.x;
          *y = grab_toplevel->height - point.y;

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
          && WINDOW_IS_TOPLEVEL (toplevel_under_pointer))
        {
          GdkWindowImplQuartz *toplevel_impl;

          toplevel = toplevel_under_pointer;

          toplevel_impl = (GdkWindowImplQuartz *)toplevel->impl;

          if ([toplevel_impl->toplevel showsResizeIndicator])
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
              if (x_tmp > frame.size.width - GRIP_WIDTH
                  && x_tmp < frame.size.width
                  && y_tmp > frame.size.height - GRIP_HEIGHT
                  && y_tmp < frame.size.height)
                {
                  return NULL;
                }
            }

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
  NSPoint point;
  NSPoint screen_point;
  NSEventType event_type;
  GdkWindow *toplevel;

  view = (GdkQuartzView *)[[nsevent window] contentView];
  toplevel = [view gdkWindow];

  point = [nsevent locationInWindow];
  screen_point = [[nsevent window] convertBaseToScreen:point];

  *x = point.x;
  *y = toplevel->height - point.y;

  _gdk_quartz_window_nspoint_to_gdk_xy (screen_point, x_root, y_root);

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
  event->crossing.state = get_keyboard_modifiers_from_ns_event (nsevent);

  gdk_event_set_device (event, _gdk_display->core_pointer);

  /* FIXME: Focus and button state? */
}

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
  gint state;
  gint button;

  state = get_keyboard_modifiers_from_ns_event (nsevent);

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      type = GDK_BUTTON_PRESS;
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
  
  button = get_mouse_button_from_ns_event (nsevent);

  event->any.type = type;
  event->button.window = window;
  event->button.time = get_time_from_ns_event (nsevent);
  event->button.x = x;
  event->button.y = y;
  event->button.x_root = x_root;
  event->button.y_root = y_root;
  /* FIXME event->axes */
  event->button.state = state;
  event->button.button = button;
  event->button.device = _gdk_display->core_pointer;
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
  GdkModifierType state;

  state = get_keyboard_modifiers_from_ns_event (nsevent);

  switch ([nsevent type])
    {
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSMouseMoved:
      break;
    }

  event->any.type = GDK_MOTION_NOTIFY;
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event (nsevent);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x_root;
  event->motion.y_root = y_root;
  /* FIXME event->axes */
  event->motion.state = state;
  event->motion.is_hint = FALSE;
  event->motion.device = _gdk_display->core_pointer;
}

static void
fill_scroll_event (GdkWindow          *window,
                   GdkEvent           *event,
                   NSEvent            *nsevent,
                   gint                x,
                   gint                y,
                   gint                x_root,
                   gint                y_root,
                   GdkScrollDirection  direction)
{
  NSPoint point;

  point = [nsevent locationInWindow];

  event->any.type = GDK_SCROLL;
  event->scroll.window = window;
  event->scroll.time = get_time_from_ns_event (nsevent);
  event->scroll.x = x;
  event->scroll.y = y;
  event->scroll.x_root = x_root;
  event->scroll.y_root = y_root;
  event->scroll.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->scroll.direction = direction;
  event->scroll.device = _gdk_display->core_pointer;
}

static void
fill_key_event (GdkWindow    *window,
                GdkEvent     *event,
                NSEvent      *nsevent,
                GdkEventType  type)
{
  GdkEventPrivate *priv;
  GdkQuartzDeviceManagerCore *device_manager;
  gchar buf[7];
  gunichar c = 0;

  priv = (GdkEventPrivate *) event;
  priv->windowing_data = [nsevent retain];

  event->any.type = type;
  event->key.window = window;
  event->key.time = get_time_from_ns_event (nsevent);
  event->key.state = get_keyboard_modifiers_from_ns_event (nsevent);
  event->key.hardware_keycode = [nsevent keyCode];
  event->key.group = ([nsevent modifierFlags] & NSAlternateKeyMask) ? 1 : 0;
  event->key.keyval = GDK_KEY_VoidSymbol;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (_gdk_display->device_manager);
  gdk_event_set_device (event, device_manager->core_keyboard);
  
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
          mask = GDK_MOD1_MASK;
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
          mask = GDK_MOD5_MASK;
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

  event->key.state |= current_button_state;

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
    case NSMouseEntered:
      /* Enter events are considered always to be from the root window as we
       * can't know for sure from what window we enter.
       */
      if (!(window->event_mask & GDK_ENTER_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_ENTER_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_ANCESTOR);
      return TRUE;

    case NSMouseExited:
      /* Exited always is to the root window as far as we are concerned,
       * since there is no way to reliably get information about what new
       * window is entered when exiting one.
       */
      if (!(window->event_mask & GDK_LEAVE_NOTIFY_MASK))
        return FALSE;

      fill_crossing_event (window, event, nsevent,
                           x, y,
                           x_root, y_root,
                           GDK_LEAVE_NOTIFY,
                           GDK_CROSSING_NORMAL,
                           GDK_NOTIFY_ANCESTOR);
      return TRUE;

    default:
      break;
    }

  return FALSE;
}

GdkEventMask 
_gdk_quartz_events_get_current_event_mask (void)
{
  return current_event_mask;
}

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
  if (event_type == NSAppKitDefined)
    {
      if ([nsevent subtype] == NSApplicationDeactivatedEventType)
        break_all_grabs (get_time_from_ns_event (nsevent));

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */

      /* Leave all AppKit events to AppKit. */
      return FALSE;
    }

  /* Keep track of button state, since we don't get that information
   * for key events. 
   */
  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      current_button_state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      current_button_state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;
    default:
      break;
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

  /* Ignore events for no window or ones not created by GDK. */
  if (!nswindow || ![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
    return FALSE;

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([(GdkQuartzNSWindow *)nswindow isInMove])
    {
      break_all_grabs (get_time_from_ns_event (nsevent));
      return FALSE;
    }

  /* Find the right GDK window to send the event to, taking grabs and
   * event masks into consideration.
   */
  window = find_window_for_ns_event (nsevent, &x, &y, &x_root, &y_root);
  if (!window)
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
  if ((event_type == NSRightMouseDown ||
       event_type == NSOtherMouseDown ||
       event_type == NSLeftMouseDown))
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

          grab = _gdk_display_get_last_device_grab (_gdk_display,
                                                    _gdk_display->core_pointer);
          if (!grab)
            [impl->toplevel makeKeyWindow];
        }
    }

  current_event_mask = get_event_mask_from_ns_event (nsevent);

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
	float dx = [nsevent deltaX];
	float dy = [nsevent deltaY];
	GdkScrollDirection direction;

        if (dy != 0)
          {
            if (dy < 0.0)
              direction = GDK_SCROLL_DOWN;
            else
              direction = GDK_SCROLL_UP;

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root, direction);
          }

        if (dx != 0)
          {
            if (dx < 0.0)
              direction = GDK_SCROLL_RIGHT;
            else
              direction = GDK_SCROLL_LEFT;

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root, direction);
          }
      }
      break;

    case NSMouseEntered:
    case NSMouseExited:
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

          GDK_THREADS_LEAVE ();
          [NSApp sendEvent:nsevent];
          GDK_THREADS_ENTER ();
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

      GDK_QUARTZ_ALLOC_POOL;

      name = [[NSFont systemFontOfSize:0] familyName];

      /* Let's try to use the "views" font size (12pt) by default. This is
       * used for lists/text/other "content" which is the largest parts of
       * apps, using the "regular control" size (13pt) looks a bit out of
       * place. We might have to tweak this.
       */

      /* The size has to be hardcoded as there doesn't seem to be a way to
       * get the views font size programmatically.
       */
      str = g_strdup_printf ("%s 12", [name UTF8String]);
      g_value_set_string (value, str);
      g_free (str);

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
