/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkinternals.h"
#include "gdkdisplayprivate.h"
#include "gdkdndprivate.h"

#include <string.h>
#include <math.h>


/**
 * SECTION:events
 * @Short_description: Functions for handling events from the window system
 * @Title: Events
 * @See_also: [Event Structures][gdk3-Event-Structures]
 *
 * This section describes functions dealing with events from the window
 * system.
 *
 * In GTK+ applications the events are handled automatically in
 * gtk_main_do_event() and passed on to the appropriate widgets, so these
 * functions are rarely needed. Though some of the fields in the
 * [Event Structures][gdk3-Event-Structures] are useful.
 */


typedef struct _GdkIOClosure GdkIOClosure;

struct _GdkIOClosure
{
  GDestroyNotify notify;
  gpointer data;
};

/* Private variable declarations
 */

static GdkEventFunc   _gdk_event_func = NULL;    /* Callback for events */
static gpointer       _gdk_event_data = NULL;
static GDestroyNotify _gdk_event_notify = NULL;

void
_gdk_event_emit (GdkEvent *event)
{
  if (gdk_drag_context_handle_source_event (event))
    return;

  if (_gdk_event_func)
    (*_gdk_event_func) (event, _gdk_event_data);

  if (gdk_drag_context_handle_dest_event (event))
    return;
}

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

/**
 * _gdk_event_queue_find_first:
 * @display: a #GdkDisplay
 * 
 * Find the first event on the queue that is not still
 * being filled in.
 * 
 * Returns: (nullable): Pointer to the list node for that event, or
 *   %NULL.
 **/
GList*
_gdk_event_queue_find_first (GdkDisplay *display)
{
  GList *tmp_list;
  GList *pending_motion = NULL;

  gboolean paused = display->event_pause_count > 0;

  tmp_list = display->queued_events;
  while (tmp_list)
    {
      GdkEventPrivate *event = tmp_list->data;

      if ((event->flags & GDK_EVENT_PENDING) == 0 &&
	  (!paused || (event->flags & GDK_EVENT_FLUSHED) != 0))
        {
          if (pending_motion)
            return pending_motion;

          if (event->event.type == GDK_MOTION_NOTIFY && (event->flags & GDK_EVENT_FLUSHED) == 0)
            pending_motion = tmp_list;
          else
            return tmp_list;
        }

      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * _gdk_event_queue_append:
 * @display: a #GdkDisplay
 * @event: Event to append.
 * 
 * Appends an event onto the tail of the event queue.
 *
 * Returns: the newly appended list node.
 **/
GList *
_gdk_event_queue_append (GdkDisplay *display,
			 GdkEvent   *event)
{
  display->queued_tail = g_list_append (display->queued_tail, event);
  
  if (!display->queued_events)
    display->queued_events = display->queued_tail;
  else
    display->queued_tail = display->queued_tail->next;

  return display->queued_tail;
}

/**
 * _gdk_event_queue_insert_after:
 * @display: a #GdkDisplay
 * @sibling: Append after this event.
 * @event: Event to append.
 *
 * Appends an event after the specified event, or if it isn’t in
 * the queue, onto the tail of the event queue.
 *
 * Returns: the newly appended list node.
 *
 * Since: 2.16
 */
GList*
_gdk_event_queue_insert_after (GdkDisplay *display,
                               GdkEvent   *sibling,
                               GdkEvent   *event)
{
  GList *prev = g_list_find (display->queued_events, sibling);
  if (prev && prev->next)
    {
      display->queued_events = g_list_insert_before (display->queued_events, prev->next, event);
      return prev->next;
    }
  else
    return _gdk_event_queue_append (display, event);
}

/**
 * _gdk_event_queue_insert_before:
 * @display: a #GdkDisplay
 * @sibling: Append before this event
 * @event: Event to prepend
 *
 * Prepends an event before the specified event, or if it isn’t in
 * the queue, onto the head of the event queue.
 *
 * Returns: the newly prepended list node.
 *
 * Since: 2.16
 */
GList*
_gdk_event_queue_insert_before (GdkDisplay *display,
				GdkEvent   *sibling,
				GdkEvent   *event)
{
  GList *next = g_list_find (display->queued_events, sibling);
  if (next)
    {
      display->queued_events = g_list_insert_before (display->queued_events, next, event);
      return next->prev;
    }
  else
    return _gdk_event_queue_append (display, event);
}


/**
 * _gdk_event_queue_remove_link:
 * @display: a #GdkDisplay
 * @node: node to remove
 * 
 * Removes a specified list node from the event queue.
 **/
void
_gdk_event_queue_remove_link (GdkDisplay *display,
			      GList      *node)
{
  if (node->prev)
    node->prev->next = node->next;
  else
    display->queued_events = node->next;
  
  if (node->next)
    node->next->prev = node->prev;
  else
    display->queued_tail = node->prev;
}

/**
 * _gdk_event_unqueue:
 * @display: a #GdkDisplay
 * 
 * Removes and returns the first event from the event
 * queue that is not still being filled in.
 * 
 * Returns: (nullable): the event, or %NULL. Ownership is transferred
 * to the caller.
 **/
GdkEvent*
_gdk_event_unqueue (GdkDisplay *display)
{
  GdkEvent *event = NULL;
  GList *tmp_list;

  tmp_list = _gdk_event_queue_find_first (display);

  if (tmp_list)
    {
      event = tmp_list->data;
      _gdk_event_queue_remove_link (display, tmp_list);
      g_list_free_1 (tmp_list);
    }

  return event;
}

void
_gdk_event_queue_handle_motion_compression (GdkDisplay *display)
{
  GList *tmp_list;
  GList *pending_motions = NULL;
  GdkWindow *pending_motion_window = NULL;
  GdkDevice *pending_motion_device = NULL;

  /* If the last N events in the event queue are motion notify
   * events for the same window, drop all but the last */

  tmp_list = display->queued_tail;

  while (tmp_list)
    {
      GdkEventPrivate *event = tmp_list->data;

      if (event->flags & GDK_EVENT_PENDING)
        break;

      if (event->event.type != GDK_MOTION_NOTIFY)
        break;

      if (pending_motion_window != NULL &&
          pending_motion_window != event->event.motion.window)
        break;

      if (pending_motion_device != NULL &&
          pending_motion_device != event->event.motion.device)
        break;

      if (!event->event.motion.window->event_compression)
        break;

      pending_motion_window = event->event.motion.window;
      pending_motion_device = event->event.motion.device;
      pending_motions = tmp_list;

      tmp_list = tmp_list->prev;
    }

  while (pending_motions && pending_motions->next != NULL)
    {
      GList *next = pending_motions->next;
      gdk_event_free (pending_motions->data);
      display->queued_events = g_list_delete_link (display->queued_events,
                                                   pending_motions);
      pending_motions = next;
    }

  if (pending_motions &&
      pending_motions == display->queued_events &&
      pending_motions == display->queued_tail)
    {
      GdkFrameClock *clock = gdk_window_get_frame_clock (pending_motion_window);
      if (clock) /* might be NULL if window was destroyed */
	gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS);
    }
}

void
_gdk_event_queue_flush (GdkDisplay *display)
{
  GList *tmp_list;

  for (tmp_list = display->queued_events; tmp_list; tmp_list = tmp_list->next)
    {
      GdkEventPrivate *event = tmp_list->data;
      event->flags |= GDK_EVENT_FLUSHED;
    }
}

/**
 * gdk_event_handler_set:
 * @func: the function to call to handle events from GDK.
 * @data: user data to pass to the function. 
 * @notify: the function to call when the handler function is removed, i.e. when
 *          gdk_event_handler_set() is called with another event handler.
 * 
 * Sets the function to call to handle all events from GDK.
 *
 * Note that GTK+ uses this to install its own event handler, so it is
 * usually not useful for GTK+ applications. (Although an application
 * can call this function then call gtk_main_do_event() to pass
 * events to GTK+.)
 **/
void 
gdk_event_handler_set (GdkEventFunc   func,
		       gpointer       data,
		       GDestroyNotify notify)
{
  if (_gdk_event_notify)
    (*_gdk_event_notify) (_gdk_event_data);

  _gdk_event_func = func;
  _gdk_event_data = data;
  _gdk_event_notify = notify;
}

/**
 * gdk_events_pending:
 *
 * Checks if any events are ready to be processed for any display.
 *
 * Returns: %TRUE if any events are pending.
 */
gboolean
gdk_events_pending (void)
{
  GSList *list, *l;
  gboolean pending;

  pending = FALSE;
  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      if (_gdk_event_queue_find_first (l->data))
        {
          pending = TRUE;
          goto out;
        }
    }

  for (l = list; l; l = l->next)
    {
      if (gdk_display_has_pending (l->data))
        {
          pending = TRUE;
          goto out;
        }
    }

 out:
  g_slist_free (list);

  return pending;
}

