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

#include "gdkscreen.h"
#include "gdkkeysyms.h"
#include "gdkprivate-quartz.h"

/* This is the window the mouse is currently over */
static GdkWindow   *current_mouse_window;

/* This is the window corresponding to the key window */
static GdkWindow   *current_keyboard_window;

/* This is the keyboard grab window */
GdkWindow *         _gdk_quartz_keyboard_grab_window;
static gboolean     keyboard_grab_owner_events;

/* This is the event mask and button state from the last event */
static GdkEventMask current_event_mask;
static int          current_button_state;

static void get_child_coordinates_from_ancestor (GdkWindow *ancestor_window,
                                                 gint       ancestor_x,
                                                 gint       ancestor_y,
                                                 GdkWindow *child_window, 
                                                 gint      *child_x, 
                                                 gint      *child_y);
static void get_ancestor_coordinates_from_child (GdkWindow *child_window,
                                                 gint       child_x,
                                                 gint       child_y,
                                                 GdkWindow *ancestor_window, 
                                                 gint      *ancestor_x, 
                                                 gint      *ancestor_y);
static void get_converted_window_coordinates    (GdkWindow *in_window,
                                                 gint       in_x,
                                                 gint       in_y,
                                                 GdkWindow *out_window, 
                                                 gint      *out_x, 
                                                 gint      *out_y);
static void append_event                        (GdkEvent  *event);

static const gchar *
which_window_is_this (GdkWindow *window)
{
  static gchar buf[256];
  const gchar *name = NULL;
  gpointer widget;

  /* Get rid of compiler warning. */
  if (0) which_window_is_this (window);

  if (window == _gdk_root)
    name = "root";
  else if (window == NULL)
    name = "null";

  if (window)
    {
      gdk_window_get_user_data (window, &widget);
      if (widget)
        name = G_OBJECT_TYPE_NAME (widget);
    }

  if (!name)
    name = "unknown";

  snprintf (buf, 256, "<%s (%p)%s>", 
            name, window, 
            window == current_mouse_window ? ", is mouse" : "");

  return buf;
}

NSEvent *
gdk_quartz_event_get_nsevent (GdkEvent *event)
{
  /* FIXME: If the event here is unallocated, we crash. */
  return ((GdkEventPrivate *) event)->windowing_data;
}

void
_gdk_events_init (void)
{
  _gdk_quartz_event_loop_init ();

  current_mouse_window = g_object_ref (_gdk_root);
  current_keyboard_window = g_object_ref (_gdk_root);
}

gboolean
gdk_events_pending (void)
{
  return (_gdk_event_queue_find_first (_gdk_display) ||
	  (_gdk_quartz_event_loop_check_pending ()));
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  /* FIXME: Implement */
  return NULL;
}

static void
generate_grab_broken_event (GdkWindow *window,
			    gboolean   keyboard,
			    GdkWindow *grab_window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkEvent *event = gdk_event_new (GDK_GRAB_BROKEN);

      event->grab_broken.window = window;
      event->grab_broken.send_event = 0;
      event->grab_broken.keyboard = keyboard;
      event->grab_broken.implicit = FALSE;
      event->grab_broken.grab_window = grab_window;
      
      append_event (event);
    }
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow  *window,
		   gint        owner_events,
		   guint32     time)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (_gdk_quartz_keyboard_grab_window)
    {
      if (_gdk_quartz_keyboard_grab_window != window)
	generate_grab_broken_event (_gdk_quartz_keyboard_grab_window,
				    TRUE, window);
      
      g_object_unref (_gdk_quartz_keyboard_grab_window);
    }

  _gdk_quartz_keyboard_grab_window = g_object_ref (window);
  keyboard_grab_owner_events = owner_events;

  return GDK_GRAB_SUCCESS;
}

void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  if (_gdk_quartz_keyboard_grab_window)
    g_object_unref (_gdk_quartz_keyboard_grab_window);
  _gdk_quartz_keyboard_grab_window = NULL;
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time)
{
  _gdk_display_unset_has_pointer_grab (display, FALSE, FALSE, time);
}

