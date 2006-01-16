/* gdkevents-quartz.c
 *
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2005 Imendio AB
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

#include <config.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "gdkscreen.h"
#include "gdkprivate-quartz.h"

static GPollFD event_poll_fd;
static NSEvent *current_event;

/* This is the window the mouse is currently over */
static GdkWindow *current_mouse_window;

/* This is the window corresponding to the key window */
static GdkWindow *current_keyboard_window;

/* This is the pointer grab window */
GdkWindow *_gdk_quartz_pointer_grab_window;
static gboolean pointer_grab_owner_events;
static GdkEventMask pointer_grab_event_mask;
static gboolean pointer_grab_implicit;

/* This is the keyboard grab window */
GdkWindow *_gdk_quartz_keyboard_grab_window;
static gboolean keyboard_grab_owner_events;

static gboolean
gdk_event_prepare (GSource *source,
		   gint    *timeout)
{
  NSEvent *event;
  gboolean retval;
  
  GDK_QUARTZ_ALLOC_POOL;

  *timeout = -1;

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: [NSDate distantPast]
	                        inMode: NSDefaultRunLoopMode
	                       dequeue: NO];

  retval = (_gdk_event_queue_find_first (_gdk_display) != NULL ||
	    event != NULL);

  GDK_QUARTZ_RELEASE_POOL;

  return retval;
}

static gboolean
gdk_event_check (GSource *source)
{
  if (_gdk_event_queue_find_first (_gdk_display) != NULL ||
      current_event)
    return TRUE;

  /* FIXME: We should maybe try to fetch an event again here */

  return FALSE;
}

static gboolean
gdk_event_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)
{
  GdkEvent *event;

  GDK_QUARTZ_ALLOC_POOL;

  _gdk_events_queue (_gdk_display);

  event = _gdk_event_unqueue (_gdk_display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_QUARTZ_RELEASE_POOL;

  return TRUE;
}

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static GPollFunc old_poll_func;

static gint
poll_func (GPollFD *ufds, guint nfds, gint timeout_)
{
  NSEvent *event;
  NSDate *limit_date;
  int n_active = 0;

  GDK_QUARTZ_ALLOC_POOL;

  /* FIXME: Support more than one pollfd */
  g_assert (nfds == 1);

  if (timeout_ == -1)
    limit_date = [NSDate distantFuture];
  else if (timeout_ == 0)
    limit_date = [NSDate distantPast];
  else
    limit_date = [NSDate dateWithTimeIntervalSinceNow:timeout_/1000.0];

  event = [NSApp nextEventMatchingMask: NSAnyEventMask
	                     untilDate: limit_date
	                        inMode: NSDefaultRunLoopMode
                               dequeue: YES];

  if (event) 
    {
      ufds[0].revents = G_IO_IN;

      g_assert (current_event == NULL);
      
      current_event = [event retain];

      n_active ++;
    }

  GDK_QUARTZ_RELEASE_POOL;

  return n_active;
}

void 
_gdk_events_init (void)
{
  GSource *source;

  event_poll_fd.events = G_IO_IN;
  event_poll_fd.fd = -1;

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  /* FIXME: I really hate it that we need a custom poll function here.
   * I think we can do better using threads.
   */
  old_poll_func = g_main_context_get_poll_func (NULL);
  g_main_context_set_poll_func (NULL, poll_func);  

  current_mouse_window = g_object_ref (_gdk_root);
  current_keyboard_window = g_object_ref (_gdk_root);
}

gboolean
gdk_events_pending (void)
{
  /* FIXME: Implement */
  return FALSE;
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  /* FIXME: Implement */
  return NULL;
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow  *window,
		   gint        owner_events,
		   guint32     time)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (_gdk_quartz_keyboard_grab_window)
    gdk_keyboard_ungrab (time);

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
pointer_ungrab_internal (gboolean implicit)
{
  if (!_gdk_quartz_pointer_grab_window)
    return;

  if (pointer_grab_implicit && !implicit)
    return;

  g_object_unref (_gdk_quartz_pointer_grab_window);

  _gdk_quartz_pointer_grab_window = NULL;
  /* FIXME: Send crossing events */
}


gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  return _gdk_quartz_pointer_grab_window != NULL;
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

  return FALSE;
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
      if (!pointer_grab_implicit)
	return GDK_GRAB_ALREADY_GRABBED;
      else
	pointer_ungrab_internal (TRUE);
    }

  return pointer_grab_internal (window, owner_events, event_mask, 
				confine_to, cursor, FALSE);
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