/**
 * gdk_event_get:
 * 
 * Checks all open displays for a #GdkEvent to process,to be processed
 * on, fetching events from the windowing system if necessary.
 * See gdk_display_get_event().
 * 
 * Returns: (nullable): the next #GdkEvent to be processed, or %NULL
 * if no events are pending. The returned #GdkEvent should be freed
 * with gdk_event_free().
 **/
GdkEvent*
gdk_event_get (void)
{
  GSList *list, *l;
  GdkEvent *event;

  event = NULL;
  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      event = gdk_display_get_event (l->data);
      if (event)
        break;
    }

  g_slist_free (list);

  return event;
}

/**
 * gdk_event_peek:
 *
 * If there is an event waiting in the event queue of some open
 * display, returns a copy of it. See gdk_display_peek_event().
 * 
 * Returns: (nullable): a copy of the first #GdkEvent on some event
 * queue, or %NULL if no events are in any queues. The returned
 * #GdkEvent should be freed with gdk_event_free().
 **/
GdkEvent*
gdk_event_peek (void)
{
  GSList *list, *l;
  GdkEvent *event;

  event = NULL;
  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      event = gdk_display_peek_event (l->data);
      if (event)
        break;
    }

  g_slist_free (list);

  return event;
}

static GdkDisplay *
event_get_display (const GdkEvent *event)
{
  if (event->any.window)
    return gdk_window_get_display (event->any.window);
  else
    return gdk_display_get_default ();
}

/**
 * gdk_event_put:
 * @event: a #GdkEvent.
 *
 * Appends a copy of the given event onto the front of the event
 * queue for event->any.window’s display, or the default event
 * queue if event->any.window is %NULL. See gdk_display_put_event().
 **/
void
gdk_event_put (const GdkEvent *event)
{
  GdkDisplay *display;
  
  g_return_if_fail (event != NULL);

  display = event_get_display (event);

  gdk_display_put_event (display, event);
}

static GHashTable *event_hash = NULL;

/**
 * gdk_event_new:
 * @type: a #GdkEventType 
 * 
 * Creates a new event of the given type. All fields are set to 0.
 * 
 * Returns: a newly-allocated #GdkEvent. The returned #GdkEvent 
 * should be freed with gdk_event_free().
 *
 * Since: 2.2
 **/
GdkEvent*
gdk_event_new (GdkEventType type)
{
  GdkEventPrivate *new_private;
  GdkEvent *new_event;
  
  if (!event_hash)
    event_hash = g_hash_table_new (g_direct_hash, NULL);

  new_private = g_slice_new0 (GdkEventPrivate);
  
  new_private->flags = 0;
  new_private->screen = NULL;

  g_hash_table_insert (event_hash, new_private, GUINT_TO_POINTER (1));

  new_event = (GdkEvent *) new_private;

  new_event->any.type = type;

  /*
   * Bytewise 0 initialization is reasonable for most of the 
   * current event types. Explicitely initialize double fields
   * since I trust bytewise 0 == 0. less than for integers
   * or pointers.
   */
  switch (type)
    {
    case GDK_MOTION_NOTIFY:
      new_event->motion.x = 0.;
      new_event->motion.y = 0.;
      new_event->motion.x_root = 0.;
      new_event->motion.y_root = 0.;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      new_event->button.x = 0.;
      new_event->button.y = 0.;
      new_event->button.x_root = 0.;
      new_event->button.y_root = 0.;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      new_event->touch.x = 0.;
      new_event->touch.y = 0.;
      new_event->touch.x_root = 0.;
      new_event->touch.y_root = 0.;
      break;
    case GDK_SCROLL:
      new_event->scroll.x = 0.;
      new_event->scroll.y = 0.;
      new_event->scroll.x_root = 0.;
      new_event->scroll.y_root = 0.;
      new_event->scroll.delta_x = 0.;
      new_event->scroll.delta_y = 0.;
      new_event->scroll.is_stop = FALSE;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      new_event->crossing.x = 0.;
      new_event->crossing.y = 0.;
      new_event->crossing.x_root = 0.;
      new_event->crossing.y_root = 0.;
      break;
    case GDK_TOUCHPAD_SWIPE:
      new_event->touchpad_swipe.x = 0;
      new_event->touchpad_swipe.y = 0;
      new_event->touchpad_swipe.dx = 0;
      new_event->touchpad_swipe.dy = 0;
      new_event->touchpad_swipe.x_root = 0;
      new_event->touchpad_swipe.y_root = 0;
      break;
    case GDK_TOUCHPAD_PINCH:
      new_event->touchpad_pinch.x = 0;
      new_event->touchpad_pinch.y = 0;
      new_event->touchpad_pinch.dx = 0;
      new_event->touchpad_pinch.dy = 0;
      new_event->touchpad_pinch.angle_delta = 0;
      new_event->touchpad_pinch.scale = 0;
      new_event->touchpad_pinch.x_root = 0;
      new_event->touchpad_pinch.y_root = 0;
      break;
    default:
      break;
    }
  
  return new_event;
}

static gboolean
gdk_event_is_allocated (const GdkEvent *event)
{
  if (event_hash)
    return g_hash_table_lookup (event_hash, event) != NULL;

  return FALSE;
}

void
gdk_event_set_pointer_emulated (GdkEvent *event,
                                gboolean  emulated)
{
  if (gdk_event_is_allocated (event))
    {
      GdkEventPrivate *private = (GdkEventPrivate *) event;

      if (emulated)
        private->flags |= GDK_EVENT_POINTER_EMULATED;
      else
        private->flags &= ~(GDK_EVENT_POINTER_EMULATED);
    }
}

/**
 * gdk_event_get_pointer_emulated:
 * #event: a #GdkEvent
 *
 * Returns whether this event is an 'emulated' pointer event (typically
 * from a touch event), as opposed to a real one.
 *
 * Returns: %TRUE if this event is emulated
 *
 * Since: 3.22
 */
gboolean
gdk_event_get_pointer_emulated (GdkEvent *event)
{
  if (gdk_event_is_allocated (event))
    return (((GdkEventPrivate *) event)->flags & GDK_EVENT_POINTER_EMULATED) != 0;

  return FALSE;
}

