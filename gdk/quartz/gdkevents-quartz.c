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

/* This is the pointer grab window */
GdkWindow          *_gdk_quartz_pointer_grab_window;
static gboolean     pointer_grab_owner_events;
static GdkEventMask pointer_grab_event_mask;
static gboolean     pointer_grab_implicit;

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
			    gboolean   implicit,
			    GdkWindow *grab_window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkEvent *event = gdk_event_new (GDK_GRAB_BROKEN);

      event->grab_broken.window = window;
      event->grab_broken.send_event = 0;
      event->grab_broken.keyboard = keyboard;
      event->grab_broken.implicit = implicit;
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
				    TRUE, FALSE, window);
      
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

gboolean
gdk_keyboard_grab_info_libgtk_only (GdkDisplay *display,
				    GdkWindow **grab_window,
				    gboolean   *owner_events)
{
  if (_gdk_quartz_keyboard_grab_window) 
    {
      if (grab_window)
	*grab_window = _gdk_quartz_keyboard_grab_window;
      if (owner_events)
	*owner_events = keyboard_grab_owner_events;

      return TRUE;
    }

  return FALSE;
}

static void
pointer_ungrab_internal (gboolean only_if_implicit)
{
  if (!_gdk_quartz_pointer_grab_window)
    return;

  if (only_if_implicit && !pointer_grab_implicit)
    return;

  g_object_unref (_gdk_quartz_pointer_grab_window);
  _gdk_quartz_pointer_grab_window = NULL;

  pointer_grab_owner_events = FALSE;
  pointer_grab_event_mask = 0;
  pointer_grab_implicit = FALSE;

  /* FIXME: Send crossing events */
}

gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  return (_gdk_quartz_pointer_grab_window != NULL && 
          !pointer_grab_implicit);
}

gboolean
gdk_pointer_grab_info_libgtk_only (GdkDisplay *display,
				   GdkWindow **grab_window,
				   gboolean   *owner_events)
{
  if (!_gdk_quartz_pointer_grab_window)
    return FALSE;

  if (grab_window)
    *grab_window = _gdk_quartz_pointer_grab_window;

  if (owner_events)
    *owner_events = pointer_grab_owner_events;

  return TRUE;
}

void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time)
{
  pointer_ungrab_internal (FALSE);
}

static GdkGrabStatus
pointer_grab_internal (GdkWindow    *window,
		       gboolean	     owner_events,
		       GdkEventMask  event_mask,
		       GdkWindow    *confine_to,
		       GdkCursor    *cursor,
		       gboolean      implicit)
{
  /* FIXME: Send crossing events */
  
  _gdk_quartz_pointer_grab_window = g_object_ref (window);
  pointer_grab_owner_events = owner_events;
  pointer_grab_event_mask = event_mask;
  pointer_grab_implicit = implicit;

  return GDK_GRAB_SUCCESS;
}

GdkGrabStatus
gdk_pointer_grab (GdkWindow    *window,
		  gboolean	owner_events,
		  GdkEventMask	event_mask,
		  GdkWindow    *confine_to,
		  GdkCursor    *cursor,
		  guint32	time)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  if (_gdk_quartz_pointer_grab_window)
    {
      if (_gdk_quartz_pointer_grab_window != window)
        generate_grab_broken_event (_gdk_quartz_pointer_grab_window,
                                    FALSE, pointer_grab_implicit, window);

      pointer_ungrab_internal (FALSE);
    }

  return pointer_grab_internal (window, owner_events, event_mask, 
				confine_to, cursor, FALSE);
}

/* This is used to break any grabs in the case where we have to due to
 * the grab emulation. Instead of enforcing the desktop wide grab, we
 * break it when the app loses focus for example.
 */