static GdkWindow *
find_child_window_by_point_helper (GdkWindow *window, int x, int y, int x_offset, int y_offset, int *x_ret, int *y_ret)
{
  GList *children;

  for (children = GDK_WINDOW_OBJECT (window)->children;
       children; children = children->next)
    {
      GdkWindowObject *private = GDK_WINDOW_OBJECT (children->data);
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
      int temp_x, temp_y;

      if (!GDK_WINDOW_IS_MAPPED (private))
	continue;

      temp_x = x_offset + private->x;
      temp_y = y_offset + private->y;

      /* FIXME: Are there off by one errors here? */
      if (x >= temp_x && y >= temp_y &&
	  x < temp_x + impl->width && y < temp_y + impl->height) 
	{
	  *x_ret = x - x_offset - private->x;
	  *y_ret = y - y_offset - private->y;
	  
	  /* Look for child windows */
	  return find_child_window_by_point_helper (GDK_WINDOW (children->data), x, y, temp_x, temp_y, x_ret, y_ret);
	}
    }
  
  return window;
}

/* Given a toplevel window and coordinates, returns the window
 * in which the point is.
 */
static GdkWindow *
find_child_window_by_point (GdkWindow *toplevel, int x, int y, int *x_ret, int *y_ret)
{
  GdkWindowObject *private = (GdkWindowObject *)toplevel;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

  /* If the point is in the title bar, ignore it */
  if (y > impl->height)
    return NULL;

  /* FIXME: Check this for off-by-one errors */
  /* First flip the y coordinate */
  y = impl->height - y;

  return find_child_window_by_point_helper (toplevel, x, y, 0, 0, x_ret, y_ret);
}

/* Returns the current keyboard window */
static GdkWindow *
find_current_keyboard_window (void)
{
  /* FIXME: Handle keyboard grabs */

  return current_keyboard_window;
}

/* this function checks if the passed in window is interested in the
 * event mask. If so, it's returned. If not, the event can be propagated
 * to its parent.
 */
static GdkWindow *
find_window_interested_in_event_mask (GdkWindow   *window, 
				      GdkEventMask event_mask,
				      gboolean     propagate)
{
  while (window)
    {
      GdkWindowObject *private = GDK_WINDOW_OBJECT (window);

      if (private->event_mask & event_mask)
	return window;

      if (!propagate)
	return NULL;
      else
	window = GDK_WINDOW (private->parent);
    }

  return NULL;
}

static guint32
get_event_time (NSEvent *event)
{
  double time = [event timestamp];
  
  return time * 1000.0;
}

static int
convert_mouse_button_number (int button)
{
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

/* Return an event mask from an NSEvent */
static GdkEventMask
get_event_mask_from_ns_event (NSEvent *nsevent)
{
  NSEventType event_type = [nsevent type];

  switch (event_type)
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
      return GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK;
    case NSScrollWheel:
      /* Since applications that want button press events can get
       * scroll events on X11 (since scroll wheel events are really
       * button press events there), we need to use GDK_BUTTON_PRESS_MASK too.
       */
      return GDK_SCROLL_MASK|GDK_BUTTON_PRESS_MASK;
    case NSLeftMouseDragged:
      return GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK|
	     GDK_BUTTON_MOTION_MASK|GDK_BUTTON1_MOTION_MASK;
    case NSRightMouseDragged:
      return GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK|
	     GDK_BUTTON_MOTION_MASK|GDK_BUTTON3_MOTION_MASK;
    case NSOtherMouseDragged:
      {
	GdkEventMask mask = GDK_POINTER_MOTION_MASK|GDK_POINTER_MOTION_HINT_MASK|
	                    GDK_BUTTON_MOTION_MASK;
	if (convert_mouse_button_number ([nsevent buttonNumber]) == 2)
	  mask |= GDK_BUTTON2_MOTION_MASK;

	return mask;
      }
    case NSKeyDown:
      return GDK_KEY_PRESS_MASK;
    case NSKeyUp:
      return GDK_KEY_RELEASE_MASK;
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
  /* FIXME: send event */
  event->focus_change.in = in;

  return event;
}

void
_gdk_quartz_update_focus_window (GdkWindow *new_window)
{
  /* FIXME: Don't do this when grabbed */

  if (new_window != current_keyboard_window)
    {
      GdkEvent *event;

      event = create_focus_event (current_keyboard_window, FALSE);
      append_event (event);

      event = create_focus_event (new_window, TRUE);
      append_event (event);

      g_object_unref (current_keyboard_window);

      current_keyboard_window = g_object_ref (new_window);
    }
}

static gboolean
gdk_window_is_ancestor (GdkWindow *ancestor,
			GdkWindow *window)
{
  if (ancestor == NULL || window == NULL)
    return FALSE;

  return (gdk_window_get_parent (window) == ancestor ||
	  gdk_window_is_ancestor (ancestor, gdk_window_get_parent (window)));
}

static GdkModifierType
get_keyboard_modifiers_from_nsevent (NSEvent *nsevent)
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
  if (nsflags & NSControlKeyMask)
    modifiers |= GDK_MOD1_MASK;
  
  /* FIXME: Support GDK_BUTTON_MASK */

  return modifiers;
}