/**
 * gdk_event_copy:
 * @event: a #GdkEvent
 * 
 * Copies a #GdkEvent, copying or incrementing the reference count of the
 * resources associated with it (e.g. #GdkWindow’s and strings).
 * 
 * Returns: a copy of @event. The returned #GdkEvent should be freed with
 * gdk_event_free().
 **/
GdkEvent*
gdk_event_copy (const GdkEvent *event)
{
  GdkEventPrivate *new_private;
  GdkEvent *new_event;

  g_return_val_if_fail (event != NULL, NULL);

  new_event = gdk_event_new (GDK_NOTHING);
  new_private = (GdkEventPrivate *)new_event;

  *new_event = *event;
  if (new_event->any.window)
    g_object_ref (new_event->any.window);

  if (gdk_event_is_allocated (event))
    {
      GdkEventPrivate *private = (GdkEventPrivate *)event;

      new_private->screen = private->screen;
      new_private->device = private->device ? g_object_ref (private->device) : NULL;
      new_private->source_device = private->source_device ? g_object_ref (private->source_device) : NULL;
      new_private->seat = private->seat;
      new_private->tool = private->tool;
    }

  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      new_event->key.string = g_strdup (event->key.string);
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
        g_object_ref (event->crossing.subwindow);
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      g_object_ref (event->dnd.context);
      break;

    case GDK_EXPOSE:
    case GDK_DAMAGE:
      if (event->expose.region)
        new_event->expose.region = cairo_region_copy (event->expose.region);
      break;

    case GDK_SETTING:
      new_event->setting.name = g_strdup (new_event->setting.name);
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      if (event->button.axes)
        new_event->button.axes = g_memdup (event->button.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (gdk_event_get_source_device(event)));
      break;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      if (event->touch.axes)
        new_event->touch.axes = g_memdup (event->touch.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (gdk_event_get_source_device(event)));
      break;

    case GDK_MOTION_NOTIFY:
      if (event->motion.axes)
        new_event->motion.axes = g_memdup (event->motion.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (gdk_event_get_source_device(event)));
      break;

    case GDK_OWNER_CHANGE:
      new_event->owner_change.owner = event->owner_change.owner;
      if (new_event->owner_change.owner)
        g_object_ref (new_event->owner_change.owner);
      break;

    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_NOTIFY:
    case GDK_SELECTION_REQUEST:
      new_event->selection.requestor = event->selection.requestor;
      if (new_event->selection.requestor)
        g_object_ref (new_event->selection.requestor);
      break;

    default:
      break;
    }

  if (gdk_event_is_allocated (event))
    _gdk_display_event_data_copy (event_get_display (event), event, new_event);

  return new_event;
}

/**
 * gdk_event_free:
 * @event:  a #GdkEvent.
 * 
 * Frees a #GdkEvent, freeing or decrementing any resources associated with it.
 * Note that this function should only be called with events returned from
 * functions such as gdk_event_peek(), gdk_event_get(), gdk_event_copy()
 * and gdk_event_new().
 **/
void
gdk_event_free (GdkEvent *event)
{
  GdkEventPrivate *private;
  GdkDisplay *display;

  g_return_if_fail (event != NULL);

  if (gdk_event_is_allocated (event))
    {
      private = (GdkEventPrivate *) event;
      g_clear_object (&private->device);
      g_clear_object (&private->source_device);
    }

  switch (event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      g_free (event->key.string);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.subwindow != NULL)
	g_object_unref (event->crossing.subwindow);
      break;
      
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      if (event->dnd.context != NULL)
        g_object_unref (event->dnd.context);
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      g_free (event->button.axes);
      break;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      g_free (event->touch.axes);
      break;

    case GDK_EXPOSE:
    case GDK_DAMAGE:
      if (event->expose.region)
	cairo_region_destroy (event->expose.region);
      break;
      
    case GDK_MOTION_NOTIFY:
      g_free (event->motion.axes);
      break;
      
    case GDK_SETTING:
      g_free (event->setting.name);
      break;
      
    case GDK_OWNER_CHANGE:
      if (event->owner_change.owner)
        g_object_unref (event->owner_change.owner);
      break;

    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_NOTIFY:
    case GDK_SELECTION_REQUEST:
      if (event->selection.requestor)
        g_object_unref (event->selection.requestor);
      break;

    default:
      break;
    }

  display = event_get_display (event);
  if (display)
    _gdk_display_event_data_free (display, event);

  if (event->any.window)
    g_object_unref (event->any.window);

  g_hash_table_remove (event_hash, event);
  g_slice_free (GdkEventPrivate, (GdkEventPrivate*) event);
}

/**
 * gdk_event_get_window:
 * @event: a #GdkEvent
 *
 * Extracts the #GdkWindow associated with an event.
 *
 * Returns: (transfer none): The #GdkWindow associated with the event
 *
 * Since: 3.10
 */
GdkWindow *
gdk_event_get_window (const GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);

  return event->any.window;
}

/**
 * gdk_event_get_time:
 * @event: a #GdkEvent
 * 
 * Returns the time stamp from @event, if there is one; otherwise
 * returns #GDK_CURRENT_TIME. If @event is %NULL, returns #GDK_CURRENT_TIME.
 * 
 * Returns: time stamp field from @event
 **/
guint32
gdk_event_get_time (const GdkEvent *event)
{
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	return event->motion.time;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
	return event->button.time;
      case GDK_TOUCH_BEGIN:
      case GDK_TOUCH_UPDATE:
      case GDK_TOUCH_END:
      case GDK_TOUCH_CANCEL:
        return event->touch.time;
      case GDK_TOUCHPAD_SWIPE:
        return event->touchpad_swipe.time;
      case GDK_TOUCHPAD_PINCH:
        return event->touchpad_pinch.time;
      case GDK_SCROLL:
        return event->scroll.time;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	return event->key.time;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	return event->crossing.time;
      case GDK_PROPERTY_NOTIFY:
	return event->property.time;
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
	return event->selection.time;
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
	return event->proximity.time;
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
	return event->dnd.time;
      case GDK_PAD_BUTTON_PRESS:
      case GDK_PAD_BUTTON_RELEASE:
        return event->pad_button.time;
      case GDK_PAD_RING:
      case GDK_PAD_STRIP:
        return event->pad_axis.time;
      case GDK_PAD_GROUP_MODE:
        return event->pad_group_mode.time;
      case GDK_CLIENT_EVENT:
      case GDK_VISIBILITY_NOTIFY:
      case GDK_CONFIGURE:
      case GDK_FOCUS_CHANGE:
      case GDK_NOTHING:
      case GDK_DAMAGE:
      case GDK_DELETE:
      case GDK_DESTROY:
      case GDK_EXPOSE:
      case GDK_MAP:
      case GDK_UNMAP:
      case GDK_WINDOW_STATE:
      case GDK_SETTING:
      case GDK_OWNER_CHANGE:
      case GDK_GRAB_BROKEN:
      case GDK_EVENT_LAST:
        /* return current time */
        break;
      }
  
  return GDK_CURRENT_TIME;
}

/**
 * gdk_event_get_state:
 * @event: (allow-none): a #GdkEvent or %NULL
 * @state: (out): return location for state
 * 
 * If the event contains a “state” field, puts that field in @state. Otherwise
 * stores an empty state (0). Returns %TRUE if there was a state field
 * in the event. @event may be %NULL, in which case it’s treated
 * as if the event had no state field.
 * 
 * Returns: %TRUE if there was a state field in the event 
 **/