GdkGrabStatus
gdk_pointer_grab (GdkWindow    *window,
		  gboolean	owner_events,
		  GdkEventMask	event_mask,
		  GdkWindow    *confine_to,
		  GdkCursor    *cursor,
		  guint32	time)
{
  GdkWindow *toplevel;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  toplevel = gdk_window_get_toplevel (window);

  _gdk_display_set_has_pointer_grab (_gdk_display,
                                     window,
                                     toplevel,
                                     owner_events,
                                     event_mask,
                                     0,
                                     time,
                                     FALSE);

  return GDK_GRAB_SUCCESS;
}

static void
break_all_grabs (guint32 time)
{
  /*
  if (_gdk_quartz_keyboard_grab_window)
    {
      generate_grab_broken_event (_gdk_quartz_keyboard_grab_window,
                                  TRUE,
                                  NULL);
      g_object_unref (_gdk_quartz_keyboard_grab_window);
      _gdk_quartz_keyboard_grab_window = NULL;
    }
  */
  if (_gdk_display->pointer_grab.window)
    {
      g_print ("break all grabs\n");
      _gdk_display_unset_has_pointer_grab (_gdk_display,
                                           _gdk_display->pointer_grab.implicit,
                                           FALSE,
                                           time);
    }
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
append_event (GdkEvent *event)
{
  fixup_event (event);
  _gdk_event_queue_append (_gdk_display, event);
}

static gint
gdk_event_apply_filters (NSEvent *nsevent,
			 GdkEvent *event,
			 GList *filters)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;

  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      
      tmp_list = tmp_list->next;
      result = filter->function (nsevent, event, filter->data);
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
  int button;

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

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = window;
  event->focus_change.in = in;

  return event;
}

/* Note: Used to both set a new focus window and to unset the old one. */
void
_gdk_quartz_events_update_focus_window (GdkWindow *window,
					gboolean   got_focus)
{
  GdkEvent *event;

  if (got_focus && window == current_keyboard_window)
    return;

  /* FIXME: Don't do this when grabbed? Or make GdkQuartzWindow
   * disallow it in the first place instead?
   */
  
  if (!got_focus && window == current_keyboard_window)
    {
      event = create_focus_event (current_keyboard_window, FALSE);
      append_event (event);
      g_object_unref (current_keyboard_window);
      current_keyboard_window = NULL;
    }

  if (got_focus)
    {
      if (current_keyboard_window)
	{
	  event = create_focus_event (current_keyboard_window, FALSE);
	  append_event (event);
	  g_object_unref (current_keyboard_window);
	  current_keyboard_window = NULL;
	}
      
      event = create_focus_event (window, TRUE);
      append_event (event);
      current_keyboard_window = g_object_ref (window);
    }
}

void
_gdk_quartz_events_send_map_event (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  if (!impl->toplevel)
    return;

  if (private->event_mask & GDK_STRUCTURE_MASK)
    {
      GdkEvent event;

      event.any.type = GDK_MAP;
      event.any.window = window;
  
      gdk_event_put (&event);
    }
}

/* Get current mouse window */
GdkWindow *
_gdk_quartz_events_get_mouse_window (gboolean consider_grabs)
{
  if (!consider_grabs)
    return current_mouse_window;

  if (_gdk_display->pointer_grab.window && !_gdk_display->pointer_grab.owner_events)
    return _gdk_display->pointer_grab.window;
  
  return current_mouse_window;
}

/* Update mouse window */
void
_gdk_quartz_events_update_mouse_window (GdkWindow *window)
{
  if (window == current_mouse_window)
    return;

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    _gdk_quartz_window_debug_highlight (window, 0);
#endif /* G_ENABLE_DEBUG */  

  if (window)
    g_object_ref (window);
  if (current_mouse_window)
    g_object_unref (current_mouse_window);

  current_mouse_window = window;
}