static void
convert_window_coordinates_to_root (GdkWindow *window, gdouble x, gdouble y,
				    gdouble *x_root, gdouble *y_root)
{
  /* FIXME: Implement */
  *x_root = 0;
  *y_root = 0;
}

static GdkEvent *
create_crossing_event (GdkWindow      *window, 
		       NSEvent        *nsevent, 
		       GdkEventType    event_type,
		       GdkCrossingMode mode, 
		       GdkNotifyType   detail)
{
  GdkEvent *event;

  event = gdk_event_new (event_type);
  
  event->crossing.window = window;
  event->crossing.subwindow = NULL; /* FIXME */
  event->crossing.time = get_event_time (nsevent);
  /* FIXME: x, y, x_root, y_root */
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  /* FIXME: focus */
  /* FIXME: state, (button state too) */

  return event;
}

static void
synthesize_enter_event (GdkWindow      *window,
			NSEvent        *nsevent,
			GdkCrossingMode mode,
			GdkNotifyType   detail)
{
  GdkEvent *event;

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

  if (gdk_window_is_ancestor (current_mouse_window, window))
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
  else if (gdk_window_is_ancestor (window, current_mouse_window))
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
	       !gdk_window_is_ancestor (common_ancestor, window));
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
      /* Dunno where we are coming from */
      synthesize_enter_event (window, nsevent, mode, GDK_NOTIFY_UNKNOWN);
    }

  _gdk_quartz_update_mouse_window (window);
}


void 
_gdk_quartz_send_map_events (GdkWindow *window)
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
    _gdk_quartz_send_map_events ((GdkWindow *)list->data);
}

/* Get current mouse window */
GdkWindow *
_gdk_quartz_get_mouse_window (void)
{
  /* FIXME: What about grabs? */
  return current_mouse_window;
}

/* Update mouse window */
void 
_gdk_quartz_update_mouse_window (GdkWindow *window)
{
  if (window)
    g_object_ref (window);
  if (current_mouse_window)
    g_object_unref (current_mouse_window);

  current_mouse_window = window;
}

/* Update current cursor */
void
_gdk_quartz_update_cursor (GdkWindow *window)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  NSCursor *nscursor = nil;

  while (private) {
    GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);

    nscursor = impl->nscursor;
    if (nscursor)
      break;

    private = private->parent;
  }

  if (!nscursor)
    nscursor = [NSCursor arrowCursor];

  if ([NSCursor currentCursor] != nscursor)
    [nscursor set];
}

/* This function finds the correct window to send an event to,
 * taking into account grabs (FIXME: not done yet), event propagation,
 * and event masks.
 */