gboolean
gdk_event_get_state (const GdkEvent        *event,
                     GdkModifierType       *state)
{
  g_return_val_if_fail (state != NULL, FALSE);
  
  if (event)
    switch (event->type)
      {
      case GDK_MOTION_NOTIFY:
	*state = event->motion.state;
        return TRUE;
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        *state = event->button.state;
        return TRUE;
      case GDK_TOUCH_BEGIN:
      case GDK_TOUCH_UPDATE:
      case GDK_TOUCH_END:
      case GDK_TOUCH_CANCEL:
        *state = event->touch.state;
        return TRUE;
      case GDK_TOUCHPAD_SWIPE:
        *state = event->touchpad_swipe.state;
        return TRUE;
      case GDK_TOUCHPAD_PINCH:
        *state = event->touchpad_pinch.state;
        return TRUE;
      case GDK_SCROLL:
	*state =  event->scroll.state;
        return TRUE;
      case GDK_KEY_PRESS:
      case GDK_KEY_RELEASE:
	*state =  event->key.state;
        return TRUE;
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	*state =  event->crossing.state;
        return TRUE;
      case GDK_PROPERTY_NOTIFY:
      case GDK_VISIBILITY_NOTIFY:
      case GDK_CLIENT_EVENT:
      case GDK_CONFIGURE:
      case GDK_FOCUS_CHANGE:
      case GDK_SELECTION_CLEAR:
      case GDK_SELECTION_REQUEST:
      case GDK_SELECTION_NOTIFY:
      case GDK_PROXIMITY_IN:
      case GDK_PROXIMITY_OUT:
      case GDK_DAMAGE:
      case GDK_DRAG_ENTER:
      case GDK_DRAG_LEAVE:
      case GDK_DRAG_MOTION:
      case GDK_DRAG_STATUS:
      case GDK_DROP_START:
      case GDK_DROP_FINISHED:
      case GDK_NOTHING:
      case GDK_DELETE:
      case GDK_DESTROY:
      case GDK_EXPOSE:
      case GDK_MAP:
      case GDK_UNMAP:
      case GDK_WINDOW_STATE:
      case GDK_SETTING:
      case GDK_OWNER_CHANGE:
      case GDK_GRAB_BROKEN:
      case GDK_PAD_BUTTON_PRESS:
      case GDK_PAD_BUTTON_RELEASE:
      case GDK_PAD_RING:
      case GDK_PAD_STRIP:
      case GDK_PAD_GROUP_MODE:
      case GDK_EVENT_LAST:
        /* no state field */
        break;
      }

  *state = 0;
  return FALSE;
}

/**
 * gdk_event_get_coords:
 * @event: a #GdkEvent
 * @x_win: (out) (optional): location to put event window x coordinate
 * @y_win: (out) (optional): location to put event window y coordinate
 * 
 * Extract the event window relative x/y coordinates from an event.
 * 
 * Returns: %TRUE if the event delivered event window coordinates
 **/
gboolean
gdk_event_get_coords (const GdkEvent *event,
		      gdouble        *x_win,
		      gdouble        *y_win)
{
  gdouble x = 0, y = 0;
  gboolean fetched = TRUE;
  
  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_CONFIGURE:
      x = event->configure.x;
      y = event->configure.y;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      x = event->crossing.x;
      y = event->crossing.y;
      break;
    case GDK_SCROLL:
      x = event->scroll.x;
      y = event->scroll.y;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      x = event->button.x;
      y = event->button.y;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      x = event->touch.x;
      y = event->touch.y;
      break;
    case GDK_MOTION_NOTIFY:
      x = event->motion.x;
      y = event->motion.y;
      break;
    case GDK_TOUCHPAD_SWIPE:
      x = event->touchpad_swipe.x;
      y = event->touchpad_swipe.y;
      break;
    case GDK_TOUCHPAD_PINCH:
      x = event->touchpad_pinch.x;
      y = event->touchpad_pinch.y;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (x_win)
    *x_win = x;
  if (y_win)
    *y_win = y;

  return fetched;
}

/**
 * gdk_event_get_root_coords:
 * @event: a #GdkEvent
 * @x_root: (out) (optional): location to put root window x coordinate
 * @y_root: (out) (optional): location to put root window y coordinate
 * 
 * Extract the root window relative x/y coordinates from an event.
 * 
 * Returns: %TRUE if the event delivered root window coordinates
 **/
gboolean
gdk_event_get_root_coords (const GdkEvent *event,
			   gdouble        *x_root,
			   gdouble        *y_root)
{
  gdouble x = 0, y = 0;
  gboolean fetched = TRUE;
  
  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      x = event->motion.x_root;
      y = event->motion.y_root;
      break;
    case GDK_SCROLL:
      x = event->scroll.x_root;
      y = event->scroll.y_root;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      x = event->button.x_root;
      y = event->button.y_root;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      x = event->touch.x_root;
      y = event->touch.y_root;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      x = event->crossing.x_root;
      y = event->crossing.y_root;
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      x = event->dnd.x_root;
      y = event->dnd.y_root;
      break;
    case GDK_TOUCHPAD_SWIPE:
      x = event->touchpad_swipe.x_root;
      y = event->touchpad_swipe.y_root;
      break;
    case GDK_TOUCHPAD_PINCH:
      x = event->touchpad_pinch.x_root;
      y = event->touchpad_pinch.y_root;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (x_root)
    *x_root = x;
  if (y_root)
    *y_root = y;

  return fetched;
}

/**
 * gdk_event_get_button:
 * @event: a #GdkEvent
 * @button: (out): location to store mouse button number
 *
 * Extract the button number from an event.
 *
 * Returns: %TRUE if the event delivered a button number
 *
 * Since: 3.2
 **/
gboolean
gdk_event_get_button (const GdkEvent *event,
                      guint *button)
{
  gboolean fetched = TRUE;
  guint number = 0;

  g_return_val_if_fail (event != NULL, FALSE);
  
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      number = event->button.button;
      break;
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
      number = event->pad_button.button;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (button)
    *button = number;

  return fetched;
}

/**
 * gdk_event_get_click_count:
 * @event: a #GdkEvent
 * @click_count: (out): location to store click count
 *
 * Extracts the click count from an event.
 *
 * Returns: %TRUE if the event delivered a click count
 *
 * Since: 3.2
 */
gboolean
gdk_event_get_click_count (const GdkEvent *event,
                           guint *click_count)
{
  gboolean fetched = TRUE;
  guint number = 0;

  g_return_val_if_fail (event != NULL, FALSE);

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      number = 1;
      break;
    case GDK_2BUTTON_PRESS:
      number = 2;
      break;
    case GDK_3BUTTON_PRESS:
      number = 3;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (click_count)
    *click_count = number;

  return fetched;
}

/**
 * gdk_event_get_keyval:
 * @event: a #GdkEvent
 * @keyval: (out): location to store the keyval
 *
 * Extracts the keyval from an event.
 *
 * Returns: %TRUE if the event delivered a key symbol
 *
 * Since: 3.2
 */
gboolean
gdk_event_get_keyval (const GdkEvent *event,
                      guint *keyval)
{
  gboolean fetched = TRUE;
  guint number = 0;

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      number = event->key.keyval;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (keyval)
    *keyval = number;

  return fetched;
}