/* Update current cursor */
void
_gdk_quartz_events_update_cursor (GdkWindow *window)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  NSCursor *nscursor = nil;

  while (private)
    {
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

      nscursor = impl->nscursor;
      if (nscursor)
        break;

      private = private->parent;
    }

  GDK_QUARTZ_ALLOC_POOL;

  if (!nscursor)
    nscursor = [NSCursor arrowCursor];

  if ([NSCursor currentCursor] != nscursor)
    [nscursor set];

  GDK_QUARTZ_RELEASE_POOL;
}

/* Translates coordinates from an ancestor window + coords, to
 * coordinates that are relative the child window.
 */
static void
get_child_coordinates_from_ancestor (GdkWindow *ancestor_window,
				     gint       ancestor_x,
				     gint       ancestor_y,
				     GdkWindow *child_window, 
				     gint      *child_x, 
				     gint      *child_y)
{
  GdkWindowObject *ancestor_private = GDK_WINDOW_OBJECT (ancestor_window);
  GdkWindowObject *child_private = GDK_WINDOW_OBJECT (child_window);

  while (child_private != ancestor_private)
    {
      ancestor_x -= child_private->x;
      ancestor_y -= child_private->y;

      child_private = child_private->parent;
    }

  *child_x = ancestor_x;
  *child_y = ancestor_y;
}

/* Translates coordinates from a child window + coords, to
 * coordinates that are relative the ancestor window.
 */
static void
get_ancestor_coordinates_from_child (GdkWindow *child_window,
				     gint       child_x,
				     gint       child_y,
				     GdkWindow *ancestor_window, 
				     gint      *ancestor_x, 
				     gint      *ancestor_y)
{
  GdkWindowObject *child_private = GDK_WINDOW_OBJECT (child_window);
  GdkWindowObject *ancestor_private = GDK_WINDOW_OBJECT (ancestor_window);

  while (child_private != ancestor_private)
    {
      child_x += child_private->x;
      child_y += child_private->y;

      child_private = child_private->parent;
    }

  *ancestor_x = child_x;
  *ancestor_y = child_y;
}

/* Translates coordinates relative to one window (in_window) into
 * coordinates relative to another window (out_window).
 */
static void
get_converted_window_coordinates (GdkWindow *in_window,
                                  gint       in_x,
                                  gint       in_y,
                                  GdkWindow *out_window, 
                                  gint      *out_x, 
                                  gint      *out_y)
{
  GdkWindow *in_toplevel;
  GdkWindow *out_toplevel;
  int in_origin_x, in_origin_y;
  int out_origin_x, out_origin_y;

  if (in_window == out_window)
    {
      *out_x = in_x;
      *out_y = in_y;
      return;
    }

  /* First translate to "in" toplevel coordinates, then on to "out"
   * toplevel coordinates, and finally to "out" child (the passed in
   * window) coordinates.
   */

  in_toplevel = gdk_window_get_toplevel (in_window);
  out_toplevel  = gdk_window_get_toplevel (out_window);

  /* Translate in_x, in_y to "in" toplevel coordinates. */
  get_ancestor_coordinates_from_child (in_window, in_x, in_y,
                                       in_toplevel, &in_x, &in_y);

  gdk_window_get_origin (in_toplevel, &in_origin_x, &in_origin_y);
  gdk_window_get_origin (out_toplevel, &out_origin_x, &out_origin_y);

  /* Translate in_x, in_y to "out" toplevel coordinates. */
  in_x -= out_origin_x - in_origin_x;
  in_y -= out_origin_y - in_origin_y;

  get_child_coordinates_from_ancestor (out_toplevel, 
                                       in_x, in_y,
                                       out_window,
                                       out_x, out_y);
}

/* Trigger crossing events if necessary. This is used when showing a new
 * window, since the tracking rect API doesn't work reliably when a window
 * shows up under the mouse cursor. It's done by finding the topmost window
 * under the mouse pointer and synthesizing crossing events into that
 * window.
 */