static GdkWindow *
find_window_for_event (NSEvent *nsevent, gint *x, gint *y)
{
  NSWindow *nswindow = [nsevent window];
  NSEventType event_type = [nsevent type];

  if (!nswindow)
    return NULL;

  if (event_type == NSMouseMoved ||
      event_type == NSLeftMouseDragged ||
      event_type == NSRightMouseDragged ||
      event_type == NSOtherMouseDragged)
    {
      GdkWindow *toplevel = [(GdkQuartzView *)[nswindow contentView] gdkWindow];
      NSPoint point = [nsevent locationInWindow];
      GdkWindow *mouse_window;

      mouse_window = find_child_window_by_point (toplevel, point.x, point.y, x, y);

      if (!mouse_window)
	mouse_window = _gdk_root;

      if (_gdk_quartz_pointer_grab_window)
	{
	  if (mouse_window != current_mouse_window)
	    synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, *x, *y);
	}
      else
	{
	  if (current_mouse_window != mouse_window)
	    {
	      synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, *x, *y);
	      
	      _gdk_quartz_update_cursor (mouse_window);
	    }
	}
    }

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
	GdkWindow *toplevel = [(GdkQuartzView *)[nswindow contentView] gdkWindow];
	NSPoint point = [nsevent locationInWindow];
	GdkWindow *mouse_window;
	GdkEventMask event_mask;
	GdkWindow *real_window;

	if (_gdk_quartz_pointer_grab_window)
	  {
	    if (pointer_grab_event_mask & get_event_mask_from_ns_event (nsevent)) 
	      {
		int tempx, tempy;
		GdkWindowObject *w = GDK_WINDOW_OBJECT (_gdk_quartz_pointer_grab_window);
		GdkWindowObject *grab_toplevel = GDK_WINDOW_OBJECT (gdk_window_get_toplevel (_gdk_quartz_pointer_grab_window));

		tempx = point.x;
		tempy = GDK_WINDOW_IMPL_QUARTZ (grab_toplevel->impl)->height -
		  point.y;

		while (w != grab_toplevel)
		  {
		    tempx -= w->x;
		    tempy -= w->y;

		    w = w->parent;
		  }

		*x = tempx;
		*y = tempy;

		return _gdk_quartz_pointer_grab_window;
	      }
	    else
	      {
		return NULL;
	      }
	  }

	if (!nswindow)
	  {
	    mouse_window = _gdk_root;
	  }
	else 
	  {
	    mouse_window = find_child_window_by_point (toplevel, point.x, point.y, x, y);
	  }

	event_mask = get_event_mask_from_ns_event (nsevent);
	real_window = find_window_interested_in_event_mask (mouse_window, event_mask, TRUE);
	
	return real_window;
      }
    case NSMouseEntered:
      {
	NSPoint point;
	GdkWindow *toplevel;
	GdkWindow *mouse_window;

	point = [nsevent locationInWindow];

	toplevel = [(GdkQuartzView *)[nswindow contentView] gdkWindow];

	mouse_window = find_child_window_by_point (toplevel, point.x, point.y, x, y);
	
	synthesize_crossing_events (mouse_window, GDK_CROSSING_NORMAL, nsevent, *x, *y);

	break;
      }
    case NSMouseExited:
      synthesize_crossing_events (_gdk_root, GDK_CROSSING_NORMAL, nsevent, *x, *y);
      break;

    case NSKeyDown:
    case NSKeyUp:
      {
	GdkWindow *keyboard_window;
	GdkEventMask event_mask;
	GdkWindow *real_window;

	if (_gdk_quartz_keyboard_grab_window && !keyboard_grab_owner_events)
	  return _gdk_quartz_keyboard_grab_window;

	keyboard_window = find_current_keyboard_window ();
	event_mask = get_event_mask_from_ns_event (nsevent);
	real_window = find_window_interested_in_event_mask (keyboard_window, event_mask, TRUE);

	return real_window;
      }
      break;

    case NSAppKitDefined:
    case NSSystemDefined:
      /* We ignore these events */
      break;
    default:
      NSLog(@"Unhandled event %@", nsevent);
    }

  return NULL;
}

static GdkEvent *
create_button_event (GdkWindow *window, NSEvent *nsevent,
		     gint x, gint y)
{
  GdkEvent *event;
  GdkEventType type;
  guint button;

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      type = GDK_BUTTON_PRESS;
      button = convert_mouse_button_number ([nsevent buttonNumber]);
      break;
    case NSLeftMouseUp:
      type = GDK_BUTTON_RELEASE;
      button = 1;
      break;
    case NSRightMouseUp:
      type = GDK_BUTTON_RELEASE;
      button = 3;
      break;
    case NSOtherMouseUp:
      type = GDK_BUTTON_RELEASE;
      button = convert_mouse_button_number ([nsevent buttonNumber]);
      break;
    default:
      g_assert_not_reached ();
    }
  
  event = gdk_event_new (type);
  event->button.window = window;
  event->button.time = get_event_time (nsevent);
  event->button.x = x;
  event->button.y = y;
  /* FIXME event->axes */
  event->button.state = get_keyboard_modifiers_from_nsevent (nsevent);
  event->button.button = button;
  event->button.device = _gdk_display->core_pointer;
  convert_window_coordinates_to_root (window, x, y, 
				      &event->button.x_root,
				      &event->button.y_root);

  return event;
}