/**
 * gdk_event_get_keycode:
 * @event: a #GdkEvent
 * @keycode: (out): location to store the keycode
 *
 * Extracts the hardware keycode from an event.
 *
 * Also see gdk_event_get_scancode().
 *
 * Returns: %TRUE if the event delivered a hardware keycode
 *
 * Since: 3.2
 */
gboolean
gdk_event_get_keycode (const GdkEvent *event,
                       guint16 *keycode)
{
  gboolean fetched = TRUE;
  guint16 number = 0;

  switch (event->type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      number = event->key.hardware_keycode;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (keycode)
    *keycode = number;

  return fetched;
}

/**
 * gdk_event_get_scroll_direction:
 * @event: a #GdkEvent
 * @direction: (out): location to store the scroll direction
 *
 * Extracts the scroll direction from an event.
 *
 * Returns: %TRUE if the event delivered a scroll direction
 *
 * Since: 3.2
 */
gboolean
gdk_event_get_scroll_direction (const GdkEvent *event,
                                GdkScrollDirection *direction)
{
  gboolean fetched = TRUE;
  GdkScrollDirection dir = 0;

  switch (event->type)
    {
    case GDK_SCROLL:
      if (event->scroll.direction == GDK_SCROLL_SMOOTH)
        fetched = FALSE;
      else
        dir = event->scroll.direction;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (direction)
    *direction = dir;

  return fetched;
}

/**
 * gdk_event_get_scroll_deltas:
 * @event: a #GdkEvent
 * @delta_x: (out): return location for X delta
 * @delta_y: (out): return location for Y delta
 *
 * Retrieves the scroll deltas from a #GdkEvent
 *
 * Returns: %TRUE if the event contains smooth scroll information
 *
 * Since: 3.4
 **/
gboolean
gdk_event_get_scroll_deltas (const GdkEvent *event,
                             gdouble        *delta_x,
                             gdouble        *delta_y)
{
  gboolean fetched = TRUE;
  gdouble dx = 0.0;
  gdouble dy = 0.0;

  switch (event->type)
    {
    case GDK_SCROLL:
      if (event->scroll.direction == GDK_SCROLL_SMOOTH)
        {
          dx = event->scroll.delta_x;
          dy = event->scroll.delta_y;
        }
      else
        fetched = FALSE;
      break;
    default:
      fetched = FALSE;
      break;
    }

  if (delta_x)
    *delta_x = dx;

  if (delta_y)
    *delta_y = dy;

  return fetched;
}

/**
 * gdk_event_is_scroll_stop_event
 * @event: a #GdkEvent
 *
 * Check whether a scroll event is a stop scroll event. Scroll sequences
 * with smooth scroll information may provide a stop scroll event once the
 * interaction with the device finishes, e.g. by lifting a finger. This
 * stop scroll event is the signal that a widget may trigger kinetic
 * scrolling based on the current velocity.
 *
 * Stop scroll events always have a a delta of 0/0.
 *
 * Returns: %TRUE if the event is a scroll stop event
 *
 * Since: 3.20
 */
gboolean
gdk_event_is_scroll_stop_event (const GdkEvent *event)
{
  return event->scroll.is_stop;
}

/**
 * gdk_event_get_axis:
 * @event: a #GdkEvent
 * @axis_use: the axis use to look for
 * @value: (out): location to store the value found
 * 
 * Extract the axis value for a particular axis use from
 * an event structure.
 * 
 * Returns: %TRUE if the specified axis was found, otherwise %FALSE
 **/
gboolean
gdk_event_get_axis (const GdkEvent *event,
		    GdkAxisUse      axis_use,
		    gdouble        *value)
{
  gdouble *axes;
  GdkDevice *device;
  
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (axis_use == GDK_AXIS_X || axis_use == GDK_AXIS_Y)
    {
      gdouble x, y;
      
      switch (event->type)
	{
        case GDK_MOTION_NOTIFY:
	  x = event->motion.x;
	  y = event->motion.y;
	  break;
	case GDK_SCROLL:
	  x = event->scroll.x;
	  y = event->scroll.y;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  x = event->button.x;
	  y = event->button.y;
	  break;
        case GDK_TOUCH_BEGIN:
        case GDK_TOUCH_UPDATE:
        case GDK_TOUCH_END:
        case GDK_TOUCH_CANCEL:
	  x = event->touch.x;
	  y = event->touch.y;
	  break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  x = event->crossing.x;
	  y = event->crossing.y;
	  break;
	  
	default:
	  return FALSE;
	}

      if (axis_use == GDK_AXIS_X && value)
	*value = x;
      if (axis_use == GDK_AXIS_Y && value)
	*value = y;

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS ||
	   event->type == GDK_BUTTON_RELEASE)
    {
      device = event->button.device;
      axes = event->button.axes;
    }
  else if (event->type == GDK_TOUCH_BEGIN ||
           event->type == GDK_TOUCH_UPDATE ||
           event->type == GDK_TOUCH_END ||
           event->type == GDK_TOUCH_CANCEL)
    {
      device = event->touch.device;
      axes = event->touch.axes;
    }
  else if (event->type == GDK_MOTION_NOTIFY)
    {
      device = event->motion.device;
      axes = event->motion.axes;
    }
  else
    return FALSE;

  return gdk_device_get_axis (device, axes, axis_use, value);
}

/**
 * gdk_event_set_device:
 * @event: a #GdkEvent
 * @device: a #GdkDevice
 *
 * Sets the device for @event to @device. The event must
 * have been allocated by GTK+, for instance, by
 * gdk_event_copy().
 *
 * Since: 3.0
 **/
void
gdk_event_set_device (GdkEvent  *event,
                      GdkDevice *device)
{
  GdkEventPrivate *private;

  g_return_if_fail (gdk_event_is_allocated (event));

  private = (GdkEventPrivate *) event;

  g_set_object (&private->device, device);

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.device = device;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.device = device;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      event->touch.device = device;
      break;
    case GDK_SCROLL:
      event->scroll.device = device;
      break;
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      event->proximity.device = device;
      break;
    default:
      break;
    }
}

/**
 * gdk_event_get_device:
 * @event: a #GdkEvent.
 *
 * If the event contains a “device” field, this function will return
 * it, else it will return %NULL.
 *
 * Returns: (nullable) (transfer none): a #GdkDevice, or %NULL.
 *
 * Since: 3.0
 **/
GdkDevice *
gdk_event_get_device (const GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);

  if (gdk_event_is_allocated (event))
    {
      GdkEventPrivate *private = (GdkEventPrivate *) event;

      if (private->device)
        return private->device;
    }

  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      return event->motion.device;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return event->button.device;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      return event->touch.device;
    case GDK_SCROLL:
      return event->scroll.device;
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      return event->proximity.device;
    default:
      break;
    }

  /* Fallback if event has no device set */
  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_FOCUS_CHANGE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
    case GDK_SCROLL:
    case GDK_GRAB_BROKEN:
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      {
        GdkDisplay *display;
        GdkSeat *seat;

        g_warning ("Event with type %d not holding a GdkDevice. "
                   "It is most likely synthesized outside Gdk/GTK+",
                   event->type);

        display = gdk_window_get_display (event->any.window);
        seat = gdk_display_get_default_seat (display);

        if (event->type == GDK_KEY_PRESS ||
            event->type == GDK_KEY_RELEASE)
          return gdk_seat_get_keyboard (seat);
        else
          return gdk_seat_get_pointer (seat);
      }
      break;
    default:
      return NULL;
    }
}

/**
 * gdk_event_set_source_device:
 * @event: a #GdkEvent
 * @device: a #GdkDevice
 *
 * Sets the slave device for @event to @device.
 *
 * The event must have been allocated by GTK+,
 * for instance by gdk_event_copy().
 *
 * Since: 3.0
 **/