static void
break_all_grabs (void)
{
  if (_gdk_quartz_keyboard_grab_window)
    {
      generate_grab_broken_event (_gdk_quartz_keyboard_grab_window,
                                  TRUE, FALSE,
                                  NULL);
      g_object_unref (_gdk_quartz_keyboard_grab_window);
      _gdk_quartz_keyboard_grab_window = NULL;
    }

  if (_gdk_quartz_pointer_grab_window)
    {
      generate_grab_broken_event (_gdk_quartz_pointer_grab_window,
                                  FALSE, pointer_grab_implicit,
                                  NULL);
      pointer_ungrab_internal (FALSE);
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

static GdkFilterReturn
apply_filters (GdkWindow  *window,
	       NSEvent    *nsevent,
	       GList      *filters)
{
  GdkFilterReturn result = GDK_FILTER_CONTINUE;
  GdkEvent *event;
  GList *node;
  GList *tmp_list;

  event = gdk_event_new (GDK_NOTHING);
  if (window != NULL)
    event->any.window = g_object_ref (window);
  ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

  /* I think GdkFilterFunc semantics require the passed-in event
   * to already be in the queue. The filter func can generate
   * more events and append them after it if it likes.
   */
  node = _gdk_event_queue_append (_gdk_display, event);
  
  tmp_list = filters;
  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter *) tmp_list->data;
      
      tmp_list = tmp_list->next;
      result = filter->function (nsevent, event, filter->data);
      if (result != GDK_FILTER_CONTINUE)
	break;
    }

  if (result == GDK_FILTER_CONTINUE || result == GDK_FILTER_REMOVE)
    {
      _gdk_event_queue_remove_link (_gdk_display, node);
      g_list_free_1 (node);
      gdk_event_free (event);
    }
  else /* GDK_FILTER_TRANSLATE */
    {
      ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
      fixup_event (event);
    }
  return result;
}

/* Checks if the passed in window is interested in the event mask, and
 * if so, it's returned. If not, the event can be propagated through
 * its ancestors until one with the right event mask is found, up to
 * the nearest toplevel.
 */
static GdkWindow *
find_window_interested_in_event_mask (GdkWindow    *window, 
				      GdkEventMask  event_mask,
				      gboolean      propagate)
{
  GdkWindowObject *private;

  private = GDK_WINDOW_OBJECT (window);
  while (private)
    {
      if (private->event_mask & event_mask)
	return (GdkWindow *)private;

      if (!propagate)
	return NULL;

      /* Don't traverse beyond toplevels. */
      if (GDK_WINDOW_TYPE (private) != GDK_WINDOW_CHILD)
	break;

      private = private->parent;
    }

  return NULL;
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

static void
convert_window_coordinates_to_root (GdkWindow *window,
				    gdouble    x,
				    gdouble    y,
				    gdouble   *x_root,
				    gdouble   *y_root)
{
  gint ox, oy;

  *x_root = x;
  *y_root = y;
  
  if (gdk_window_get_origin (window, &ox, &oy))
    {
      *x_root += ox;
      *y_root += oy;
    }
}

/* FIXME: Refactor and share with scroll event. */
static GdkEvent *
create_crossing_event (GdkWindow      *window, 
		       NSEvent        *nsevent, 
		       GdkEventType    event_type,
		       GdkCrossingMode mode, 
		       GdkNotifyType   detail)
{
  GdkEvent *event;
  gint x_tmp, y_tmp;

  event = gdk_event_new (event_type);

  event->crossing.window = window;
  event->crossing.subwindow = NULL; /* FIXME */
  event->crossing.time = get_time_from_ns_event (nsevent);

  /* Split out this block: */
  {
    NSWindow *nswindow;
    GdkWindow *toplevel;
    GdkWindowImplQuartz *impl;
    NSPoint point;

    nswindow = [nsevent window];
    point = [nsevent locationInWindow];

    toplevel = [(GdkQuartzView *)[nswindow contentView] gdkWindow];

    impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (toplevel)->impl);

    x_tmp = point.x;
    y_tmp = impl->height - point.y;

    get_converted_window_coordinates (toplevel,
                                      x_tmp, y_tmp,
                                      window,
                                      &x_tmp, &y_tmp);
    }

  event->crossing.x = x_tmp;
  event->crossing.y = y_tmp;

  convert_window_coordinates_to_root (window, 
                                      event->crossing.x, 
                                      event->crossing.y, 
				      &event->crossing.x_root,
				      &event->crossing.y_root);

  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.state = get_keyboard_modifiers_from_ns_event (nsevent);

  /* FIXME: focus and button state */

  return event;
}