void
_gdk_quartz_events_trigger_crossing_events (gboolean defer_to_mainloop)
{
  NSPoint point;
  gint x, y; 
  gint x_toplevel, y_toplevel;
  GdkWindow *mouse_window;
  GdkWindow *toplevel;
  GdkWindowImplQuartz *impl;
  GdkWindowObject *private;
  guint flags = 0;
  NSTimeInterval timestamp = 0;
  NSEvent *current_event;
  NSEvent *nsevent;

  if (defer_to_mainloop)
    {
      nsevent = [NSEvent otherEventWithType:NSApplicationDefined
                                   location:NSZeroPoint
                              modifierFlags:0
                                  timestamp:0
                               windowNumber:0
                                    context:nil
                                    subtype:GDK_QUARTZ_EVENT_SUBTYPE_FAKE_CROSSING
                                      data1:0
                                      data2:0];
      [NSApp postEvent:nsevent atStart:NO];
      return;
    }

  point = [NSEvent mouseLocation];
  x = point.x;
  y = _gdk_quartz_window_get_inverted_screen_y (point.y);

  mouse_window = _gdk_quartz_window_find_child (_gdk_root, x, y);
  if (!mouse_window || mouse_window == _gdk_root)
    return;

  toplevel = gdk_window_get_toplevel (mouse_window);

  /* We ignore crossing within the same toplevel since that is already
   * handled elsewhere.
   */
  if (toplevel == gdk_window_get_toplevel (current_mouse_window))
    return;

  get_converted_window_coordinates (_gdk_root,
                                    x, y,
                                    toplevel,
                                    &x_toplevel, &y_toplevel);

  get_converted_window_coordinates (_gdk_root,
                                    x, y,
                                    mouse_window,
                                    &x, &y);

  /* Fix up the event to be less fake if possible. */
  current_event = [NSApp currentEvent];
  if (current_event)
    {
      flags = [current_event modifierFlags];
      timestamp = [current_event timestamp];
    }

  if (timestamp == 0)
    timestamp = GetCurrentEventTime ();

  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);
  private = GDK_WINDOW_OBJECT (toplevel);
  nsevent = [NSEvent otherEventWithType:NSApplicationDefined
                               location:NSMakePoint (x_toplevel, private->height - y_toplevel)
                          modifierFlags:flags
                              timestamp:timestamp
                           windowNumber:[impl->toplevel windowNumber]
                                context:nil
                                subtype:GDK_QUARTZ_EVENT_SUBTYPE_FAKE_CROSSING
                                  data1:0
                                  data2:0];

#ifdef G_ENABLE_DEBUG
  /*_gdk_quartz_window_debug_highlight (mouse_window, 0);*/