void
gdk_event_set_source_device (GdkEvent  *event,
                             GdkDevice *device)
{
  GdkEventPrivate *private;

  g_return_if_fail (gdk_event_is_allocated (event));
  g_return_if_fail (GDK_IS_DEVICE (device));

  private = (GdkEventPrivate *) event;

  g_set_object (&private->source_device, device);
}

/**
 * gdk_event_get_source_device:
 * @event: a #GdkEvent
 *
 * This function returns the hardware (slave) #GdkDevice that has
 * triggered the event, falling back to the virtual (master) device
 * (as in gdk_event_get_device()) if the event wasn’t caused by
 * interaction with a hardware device. This may happen for example
 * in synthesized crossing events after a #GdkWindow updates its
 * geometry or a grab is acquired/released.
 *
 * If the event does not contain a device field, this function will
 * return %NULL.
 *
 * Returns: (nullable) (transfer none): a #GdkDevice, or %NULL.
 *
 * Since: 3.0
 **/
GdkDevice *
gdk_event_get_source_device (const GdkEvent *event)
{
  GdkEventPrivate *private;

  g_return_val_if_fail (event != NULL, NULL);

  if (!gdk_event_is_allocated (event))
    return NULL;

  private = (GdkEventPrivate *) event;

  if (private->source_device)
    return private->source_device;

  /* Fallback to event device */
  return gdk_event_get_device (event);
}

/**
 * gdk_event_request_motions:
 * @event: a valid #GdkEvent
 *
 * Request more motion notifies if @event is a motion notify hint event.
 *
 * This function should be used instead of gdk_window_get_pointer() to
 * request further motion notifies, because it also works for extension
 * events where motion notifies are provided for devices other than the
 * core pointer. Coordinate extraction, processing and requesting more
 * motion events from a %GDK_MOTION_NOTIFY event usually works like this:
 *
 * |[<!-- language="C" -->
 * {
 *   // motion_event handler
 *   x = motion_event->x;
 *   y = motion_event->y;
 *   // handle (x,y) motion
 *   gdk_event_request_motions (motion_event); // handles is_hint events
 * }
 * ]|
 *
 * Since: 2.12
 **/
void
gdk_event_request_motions (const GdkEventMotion *event)
{
  GdkDisplay *display;
  
  g_return_if_fail (event != NULL);
  
  if (event->type == GDK_MOTION_NOTIFY && event->is_hint)
    {
      gdk_device_get_state (event->device, event->window, NULL, NULL);
      
      display = gdk_window_get_display (event->window);
      _gdk_display_enable_motion_hints (display, event->device);
    }
}

/**
 * gdk_event_triggers_context_menu:
 * @event: a #GdkEvent, currently only button events are meaningful values
 *
 * This function returns whether a #GdkEventButton should trigger a
 * context menu, according to platform conventions. The right mouse
 * button always triggers context menus. Additionally, if
 * gdk_keymap_get_modifier_mask() returns a non-0 mask for
 * %GDK_MODIFIER_INTENT_CONTEXT_MENU, then the left mouse button will
 * also trigger a context menu if this modifier is pressed.
 *
 * This function should always be used instead of simply checking for
 * event->button == %GDK_BUTTON_SECONDARY.
 *
 * Returns: %TRUE if the event should trigger a context menu.
 *
 * Since: 3.4
 **/