static void
synthesize_enter_event (GdkWindow      *window,
			NSEvent        *nsevent,
			GdkCrossingMode mode,
			GdkNotifyType   detail)
{
  GdkEvent *event;

  if (_gdk_quartz_pointer_grab_window != NULL && 
      !pointer_grab_owner_events && 
      !(pointer_grab_event_mask & GDK_ENTER_NOTIFY_MASK))
    return;

  if (!(GDK_WINDOW_OBJECT (window)->event_mask & GDK_ENTER_NOTIFY_MASK))
    return;

  event = create_crossing_event (window, nsevent, GDK_ENTER_NOTIFY,
				 mode, detail);

  append_event (event);
}
  
static void
synthesize_enter_events (GdkWindow      *from,
			 GdkWindow      *to,
			 NSEvent        *nsevent,
			 GdkCrossingMode mode,
			 GdkNotifyType   detail)
{
  GdkWindow *prev = gdk_window_get_parent (to);

  if (prev != from)
    synthesize_enter_events (from, prev, nsevent, mode, detail);
  synthesize_enter_event (to, nsevent, mode, detail);
}

static void
synthesize_leave_event (GdkWindow      *window,
			NSEvent        *nsevent,
			GdkCrossingMode mode,
			GdkNotifyType   detail)
{
  GdkEvent *event;

  if (_gdk_quartz_pointer_grab_window != NULL && 
      !pointer_grab_owner_events && 
      !(pointer_grab_event_mask & GDK_LEAVE_NOTIFY_MASK))
    return;

  if (!(GDK_WINDOW_OBJECT (window)->event_mask & GDK_LEAVE_NOTIFY_MASK))
    return;

  event = create_crossing_event (window, nsevent, GDK_LEAVE_NOTIFY,
				 mode, detail);

  append_event (event);
}
			 
static void
synthesize_leave_events (GdkWindow    	*from,
			 GdkWindow    	*to,
			 NSEvent        *nsevent,
			 GdkCrossingMode mode,
			 GdkNotifyType	 detail)
{
  GdkWindow *next = gdk_window_get_parent (from);
  
  synthesize_leave_event (from, nsevent, mode, detail);
  if (next != to)
    synthesize_leave_events (next, to, nsevent, mode, detail);
}
			 