#endif

  /* FIXME: create an event, fill it, put on the queue... */
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
  GdkWindow *toplevel;
  GdkWindowObject *private;
  GdkWindowImplQuartz *impl;
  NSPoint point;
  NSPoint base;
  NSEventType event_type;

  toplevel = [(GdkQuartzView *)[[nsevent window] contentView] gdkWindow];
  private = GDK_WINDOW_OBJECT (toplevel);
  impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  point = [nsevent locationInWindow];
  base = [[nsevent window] convertBaseToScreen:point];

  *x = point.x;
  *y = private->height - point.y;

  *x_root = base.x;
  *y_root = _gdk_quartz_window_get_inverted_screen_y (base.y);

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
      {
	GdkDisplay *display;

        display = gdk_drawable_get_display (toplevel);

	/* From the docs for XGrabPointer:
	 *
	 * If owner_events is True and if a generated pointer event
	 * would normally be reported to this client, it is reported
	 * as usual. Otherwise, the event is reported with respect to
	 * the grab_window and is reported only if selected by
	 * event_mask. For either value of owner_events, unreported
	 * events are discarded.
	 *
	 * This means we first try the owner, then the grab window,
	 * then give up.
	 */
	if (display->pointer_grab.window)
	  {
	    if (display->pointer_grab.owner_events)
              return toplevel;

	    /* Finally check the grab window. */
	    if (display->pointer_grab.event_mask & get_event_mask_from_ns_event (nsevent))
	      {
		GdkWindow *grab_toplevel;
		NSPoint point;
		int x_tmp, y_tmp;

		grab_toplevel = gdk_window_get_toplevel (display->pointer_grab.window);
		point = [nsevent locationInWindow];

		x_tmp = point.x;
		y_tmp = GDK_WINDOW_OBJECT (grab_toplevel)->height - point.y;

                /* FIXME: Would be better and easier to use cocoa to convert. */

                /* Translate the coordinates so they are relative to
                 * the grab window instead of the event toplevel for
                 * the cases where they are not the same.
                 */
                get_converted_window_coordinates (toplevel,
                                                  x_tmp, y_tmp,
                                                  grab_toplevel,
                                                  x, y);

		return grab_toplevel;
	      }

	    return NULL;
	  }
	else 
	  {
	    /* The non-grabbed case. */

            /* Leave events above the window (e.g. possibly on the titlebar)
             * to cocoa.
             */
            if (*y < 0)
              return NULL;

            /* FIXME: Also need to leave resize events to cocoa somehow? */

            return toplevel;
	  }
      }
      break;
      
    case NSMouseEntered:
    case NSMouseExited:
      return toplevel;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        /* FIXME: Use common code here instead. */
	if (_gdk_quartz_keyboard_grab_window && !keyboard_grab_owner_events)
	  return _gdk_quartz_keyboard_grab_window;

        return toplevel;
      }
      break;

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
  GdkWindowObject *private;
  NSPoint point;

  private = GDK_WINDOW_OBJECT (toplevel);

  point = [nsevent locationInWindow];

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
  GdkWindowObject *private;
  NSPoint point;

  private = GDK_WINDOW_OBJECT (window);

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
  event->key.keyval = GDK_VoidSymbol;
  
  gdk_keymap_translate_keyboard_state (NULL,
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
        case GDK_Meta_R:
        case GDK_Meta_L:
          mask = GDK_MOD1_MASK;
          break;
        case GDK_Shift_R:
        case GDK_Shift_L:
          mask = GDK_SHIFT_MASK;
          break;
        case GDK_Caps_Lock:
          mask = GDK_LOCK_MASK;
          break;
        case GDK_Alt_R:
        case GDK_Alt_L:
          mask = GDK_MOD5_MASK;
          break;
        case GDK_Control_R:
        case GDK_Control_L:
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
  if (event->key.keyval != GDK_VoidSymbol)
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
  else if (event->key.keyval == GDK_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_Return ||
	  event->key.keyval == GDK_KP_Enter)
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
  GdkWindowObject *private;

  private = GDK_WINDOW_OBJECT (window);

  /* FIXME: had this before csw:
     _gdk_quartz_events_update_mouse_window (window);
     if (window && !_gdk_quartz_pointer_grab_window)
       _gdk_quartz_events_update_cursor (window);
  */

  switch ([nsevent type])
    {
    case NSMouseEntered:
      {
        /* Enter events are considered always to be from the root window as
         * we can't know for sure from what window we enter.
         */
        if (!(private->event_mask & GDK_ENTER_NOTIFY_MASK))
          return FALSE;

        fill_crossing_event (window, event, nsevent,
                             x, y,
                             x_root, y_root,
                             GDK_ENTER_NOTIFY,
                             GDK_CROSSING_NORMAL,
                             GDK_NOTIFY_ANCESTOR);
      }
      return TRUE;

    case NSMouseExited:
      {
        /* Exited always is to the root window as far as we are concerned,
         * since there is no way to reliably get information about what new
         * window is entered when exiting one.
         */
        if (!(private->event_mask & GDK_LEAVE_NOTIFY_MASK))
          return FALSE;

        /*if (!mouse_window ||
            gdk_window_get_toplevel (mouse_window) ==
            gdk_window_get_toplevel (current_mouse_window))
          {
            mouse_window = _gdk_root;
          }
        */

        fill_crossing_event (window, event, nsevent,
                             x, y,
                             x_root, y_root,
                             GDK_LEAVE_NOTIFY,
                             GDK_CROSSING_NORMAL,
                             GDK_NOTIFY_ANCESTOR);
      }
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

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
                             GDK_BUTTON2_MASK | \
                             GDK_BUTTON3_MASK | \
                             GDK_BUTTON4_MASK | \
                             GDK_BUTTON5_MASK)

static void
button_event_check_implicit_grab (GdkWindow *window,
                                  GdkEvent  *event,
                                  NSEvent   *nsevent)
{
  GdkDisplay *display = gdk_drawable_get_display (window);

  /* track implicit grabs for button presses */
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      if (!display->pointer_grab.window)
	{
	  _gdk_display_set_has_pointer_grab (display,
					     window,
					     window,
					     FALSE,
					     gdk_window_get_events (window),
					     0, /* serial */
					     event->button.time,
					     TRUE);
	}
      break;

    case GDK_BUTTON_RELEASE:
      if (display->pointer_grab.window &&
	  display->pointer_grab.implicit &&
          (current_button_state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (event->button.button - 1))) == 0)
	{
	  _gdk_display_unset_has_pointer_grab (display, TRUE, TRUE,
					       event->button.time);
	}
      break;
    default:
      g_assert_not_reached ();
    }
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

  /* Handle our generated "fake" crossing events. */
  if (event_type == NSApplicationDefined &&
      [nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_FAKE_CROSSING)
    {
      /* FIXME: This needs to actually fill in the event we have... */
      _gdk_quartz_events_trigger_crossing_events (FALSE);
      return FALSE; /* ...and return TRUE instead. */
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

      result = gdk_event_apply_filters (nsevent, event, _gdk_default_filters);
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
  if ([(GdkQuartzWindow *)nswindow isInMove])
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
      GdkWindowObject *filter_private = (GdkWindowObject *) window;
      GdkFilterReturn result;

      if (filter_private->filters)
	{
	  g_object_ref (window);

	  result = gdk_event_apply_filters (nsevent, event, filter_private->filters);

	  g_object_unref (window);

	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	      goto done;
	    }
	}
    }

  /* We only activate the application on click if it's not already active,
   * or if it's active but the window isn't focused. This matches most use
   * cases of native apps (no click-through).
   */
  if ((event_type == NSRightMouseDown ||
       event_type == NSOtherMouseDown ||
       event_type == NSLeftMouseDown))
    {
      GdkWindowObject *private = (GdkWindowObject *)window;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

      if (![NSApp isActive])
        return FALSE;
      else if (![impl->toplevel isKeyWindow])
        return FALSE;
    }

  current_event_mask = get_event_mask_from_ns_event (nsevent);

  return_val = TRUE;

  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      fill_button_event (window, event, nsevent, x, y, x_root, y_root);
      button_event_check_implicit_grab (window, event, nsevent);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      fill_button_event (window, event, nsevent, x, y, x_root, y_root);
      button_event_check_implicit_grab (window, event, nsevent);
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
_gdk_events_queue (GdkDisplay *display)
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
gdk_flush (void)
{
  /* Not supported. */
}

void
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  /* Not supported. */
}

void
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  /* Not supported. */
}

void
gdk_display_sync (GdkDisplay *display)
{
  /* Not supported. */
}

void
gdk_display_flush (GdkDisplay *display)
{
  /* Not supported. */
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay      *display,
					   GdkEvent        *event,
					   GdkNativeWindow  winid)
{
  /* Not supported. */
  return FALSE;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
				     GdkEvent  *event)
{
  /* Not supported. */
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
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
_gdk_windowing_event_data_copy (const GdkEvent *src,
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
_gdk_windowing_event_data_free (GdkEvent *event)
{
  GdkEventPrivate *priv = (GdkEventPrivate *) event;

  if (priv->windowing_data)
    {
      [(NSEvent *)priv->windowing_data release];
      priv->windowing_data = NULL;
    }
}