static GdkEvent *
create_motion_event (GdkWindow *window, NSEvent *nsevent, gint x, gint y)
{
  GdkEvent *event;
  GdkEventType type;
  GdkModifierType state = 0;
  int button = 0;

  switch ([nsevent type])
    {
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      button = convert_mouse_button_number ([nsevent buttonNumber]);
      /* Fall through */
    case NSMouseMoved:
      type = GDK_MOTION_NOTIFY;
      break;
    default:
      g_assert_not_reached ();
    }

  /* This maps buttons 1 to 5 to GDK_BUTTON[1-5]_MASK */
  if (button >= 1 && button <= 5)
    state = (1 << (button + 7));
  
  state |= get_keyboard_modifiers_from_nsevent (nsevent);

  event = gdk_event_new (type);
  event->motion.window = window;
  event->motion.time = get_event_time (nsevent);
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
create_scroll_event (GdkWindow *window, NSEvent *nsevent, GdkScrollDirection direction)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = window;
  event->scroll.time = get_event_time (nsevent);
  /* FIXME event->x, event->y */
  /* FIXME event->state; */
  /* FIXME event->is_hint; */
  event->scroll.direction = direction;
  event->scroll.device = _gdk_display->core_pointer;
  /* FIXME: event->x_root, event->y_root */
  
  return event;
}

static GdkEvent *
create_key_event (GdkWindow *window, NSEvent *nsevent)
{
  GdkEventType event_type;
  GdkEvent *event;

  switch ([nsevent type]) 
    {
    case NSKeyDown:
      event_type = GDK_KEY_PRESS;
      break;
    case NSKeyUp:
      event_type = GDK_KEY_RELEASE;
      break;
    default:
      g_assert_not_reached ();
    }

  event = gdk_event_new (event_type);
  event->key.window = window;
  event->key.time = get_event_time (nsevent);
  event->key.state = get_keyboard_modifiers_from_nsevent (nsevent);
  event->key.hardware_keycode = [nsevent keyCode];
  event->key.group = ([nsevent modifierFlags] & NSAlternateKeyMask) ? 1 : 0;
  
  gdk_keymap_translate_keyboard_state (NULL,
				       event->key.hardware_keycode,
				       event->key.state, 
				       event->key.group,
				       &event->key.keyval,
				       NULL, NULL, NULL);

  return event;
}

static gboolean
gdk_event_translate (NSEvent *nsevent)
{
  NSWindow *nswindow = [nsevent window];
  GdkWindow *window;
  GdkFilterReturn result;
  GdkEvent *event;
  int x, y;

  if (_gdk_default_filters)
    {
      /* Apply global filters */

      GdkFilterReturn result = apply_filters (NULL, nsevent, _gdk_default_filters);
      
      /* If result is GDK_FILTER_CONTINUE, we continue as if nothing
       * happened. If it is GDK_FILTER_REMOVE,
       * we return TRUE and won't send the message to Quartz.
       */
      if (result == GDK_FILTER_REMOVE)
	return TRUE;
    }

  window = find_window_for_event (nsevent, &x, &y);

  if (!window)
    return FALSE;

  result = apply_filters (window, nsevent, ((GdkWindowObject *) window)->filters);

  if (result == GDK_FILTER_REMOVE)
    return TRUE;
  
  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      /* Emulate implicit grab */
      if (!_gdk_quartz_pointer_grab_window)
	{
	  pointer_grab_internal (window, FALSE, GDK_WINDOW_OBJECT (window)->event_mask,
				 NULL, NULL, TRUE);
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
      if (_gdk_quartz_pointer_grab_window &&
	  pointer_grab_implicit)
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
	  }

	break;
      }
    case NSKeyDown:
    case NSKeyUp:
      event = create_key_event (window, nsevent);
      append_event (event);
      return TRUE;
      break;

    default:
      NSLog(@"Untranslated: %@", nsevent);
    }

  return FALSE;
}

void
_gdk_events_queue (GdkDisplay *display)
{  
    if (current_event)
      {
	
	if (!gdk_event_translate (current_event))
	  {
	    [NSApp sendEvent:current_event];
	  }
	
	[current_event release];
	current_event = NULL;
      }
}

void
gdk_flush (void)
{
  gdk_display_flush (NULL);
}

void
gdk_display_sync (GdkDisplay *display)
{
  /* FIXME: Implement */
}

void
gdk_display_flush (GdkDisplay *display)
{
  /* FIXME: Implement */
}

gboolean
gdk_event_send_client_message_for_display (GdkDisplay      *display,
					   GdkEvent        *event,
					   GdkNativeWindow  winid)
{
  return FALSE;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
                     				     GdkEvent  *event)
{
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{
  if (strcmp (name, "gtk-font-name") == 0)
    {
	   /* FIXME: This should be fetched from the correct preference value */
      g_value_set_string (value, "Lucida Grande 13");
      return TRUE;
    }
  
  /* FIXME: Add more settings */

  return FALSE;
}