static void
synthesize_crossing_events (GdkWindow      *window,
			    GdkCrossingMode mode,
			    NSEvent        *nsevent,
			    gint            x,
			    gint            y)
{
  GdkWindow *intermediate, *tem, *common_ancestor;

  if (window == current_mouse_window)
    return;

  if (_gdk_quartz_window_is_ancestor (current_mouse_window, window))
    {
      /* Pointer has moved to an inferior window. */
      synthesize_leave_event (current_mouse_window, nsevent, mode, GDK_NOTIFY_INFERIOR);

      /* If there are intermediate windows, generate ENTER_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (window);

      if (intermediate != current_mouse_window)
	{
	  synthesize_enter_events (current_mouse_window, intermediate, nsevent, mode, GDK_NOTIFY_VIRTUAL);
	}

      synthesize_enter_event (window, nsevent, mode, GDK_NOTIFY_ANCESTOR);
    }
  else if (_gdk_quartz_window_is_ancestor (window, current_mouse_window))
    {
      /* Pointer has moved to an ancestor window. */
      synthesize_leave_event (current_mouse_window, nsevent, mode, GDK_NOTIFY_ANCESTOR);
      
      /* If there are intermediate windows, generate LEAVE_NOTIFY
       * events for them
       */
      intermediate = gdk_window_get_parent (current_mouse_window);
      if (intermediate != window)
	{
	  synthesize_leave_events (intermediate, window, nsevent, mode, GDK_NOTIFY_VIRTUAL);
	}

      synthesize_enter_event (window, nsevent, mode, GDK_NOTIFY_INFERIOR);
    }
  else if (current_mouse_window)
    {
      /* Find least common ancestor of current_mouse_window and window */
      tem = current_mouse_window;
      do {
	common_ancestor = gdk_window_get_parent (tem);
	tem = common_ancestor;
      } while (common_ancestor &&
	       !_gdk_quartz_window_is_ancestor (common_ancestor, window));
      if (common_ancestor)
	{
	  synthesize_leave_event (current_mouse_window, nsevent, mode, GDK_NOTIFY_NONLINEAR);
	  intermediate = gdk_window_get_parent (current_mouse_window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_leave_events (intermediate, common_ancestor,
				       nsevent, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  intermediate = gdk_window_get_parent (window);
	  if (intermediate != common_ancestor)
	    {
	      synthesize_enter_events (common_ancestor, intermediate,
				       nsevent, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  synthesize_enter_event (window, nsevent, mode, GDK_NOTIFY_NONLINEAR);
	}
    }
  else
    {
      /* This means we have no current_mouse_window, which probably
       * means that there is a bug somewhere, we should always have
       * the root in we don't have another window. Does this ever
       * happen?
       */
      g_warning ("Trying to create crossing event when current_mouse_window is NULL");
    }

  _gdk_quartz_events_update_mouse_window (window);

  /* FIXME: This does't work when someone calls gdk_window_set_cursor
   * during a grab. The right behavior is that the cursor doesn't
   * change when a grab is in effect, but in that case it does.
   */
  if (window && !_gdk_quartz_pointer_grab_window)
    _gdk_quartz_events_update_cursor (window);
}

void 
_gdk_quartz_events_send_map_events (GdkWindow *window)
{
  GList *list;
  GdkWindow *interested_window;
  GdkWindowObject *private = (GdkWindowObject *)window;

  interested_window = find_window_interested_in_event_mask (window, 
							    GDK_STRUCTURE_MASK,
							    TRUE);
  
  if (interested_window)
    {
      GdkEvent *event = gdk_event_new (GDK_MAP);
      event->any.window = interested_window;
      append_event (event);
    }

  for (list = private->children; list != NULL; list = list->next)
    _gdk_quartz_events_send_map_events ((GdkWindow *)list->data);
}

/* Get current mouse window */
GdkWindow *
_gdk_quartz_events_get_mouse_window (gboolean consider_grabs)
{
  if (!consider_grabs)
    return current_mouse_window;

  if (_gdk_quartz_pointer_grab_window && !pointer_grab_owner_events)
    return _gdk_quartz_pointer_grab_window;
  
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

/* Given a mouse NSEvent (must be a mouse event for a GDK window),
 * finds the subwindow over which the pointer is located. Returns
 * coordinates relative to the found window. If no window is found,
 * returns the root window, and root window coordinates.
 */
static GdkWindow *
find_mouse_window_for_ns_event (NSEvent *nsevent,
                                gint    *x_ret,
                                gint    *y_ret)
{
  GdkWindow *event_toplevel;
  GdkWindowImplQuartz *impl;
  GdkWindow *mouse_toplevel;
  GdkWindow *mouse_window;
  NSPoint point;
  gint x_tmp, y_tmp;

  event_toplevel = [(GdkQuartzView *)[[nsevent window] contentView] gdkWindow];
  impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (event_toplevel)->impl);
  point = [nsevent locationInWindow];

  x_tmp = point.x;
  y_tmp = impl->height - point.y;

  mouse_toplevel = gdk_window_get_toplevel (current_mouse_window);

  get_converted_window_coordinates (event_toplevel,
                                    x_tmp, y_tmp,
                                    mouse_toplevel,
                                    &x_tmp, &y_tmp);

  mouse_window = _gdk_quartz_window_find_child (mouse_toplevel, x_tmp, y_tmp);
  if (mouse_window && mouse_window != mouse_toplevel)
    {
      get_child_coordinates_from_ancestor (mouse_toplevel,
                                           x_tmp, y_tmp,
                                           mouse_window,
                                           &x_tmp, &y_tmp);
    }
  else if (!mouse_window)
    {
      /* This happens for events on the window title buttons and the
       * desktop, treat those as being on the root window.
       */
      get_converted_window_coordinates (mouse_toplevel,
                                        x_tmp, y_tmp,
                                        _gdk_root,
                                        &x_tmp, &y_tmp);
      mouse_window = _gdk_root;
    }

  *x_ret = x_tmp;
  *y_ret = y_tmp;

  return mouse_window;
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
  nsevent = [NSEvent otherEventWithType:NSApplicationDefined
                               location:NSMakePoint (x_toplevel, impl->height - y_toplevel)
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

  synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, x, y);
}

/* Synthesizes crossing events if necessary, based on the passed in
 * NSEvent. Uses NSMouseEntered and NSMouseExisted for toplevels and
 * the mouse moved/dragged events for child windows, to see if the
 * mouse window has changed.
 */
static void
synthesize_crossing_events_for_ns_event (NSEvent *nsevent)
{
  NSEventType event_type;
  GdkWindow *mouse_window;
  gint x; 
  gint y;

  event_type = [nsevent type];

  switch (event_type)
    {
    case NSMouseMoved:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      /* We only handle moving the pointer to another GDK window.
       * Leaving to a non-GDK toplevel window (or window title bar or
       * the desktop) is covered by NSMouseExited events.
       */
      mouse_window = find_mouse_window_for_ns_event (nsevent, &x, &y);
      if (mouse_window != _gdk_root)
        synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, x, y);

      break;

    case NSMouseEntered:
      {
	GdkWindow *event_toplevel;
        GdkWindowImplQuartz *impl;
        NSPoint point;

        /* This is the only case where we actually use the window from
         * the event since we need to know which toplevel we entered
         * so it can be tracked properly.
         */
        event_toplevel = [(GdkQuartzView *)[[nsevent window] contentView] gdkWindow];
        impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (event_toplevel)->impl);

        point = [nsevent locationInWindow];

        x = point.x;
        y = impl->height - point.y;

	mouse_window = _gdk_quartz_window_find_child (event_toplevel, x, y);

        /* Treat unknown windows (like the title bar buttons or
         * desktop) as the root window.
         */
        if (!mouse_window)
          mouse_window = _gdk_root;

        if (mouse_window != event_toplevel)
          get_converted_window_coordinates (event_toplevel,
                                            x, y,
                                            mouse_window,
                                            &x, &y);

        synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, x, y);
      }
      break;

    case NSMouseExited:
      {
	GdkWindow *event_toplevel;
        GdkWindowImplQuartz *impl;
        NSPoint point;

        /* We only use NSMouseExited when leaving to the root
         * window. The other cases are handled above by checking the
         * motion/button events, or getting a NSMouseEntered for
         * another GDK window. The reason we don't use NSMouseExited
         * for other windows is that quartz first delivers the entered
         * event and then the exited which is the opposite from what
         * we need.
         */
        event_toplevel = [(GdkQuartzView *)[[nsevent window] contentView] gdkWindow];
        impl = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (event_toplevel)->impl);
        point = [nsevent locationInWindow];

        x = point.x;
        y = impl->height - point.y;

        x += GDK_WINDOW_OBJECT (event_toplevel)->x;
        y += GDK_WINDOW_OBJECT (event_toplevel)->y;

        /* If there is a window other than the root window at this
         * position, it means we didn't exit to the root window and we
         * ignore the event. (Note that we can get NULL here when swithing
         * spaces for example.)
         *
         * FIXME: This is not enough, it doesn't catch the case where
         * we leave a GDK window to a non-GDK window that has GDK
         * windows below it.
         */
        mouse_window = _gdk_quartz_window_find_child (_gdk_root, x, y);

        if (!mouse_window ||
            gdk_window_get_toplevel (mouse_window) ==
            gdk_window_get_toplevel (current_mouse_window))
          {
            mouse_window = _gdk_root;
          }

        if (mouse_window == _gdk_root)
          synthesize_crossing_events (_gdk_root, GDK_CROSSING_NORMAL, nsevent, x, y);
      }
      break;

    default:
      break;
    }
}