gboolean
gdk_event_triggers_context_menu (const GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->type == GDK_BUTTON_PRESS)
    {
      const GdkEventButton *bevent = (const GdkEventButton *) event;
      GdkDisplay *display;
      GdkModifierType modifier;

      g_return_val_if_fail (GDK_IS_WINDOW (bevent->window), FALSE);

      if (bevent->button == GDK_BUTTON_SECONDARY &&
          ! (bevent->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)))
        return TRUE;

      display = gdk_window_get_display (bevent->window);

      modifier = gdk_keymap_get_modifier_mask (gdk_keymap_get_for_display (display),
                                               GDK_MODIFIER_INTENT_CONTEXT_MENU);

      if (modifier != 0 &&
          bevent->button == GDK_BUTTON_PRIMARY &&
          ! (bevent->state & (GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) &&
          (bevent->state & modifier))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gdk_events_get_axis_distances (GdkEvent *event1,
                               GdkEvent *event2,
                               gdouble  *x_distance,
                               gdouble  *y_distance,
                               gdouble  *distance)
{
  gdouble x1, x2, y1, y2;
  gdouble xd, yd;

  if (!gdk_event_get_coords (event1, &x1, &y1) ||
      !gdk_event_get_coords (event2, &x2, &y2))
    return FALSE;

  xd = x2 - x1;
  yd = y2 - y1;

  if (x_distance)
    *x_distance = xd;

  if (y_distance)
    *y_distance = yd;

  if (distance)
    *distance = sqrt ((xd * xd) + (yd * yd));

  return TRUE;
}

/**
 * gdk_events_get_distance:
 * @event1: first #GdkEvent
 * @event2: second #GdkEvent
 * @distance: (out): return location for the distance
 *
 * If both events have X/Y information, the distance between both coordinates
 * (as in a straight line going from @event1 to @event2) will be returned.
 *
 * Returns: %TRUE if the distance could be calculated.
 *
 * Since: 3.0
 **/
gboolean
gdk_events_get_distance (GdkEvent *event1,
                         GdkEvent *event2,
                         gdouble  *distance)
{
  return gdk_events_get_axis_distances (event1, event2,
                                        NULL, NULL,
                                        distance);
}

/**
 * gdk_events_get_angle:
 * @event1: first #GdkEvent
 * @event2: second #GdkEvent
 * @angle: (out): return location for the relative angle between both events
 *
 * If both events contain X/Y information, this function will return %TRUE
 * and return in @angle the relative angle from @event1 to @event2. The rotation
 * direction for positive angles is from the positive X axis towards the positive
 * Y axis.
 *
 * Returns: %TRUE if the angle could be calculated.
 *
 * Since: 3.0
 **/
gboolean
gdk_events_get_angle (GdkEvent *event1,
                      GdkEvent *event2,
                      gdouble  *angle)
{
  gdouble x_distance, y_distance, distance;

  if (!gdk_events_get_axis_distances (event1, event2,
                                      &x_distance, &y_distance,
                                      &distance))
    return FALSE;

  if (angle)
    {
      *angle = atan2 (x_distance, y_distance);

      /* Invert angle */
      *angle = (2 * G_PI) - *angle;

      /* Shift it 90° */
      *angle += G_PI / 2;

      /* And constraint it to 0°-360° */
      *angle = fmod (*angle, 2 * G_PI);
    }

  return TRUE;
}

/**
 * gdk_events_get_center:
 * @event1: first #GdkEvent
 * @event2: second #GdkEvent
 * @x: (out): return location for the X coordinate of the center
 * @y: (out): return location for the Y coordinate of the center
 *
 * If both events contain X/Y information, the center of both coordinates
 * will be returned in @x and @y.
 *
 * Returns: %TRUE if the center could be calculated.
 *
 * Since: 3.0
 **/
gboolean
gdk_events_get_center (GdkEvent *event1,
                       GdkEvent *event2,
                       gdouble  *x,
                       gdouble  *y)
{
  gdouble x1, x2, y1, y2;

  if (!gdk_event_get_coords (event1, &x1, &y1) ||
      !gdk_event_get_coords (event2, &x2, &y2))
    return FALSE;

  if (x)
    *x = (x2 + x1) / 2;

  if (y)
    *y = (y2 + y1) / 2;

  return TRUE;
}

/**
 * gdk_event_set_screen:
 * @event: a #GdkEvent
 * @screen: a #GdkScreen
 * 
 * Sets the screen for @event to @screen. The event must
 * have been allocated by GTK+, for instance, by
 * gdk_event_copy().
 *
 * Since: 2.2
 **/
void
gdk_event_set_screen (GdkEvent  *event,
		      GdkScreen *screen)
{
  GdkEventPrivate *private;
  
  g_return_if_fail (gdk_event_is_allocated (event));

  private = (GdkEventPrivate *)event;
  
  private->screen = screen;
}

/**
 * gdk_event_get_screen:
 * @event: a #GdkEvent
 * 
 * Returns the screen for the event. The screen is
 * typically the screen for `event->any.window`, but
 * for events such as mouse events, it is the screen
 * where the pointer was when the event occurs -
 * that is, the screen which has the root window 
 * to which `event->motion.x_root` and
 * `event->motion.y_root` are relative.
 * 
 * Returns: (transfer none): the screen for the event
 *
 * Since: 2.2
 **/
GdkScreen *
gdk_event_get_screen (const GdkEvent *event)
{
  if (gdk_event_is_allocated (event))
    {
      GdkEventPrivate *private = (GdkEventPrivate *)event;

      if (private->screen)
	return private->screen;
    }

  if (event->any.window)
    return gdk_window_get_screen (event->any.window);

  return NULL;
}

/**
 * gdk_event_get_event_sequence:
 * @event: a #GdkEvent
 *
 * If @event if of type %GDK_TOUCH_BEGIN, %GDK_TOUCH_UPDATE,
 * %GDK_TOUCH_END or %GDK_TOUCH_CANCEL, returns the #GdkEventSequence
 * to which the event belongs. Otherwise, return %NULL.
 *
 * Returns: (transfer none): the event sequence that the event belongs to
 *
 * Since: 3.4
 */
GdkEventSequence *
gdk_event_get_event_sequence (const GdkEvent *event)
{
  if (!event)
    return NULL;

  if (event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_TOUCH_UPDATE ||
      event->type == GDK_TOUCH_END ||
      event->type == GDK_TOUCH_CANCEL)
    return event->touch.sequence;

  return NULL;
}

/**
 * gdk_set_show_events:
 * @show_events:  %TRUE to output event debugging information.
 * 
 * Sets whether a trace of received events is output.
 * Note that GTK+ must be compiled with debugging (that is,
 * configured using the `--enable-debug` option)
 * to use this option.
 **/
void
gdk_set_show_events (gboolean show_events)
{
  if (show_events)
    _gdk_debug_flags |= GDK_DEBUG_EVENTS;
  else
    _gdk_debug_flags &= ~GDK_DEBUG_EVENTS;
}

/**
 * gdk_get_show_events:
 * 
 * Gets whether event debugging output is enabled.
 * 
 * Returns: %TRUE if event debugging output is enabled.
 **/
gboolean
gdk_get_show_events (void)
{
  return (_gdk_debug_flags & GDK_DEBUG_EVENTS) != 0;
}

static void
gdk_synthesize_click (GdkDisplay *display,
                      GdkEvent   *event,
                      gint        nclicks)
{
  GdkEvent *event_copy;

  event_copy = gdk_event_copy (event);
  event_copy->type = (nclicks == 2) ? GDK_2BUTTON_PRESS : GDK_3BUTTON_PRESS;

  _gdk_event_queue_append (display, event_copy);
}

void
_gdk_event_button_generate (GdkDisplay *display,
			    GdkEvent   *event)
{
  GdkMultipleClickInfo *info;
  GdkDevice *source_device;

  g_return_if_fail (event->type == GDK_BUTTON_PRESS);

  source_device = gdk_event_get_source_device (event);
  info = g_hash_table_lookup (display->multiple_click_info, event->button.device);

  if (G_UNLIKELY (!info))
    {
      info = g_new0 (GdkMultipleClickInfo, 1);
      info->button_number[0] = info->button_number[1] = -1;

      g_hash_table_insert (display->multiple_click_info,
                           event->button.device, info);
    }

  if ((event->button.time < (info->button_click_time[1] + 2 * display->double_click_time)) &&
      (event->button.window == info->button_window[1]) &&
      (event->button.button == info->button_number[1]) &&
      (source_device == info->last_slave) &&
      (ABS (event->button.x - info->button_x[1]) <= display->double_click_distance) &&
      (ABS (event->button.y - info->button_y[1]) <= display->double_click_distance))
    {
      gdk_synthesize_click (display, event, 3);

      info->button_click_time[1] = 0;
      info->button_click_time[0] = 0;
      info->button_window[1] = NULL;
      info->button_window[0] = NULL;
      info->button_number[1] = -1;
      info->button_number[0] = -1;
      info->button_x[0] = info->button_x[1] = 0;
      info->button_y[0] = info->button_y[1] = 0;
      info->last_slave = NULL;
    }
  else if ((event->button.time < (info->button_click_time[0] + display->double_click_time)) &&
	   (event->button.window == info->button_window[0]) &&
	   (event->button.button == info->button_number[0]) &&
           (source_device == info->last_slave) &&
	   (ABS (event->button.x - info->button_x[0]) <= display->double_click_distance) &&
	   (ABS (event->button.y - info->button_y[0]) <= display->double_click_distance))
    {
      gdk_synthesize_click (display, event, 2);
      
      info->button_click_time[1] = info->button_click_time[0];
      info->button_click_time[0] = event->button.time;
      info->button_window[1] = info->button_window[0];
      info->button_window[0] = event->button.window;
      info->button_number[1] = info->button_number[0];
      info->button_number[0] = event->button.button;
      info->button_x[1] = info->button_x[0];
      info->button_x[0] = event->button.x;
      info->button_y[1] = info->button_y[0];
      info->button_y[0] = event->button.y;
      info->last_slave = source_device;
    }
  else
    {
      info->button_click_time[1] = 0;
      info->button_click_time[0] = event->button.time;
      info->button_window[1] = NULL;
      info->button_window[0] = event->button.window;
      info->button_number[1] = -1;
      info->button_number[0] = event->button.button;
      info->button_x[1] = 0;
      info->button_x[0] = event->button.x;
      info->button_y[1] = 0;
      info->button_y[0] = event->button.y;
      info->last_slave = source_device;
    }
}

static GList *
gdk_get_pending_window_state_event_link (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GList *tmp_list;

  for (tmp_list = display->queued_events; tmp_list; tmp_list = tmp_list->next)
    {
      GdkEventPrivate *event = tmp_list->data;

      if (event->event.type == GDK_WINDOW_STATE &&
          event->event.window_state.window == window)
        return tmp_list;
    }

  return NULL;
}

void
_gdk_set_window_state (GdkWindow      *window,
                       GdkWindowState  new_state)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkEvent temp_event;
  GdkWindowState old;
  GList *pending_event_link;

  g_return_if_fail (window != NULL);

  temp_event.window_state.window = window;
  temp_event.window_state.type = GDK_WINDOW_STATE;
  temp_event.window_state.send_event = FALSE;
  temp_event.window_state.new_window_state = new_state;

  if (temp_event.window_state.new_window_state == window->state)
    return; /* No actual work to do, nothing changed. */

  pending_event_link = gdk_get_pending_window_state_event_link (window);
  if (pending_event_link)
    {
      old = window->old_state;
      _gdk_event_queue_remove_link (display, pending_event_link);
      gdk_event_free (pending_event_link->data);
      g_list_free_1 (pending_event_link);
    }
  else
    {
      old = window->state;
      window->old_state = old;
    }

  temp_event.window_state.changed_mask = new_state ^ old;

  /* Actually update the field in GdkWindow, this is sort of an odd
   * place to do it, but seems like the safest since it ensures we expose no
   * inconsistent state to the user.
   */

  window->state = new_state;

  if (temp_event.window_state.changed_mask & GDK_WINDOW_STATE_WITHDRAWN)
    _gdk_window_update_viewable (window);

  /* We only really send the event to toplevels, since
   * all the window states don't apply to non-toplevels.
   * Non-toplevels do use the GDK_WINDOW_STATE_WITHDRAWN flag
   * internally so we needed to update window->state.
   */
  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP: /* ? */
      gdk_display_put_event (display, &temp_event);
      break;
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_CHILD:
      break;
    }
}