/* This function finds the correct window to send an event to, taking
 * into account grabs, event propagation, and event masks.
 */
static GdkWindow *
find_window_for_ns_event (NSEvent *nsevent, 
                          gint    *x, 
                          gint    *y)
{
  NSEventType event_type;

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
	GdkWindow *mouse_window;
	GdkEventMask event_mask;
	GdkWindow *real_window;

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
	if (_gdk_quartz_pointer_grab_window)
	  {
	    if (pointer_grab_owner_events)
	      {
                mouse_window = find_mouse_window_for_ns_event (nsevent, x, y);
		event_mask = get_event_mask_from_ns_event (nsevent);
		real_window = find_window_interested_in_event_mask (mouse_window, event_mask, TRUE);
		
		if (mouse_window && real_window && mouse_window != real_window)
		  get_ancestor_coordinates_from_child (mouse_window,
						       *x, *y,
						       real_window,
						       x, y);

		if (real_window)
		  return real_window;
	      }

	    /* Finally check the grab window. */
	    if (pointer_grab_event_mask & get_event_mask_from_ns_event (nsevent))
	      {
                GdkWindow *event_toplevel;
		GdkWindow *grab_toplevel;
		NSPoint point;
		int x_tmp, y_tmp;

                event_toplevel = [(GdkQuartzView *)[[nsevent window] contentView] gdkWindow];
		grab_toplevel = gdk_window_get_toplevel (_gdk_quartz_pointer_grab_window);
		point = [nsevent locationInWindow];

		x_tmp = point.x;
		y_tmp = GDK_WINDOW_IMPL_QUARTZ (GDK_WINDOW_OBJECT (grab_toplevel)->impl)->height - point.y;

                /* Translate the coordinates so they are relative to
                 * the grab window instead of the event toplevel for
                 * the cases where they are not the same.
                 */
                get_converted_window_coordinates (event_toplevel,
                                                  x_tmp, y_tmp,
                                                  _gdk_quartz_pointer_grab_window,
                                                  x, y);

		return _gdk_quartz_pointer_grab_window;
	      }

	    return NULL;
	  }
	else 
	  {
	    /* The non-grabbed case. */
            mouse_window = find_mouse_window_for_ns_event (nsevent, x, y);
	    event_mask = get_event_mask_from_ns_event (nsevent);
	    real_window = find_window_interested_in_event_mask (mouse_window, event_mask, TRUE);
	    
	    /* We have to translate the coordinates if the actual
	     * window is different from the mouse window.
	     */
	    if (mouse_window && real_window && mouse_window != real_window)
	      get_ancestor_coordinates_from_child (mouse_window,
						   *x, *y,
						   real_window,
						   x, y);

	    return real_window;
	  }
      }
      break;
      
    case NSMouseEntered:
    case NSMouseExited:
      /* Already handled in synthesize_crossing_events_for_ns_event. */
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
	GdkEventMask event_mask;

	if (_gdk_quartz_keyboard_grab_window && !keyboard_grab_owner_events)
	  return _gdk_quartz_keyboard_grab_window;

	event_mask = get_event_mask_from_ns_event (nsevent);
	return find_window_interested_in_event_mask (current_keyboard_window, event_mask, TRUE);
      }
      break;

    default:
      /* Ignore everything else. */
      break;
    }

  return NULL;
}

static GdkEvent *
create_button_event (GdkWindow *window, 
                     NSEvent   *nsevent,
		     gint       x,
                     gint       y)
{
  GdkEvent *event;
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

  event = gdk_event_new (type);
  event->button.window = window;
  event->button.time = get_time_from_ns_event (nsevent);
  event->button.x = x;
  event->button.y = y;
  /* FIXME event->axes */
  event->button.state = state;
  event->button.button = button;
  event->button.device = _gdk_display->core_pointer;
  convert_window_coordinates_to_root (window, x, y, 
				      &event->button.x_root,
				      &event->button.y_root);

  return event;
}

static GdkEvent *
create_motion_event (GdkWindow *window, 
                     NSEvent   *nsevent, 
                     gint       x, 
                     gint       y)
{
  GdkEvent *event;
  GdkEventType type;
  GdkModifierType state = 0;

  switch ([nsevent type])
    {
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      state = get_mouse_button_modifiers_from_ns_event (nsevent);
      /* Fall through */
    case NSMouseMoved:
      type = GDK_MOTION_NOTIFY;
      break;
    default:
      g_assert_not_reached ();
    }

  state |= get_keyboard_modifiers_from_ns_event (nsevent);

  event = gdk_event_new (type);
  event->motion.window = window;
  event->motion.time = get_time_from_ns_event (nsevent);
  event->motion.x = x;
  event->motion.y = y;
  /* FIXME event->axes */
  event->motion.state = state;
  event->motion.is_hint = FALSE;
  event->motion.device = _gdk_display->core_pointer;
  convert_window_coordinates_to_root (window, x, y,
				      &event->motion.x_root, &event->motion.y_root);
  
  return event;
}