void
gdk_synthesize_window_state (GdkWindow     *window,
                             GdkWindowState unset_flags,
                             GdkWindowState set_flags)
{
  g_return_if_fail (window != NULL);

  _gdk_set_window_state (window, (window->state | set_flags) & ~unset_flags);
}

/**
 * gdk_display_set_double_click_time:
 * @display: a #GdkDisplay
 * @msec: double click time in milliseconds (thousandths of a second) 
 * 
 * Sets the double click time (two clicks within this time interval
 * count as a double click and result in a #GDK_2BUTTON_PRESS event).
 * Applications should not set this, it is a global 
 * user-configured setting.
 *
 * Since: 2.2
 **/
void
gdk_display_set_double_click_time (GdkDisplay *display,
				   guint       msec)
{
  display->double_click_time = msec;
}

/**
 * gdk_set_double_click_time:
 * @msec: double click time in milliseconds (thousandths of a second)
 *
 * Set the double click time for the default display. See
 * gdk_display_set_double_click_time(). 
 * See also gdk_display_set_double_click_distance().
 * Applications should not set this, it is a 
 * global user-configured setting.
 **/
void
gdk_set_double_click_time (guint msec)
{
  gdk_display_set_double_click_time (gdk_display_get_default (), msec);
}

/**
 * gdk_display_set_double_click_distance:
 * @display: a #GdkDisplay
 * @distance: distance in pixels
 * 
 * Sets the double click distance (two clicks within this distance
 * count as a double click and result in a #GDK_2BUTTON_PRESS event).
 * See also gdk_display_set_double_click_time().
 * Applications should not set this, it is a global 
 * user-configured setting.
 *
 * Since: 2.4
 **/
void
gdk_display_set_double_click_distance (GdkDisplay *display,
				       guint       distance)
{
  display->double_click_distance = distance;
}

G_DEFINE_BOXED_TYPE (GdkEvent, gdk_event,
                     gdk_event_copy,
                     gdk_event_free)

static GdkEventSequence *
gdk_event_sequence_copy (GdkEventSequence *sequence)
{
  return sequence;
}

static void
gdk_event_sequence_free (GdkEventSequence *sequence)
{
  /* Nothing to free here */
}

G_DEFINE_BOXED_TYPE (GdkEventSequence, gdk_event_sequence,
                     gdk_event_sequence_copy,
                     gdk_event_sequence_free)

/**
 * gdk_setting_get:
 * @name: the name of the setting.
 * @value: location to store the value of the setting.
 *
 * Obtains a desktop-wide setting, such as the double-click time,
 * for the default screen. See gdk_screen_get_setting().
 *
 * Returns: %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise.
 **/
gboolean
gdk_setting_get (const gchar *name,
		 GValue      *value)
{
  return gdk_screen_get_setting (gdk_screen_get_default (), name, value);
}

/**
 * gdk_event_get_event_type:
 * @event: a #GdkEvent
 *
 * Retrieves the type of the event.
 *
 * Returns: a #GdkEventType
 *
 * Since: 3.10
 */
GdkEventType
gdk_event_get_event_type (const GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, GDK_NOTHING);

  return event->type;
}

/**
 * gdk_event_get_seat:
 * @event: a #GdkEvent
 *
 * Returns the #GdkSeat this event was generated for.
 *
 * Returns: (transfer none): The #GdkSeat of this event
 *
 * Since: 3.20
 **/
GdkSeat *
gdk_event_get_seat (const GdkEvent *event)
{
  const GdkEventPrivate *priv;

  if (!gdk_event_is_allocated (event))
    return NULL;

  priv = (const GdkEventPrivate *) event;

  if (!priv->seat)
    {
      GdkDevice *device;

      g_warning ("Event with type %d not holding a GdkSeat. "
                 "It is most likely synthesized outside Gdk/GTK+",
                 event->type);

      device = gdk_event_get_device (event);

      return device ? gdk_device_get_seat (device) : NULL;
    }

  return priv->seat;
}

void
gdk_event_set_seat (GdkEvent *event,
                    GdkSeat  *seat)
{
  GdkEventPrivate *priv;

  if (gdk_event_is_allocated (event))
    {
      priv = (GdkEventPrivate *) event;
      priv->seat = seat;
    }
}

/**
 * gdk_event_get_device_tool:
 * @event: a #GdkEvent
 *
 * If the event was generated by a device that supports
 * different tools (eg. a tablet), this function will
 * return a #GdkDeviceTool representing the tool that
 * caused the event. Otherwise, %NULL will be returned.
 *
 * Note: the #GdkDeviceTool<!-- -->s will be constant during
 * the application lifetime, if settings must be stored
 * persistently across runs, see gdk_device_tool_get_serial()
 *
 * Returns: (transfer none): The current device tool, or %NULL
 *
 * Since: 3.22
 **/
GdkDeviceTool *
gdk_event_get_device_tool (const GdkEvent *event)
{
  GdkEventPrivate *private;

  if (!gdk_event_is_allocated (event))
    return NULL;

  private = (GdkEventPrivate *) event;
  return private->tool;
}

/**
 * gdk_event_set_device_tool:
 * @event: a #GdkEvent
 * @tool: (nullable): tool to set on the event, or %NULL
 *
 * Sets the device tool for this event, should be rarely used.
 *
 * Since: 3.22
 **/
void
gdk_event_set_device_tool (GdkEvent      *event,
                           GdkDeviceTool *tool)
{
  GdkEventPrivate *private;

  if (!gdk_event_is_allocated (event))
    return;

  private = (GdkEventPrivate *) event;
  private->tool = tool;
}

void
gdk_event_set_scancode (GdkEvent *event,
                        guint16 scancode)
{
  GdkEventPrivate *private = (GdkEventPrivate *) event;

  private->key_scancode = scancode;
}

/**
 * gdk_event_get_scancode:
 * @event: a #GdkEvent
 *
 * Gets the keyboard low-level scancode of a key event.
 *
 * This is usually hardware_keycode. On Windows this is the high
 * word of WM_KEY{DOWN,UP} lParam which contains the scancode and
 * some extended flags.
 *
 * Returns: The associated keyboard scancode or 0
 *
 * Since: 3.22
 **/
int
gdk_event_get_scancode (GdkEvent *event)
{
  GdkEventPrivate *private;

  if (!gdk_event_is_allocated (event))
    return 0;

  private = (GdkEventPrivate *) event;
  return private->key_scancode;
}