static GdkEvent *
create_scroll_event (GdkWindow          *window, 
                     NSEvent            *nsevent, 
                     GdkScrollDirection  direction)
{
  GdkEvent *event;
  NSPoint point;
  
  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = window;
  event->scroll.time = get_time_from_ns_event (nsevent);

  point = [nsevent locationInWindow];
  event->scroll.x = point.x;
  event->scroll.y = point.y;
  event->scroll.state = get_keyboard_modifiers_from_ns_event (nsevent);
  convert_window_coordinates_to_root (window, event->scroll.x, event->scroll.y, 
				      &event->scroll.x_root,
				      &event->scroll.y_root);

  event->scroll.direction = direction;
  event->scroll.device = _gdk_display->core_pointer;
  
  return event;
}

static GdkEvent *
create_key_event (GdkWindow    *window, 
                  NSEvent      *nsevent, 
                  GdkEventType  type)
{
  GdkEvent *event;
  GdkEventPrivate *priv;
  gchar buf[7];
  gunichar c = 0;

  event = gdk_event_new (type);

  priv = (GdkEventPrivate *) event;
  priv->windowing_data = [nsevent retain];

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
  return event;
}

GdkEventMask 
_gdk_quartz_events_get_current_event_mask (void)
{
  return current_event_mask;
}

static gboolean
gdk_event_translate (NSEvent *nsevent)
{
  NSWindow *nswindow;
  GdkWindow *window;
  GdkFilterReturn result;
  GdkEvent *event;
  int x, y;

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  if ([nsevent type] == NSAppKitDefined)
    {
      if ([nsevent subtype] == NSApplicationDeactivatedEventType)
        break_all_grabs ();

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */
    }

  /* Handle our generated "fake" crossing events. */
  if ([nsevent type] == NSApplicationDefined && 
      [nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_FAKE_CROSSING)
    {
      _gdk_quartz_events_trigger_crossing_events (FALSE);
      return TRUE;
    }

  /* Keep track of button state, since we don't get that information
   * for key events. 
   */
  switch ([nsevent type])
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

  nswindow = [nsevent window];

  /* Apply any global filters. */
  if (_gdk_default_filters)
    {
      result = apply_filters (NULL, nsevent, _gdk_default_filters);

      /* If result is GDK_FILTER_CONTINUE, we continue as if nothing
       * happened. If it is GDK_FILTER_REMOVE,
       * we return TRUE and won't send the message to Quartz.
       */
      if (result == GDK_FILTER_REMOVE)
	return TRUE;
    }

  /* Ignore events for no window or ones not created by GDK. */
  if (!nswindow || ![[nswindow contentView] isKindOfClass:[GdkQuartzView class]])
    return FALSE;

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([(GdkQuartzWindow *)nswindow isInMove])
    {
      break_all_grabs ();
      return FALSE;
    }

  /* Take care of NSMouseEntered/Exited events and mouse movements
   * events and emit the right GDK crossing events.
   */
  synthesize_crossing_events_for_ns_event (nsevent);

  /* Find the right GDK window to send the event to, taking grabs and
   * event masks into consideration.
   */
  window = find_window_for_ns_event (nsevent, &x, &y);
  if (!window)
    return FALSE;

  /* Apply any window filters. */
  result = apply_filters (window, nsevent, ((GdkWindowObject *) window)->filters);
  if (result == GDK_FILTER_REMOVE)
    return TRUE;

  /* We need the appliction to be activated on clicks so that popups
   * like context menus get events routed properly. This is handled
   * automatically for left mouse button presses but not other
   * buttons, so we do it here.
   */
  if ([nsevent type] == NSRightMouseDown || [nsevent type] == NSOtherMouseDown)
    {
      if (![NSApp isActive])
        [NSApp activateIgnoringOtherApps:YES];
    }

  current_event_mask = get_event_mask_from_ns_event (nsevent);

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      {
	GdkEventMask event_mask;

	/* Emulate implicit grab, when the window has both PRESS and RELEASE
	 * in its mask, like X.
	 */
	event_mask = (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	if (!_gdk_quartz_pointer_grab_window &&
	    (GDK_WINDOW_OBJECT (window)->event_mask & event_mask) == event_mask)
	  {
	    pointer_grab_internal (window, FALSE,
				   GDK_WINDOW_OBJECT (window)->event_mask,
				   NULL, NULL, TRUE);
	  }
      }
      
      event = create_button_event (window, nsevent, x, y);
      append_event (event);
      
      _gdk_event_button_generate (_gdk_display, event);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      event = create_button_event (window, nsevent, x, y);
      append_event (event);
      
      /* Ungrab implicit grab */
      if (_gdk_quartz_pointer_grab_window && pointer_grab_implicit)
	pointer_ungrab_internal (TRUE);
      break;

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSMouseMoved:
      event = create_motion_event (window, nsevent, x, y);
      append_event (event);
      break;

    case NSScrollWheel:
      {
	float dx = [nsevent deltaX];
	float dy = [nsevent deltaY];
	GdkScrollDirection direction;

	/* The delta is how much the mouse wheel has moved. Since there's no such thing in GTK+
	 * we accomodate by sending a different number of scroll wheel events.
	 */

	/* First do y events */
	if (dy < 0.0)
	  {
	    dy = -dy;
	    direction = GDK_SCROLL_DOWN;
	  }
	else
	  direction = GDK_SCROLL_UP;

	while (dy > 0.0)
	  {
	    event = create_scroll_event (window, nsevent, direction);
	    append_event (event);
	    dy--;

            /* Ignore the delta for now, things get too slow when the events queue up. */
            break;
	  }

	/* Now do x events */
	if (dx < 0.0)
	  {
	    dx = -dx;
	    direction = GDK_SCROLL_RIGHT;
	  }
	else
	  direction = GDK_SCROLL_LEFT;

	while (dx > 0.0)
	  {
	    event = create_scroll_event (window, nsevent, direction);
	    append_event (event);
	    dx--;
            
            /* Ignore the delta for now, things get too slow when the events queue up. */
            break;
	  }

      }
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        GdkEventType type;

        type = _gdk_quartz_keys_event_type (nsevent);
        if (type == GDK_NOTHING)
          return FALSE;
        
        event = create_key_event (window, nsevent, type);
        append_event (event);
        return TRUE;
      }
      break;

    default:
      /* Ignore everything elsee. */
      break;
    }

  return FALSE;
}

void
_gdk_events_queue (GdkDisplay *display)
{  
  NSEvent *event;

  event = _gdk_quartz_event_loop_get_pending ();
  if (event)
    {
      if (!gdk_event_translate (event))
        {
          GDK_THREADS_LEAVE ();
          [NSApp sendEvent:event];
          GDK_THREADS_ENTER ();
        }

      _gdk_quartz_event_loop_release_event (event);
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
