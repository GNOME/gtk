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

#include "gdkintl.h"
#include "gdkeventsprivate.h"
#include "gdkinternals.h"
#include "gdkdisplayprivate.h"
#include "gdkdndprivate.h"
#include "gdk-private.h"

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

static GQuark quark_event_user_data = 0;

static void gdk_event_finalize (GObject *object);

G_DEFINE_TYPE (GdkEvent, gdk_event, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_EVENT_TYPE,
  N_PROPS
};

static GParamSpec *event_props[N_PROPS] = { NULL, };

#define EVENT_PAYLOAD(ev) (&(ev)->any.type)
#define EVENT_PAYLOAD_SIZE (sizeof (GdkEvent) - sizeof (GObject))

static void
gdk_event_init (GdkEvent *event)
{
}

static void
gdk_event_real_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdkEvent *event = GDK_EVENT (object);

  switch (prop_id)
    {
    case PROP_EVENT_TYPE:
      g_value_set_enum (value, event->any.type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_event_real_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdkEvent *event = GDK_EVENT (object);

  switch (prop_id)
    {
    case PROP_EVENT_TYPE:
      event->any.type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_event_class_init (GdkEventClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gdk_event_real_get_property;
  object_class->set_property = gdk_event_real_set_property;
  object_class->finalize = gdk_event_finalize;

  event_props[PROP_EVENT_TYPE] =
    g_param_spec_enum ("event-type",
                       P_("Event type"),
                       P_("Event type"),
                       GDK_TYPE_EVENT_TYPE, GDK_NOTHING,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_properties (object_class, N_PROPS, event_props);

  quark_event_user_data = g_quark_from_static_string ("gdk-event-user-data");
}

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
      GdkEvent *event = tmp_list->data;

      if ((event->any.flags & GDK_EVENT_PENDING) == 0 &&
	  (!paused || (event->any.flags & GDK_EVENT_FLUSHED) != 0))
        {
          if (pending_motion)
            return pending_motion;

          if (event->any.type == GDK_MOTION_NOTIFY && (event->any.flags & GDK_EVENT_FLUSHED) == 0)
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
      GdkEvent *event = tmp_list->data;

      if (event->any.flags & GDK_EVENT_PENDING)
        break;

      if (event->any.type != GDK_MOTION_NOTIFY)
        break;

      if (pending_motion_window != NULL &&
          pending_motion_window != event->any.window)
        break;

      if (pending_motion_device != NULL &&
          pending_motion_device != event->any.device)
        break;

      if (!event->any.window->event_compression)
        break;

      pending_motion_window = event->any.window;
      pending_motion_device = event->any.device;
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
      GdkEvent *event = tmp_list->data;
      event->any.flags |= GDK_EVENT_FLUSHED;
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
  GdkEvent *new_event;

  new_event = g_object_new (GDK_TYPE_EVENT,
                            "event-type", type,
                            NULL);

  /*
   * Bytewise 0 initialization is reasonable for most of the 
   * current event types. Explicitely initialize double fields
   * since I trust bytewise 0 == 0. less than for integers
   * or pointers.
   */
  switch ((guint) type)
    {
    case GDK_MOTION_NOTIFY:
      new_event->motion.x = 0.;
      new_event->motion.y = 0.;
      new_event->motion.x_root = 0.;
      new_event->motion.y_root = 0.;
      break;
    case GDK_BUTTON_PRESS:
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

void
gdk_event_set_pointer_emulated (GdkEvent *event,
                                gboolean  emulated)
{
  if (emulated)
    event->any.flags |= GDK_EVENT_POINTER_EMULATED;
  else
    event->any.flags &= ~(GDK_EVENT_POINTER_EMULATED);
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
  return (event->any.flags & GDK_EVENT_POINTER_EMULATED) != 0;
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
  GdkEvent *new_event;

  g_return_val_if_fail (event != NULL, NULL);

  new_event = gdk_event_new (event->any.type);

  memcpy (EVENT_PAYLOAD (new_event),
          EVENT_PAYLOAD (event),
          EVENT_PAYLOAD_SIZE);

  if (new_event->any.window)
    g_object_ref (new_event->any.window);
  if (new_event->any.device)
    g_object_ref (new_event->any.device);
  if (new_event->any.source_device)
    g_object_ref (new_event->any.source_device);

  gdk_event_set_user_data (new_event, gdk_event_get_user_data (event));

  switch ((guint) event->any.type)
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

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      if (event->button.axes)
        new_event->button.axes = g_memdup (event->button.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (event->any.device));
      break;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      if (event->touch.axes)
        new_event->touch.axes = g_memdup (event->touch.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (event->any.device));
      break;

    case GDK_MOTION_NOTIFY:
      if (event->motion.axes)
        new_event->motion.axes = g_memdup (event->motion.axes,
                                           sizeof (gdouble) * gdk_device_get_n_axes (event->any.device));
      break;

    default:
      break;
    }

  _gdk_display_event_data_copy (gdk_event_get_display (event), event, new_event);

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
  g_object_unref (event);
}

void
gdk_event_finalize (GObject *object)
{
  GdkEvent *event = GDK_EVENT (object);
  GdkDisplay *display;

  switch ((guint) event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      g_free (event->key.string);
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      g_clear_object (&event->crossing.subwindow);
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
    case GDK_BUTTON_RELEASE:
      g_clear_object (&event->button.tool);
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
      g_clear_object (&event->motion.tool);
      g_free (event->motion.axes);
      break;

    default:
      break;
    }

  display = gdk_event_get_display (event);
  if (display)
    _gdk_display_event_data_free (display, event);

  if (event->any.window)
    g_object_unref (event->any.window);

  g_clear_object (&event->any.device);
  g_clear_object (&event->any.source_device);

  G_OBJECT_CLASS (gdk_event_parent_class)->finalize (object);
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
    switch (event->any.type)
      {
      case GDK_MOTION_NOTIFY:
	return event->motion.time;
      case GDK_BUTTON_PRESS:
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
      case GDK_GRAB_BROKEN:
      case GDK_EVENT_LAST:
      default:
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
    switch (event->any.type)
      {
      case GDK_MOTION_NOTIFY:
	*state = event->motion.state;
        return TRUE;
      case GDK_BUTTON_PRESS:
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
      case GDK_CLIENT_EVENT:
      case GDK_CONFIGURE:
      case GDK_FOCUS_CHANGE:
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
      case GDK_GRAB_BROKEN:
      case GDK_PAD_BUTTON_PRESS:
      case GDK_PAD_BUTTON_RELEASE:
      case GDK_PAD_RING:
      case GDK_PAD_STRIP:
      case GDK_PAD_GROUP_MODE:
      case GDK_EVENT_LAST:
      default:
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

  switch ((guint) event->any.type)
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

  switch ((guint) event->any.type)
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

void
gdk_event_set_coords (GdkEvent *event,
                      gdouble   x,
                      gdouble   y)
{
  g_return_if_fail (event != NULL);

  switch ((guint) event->any.type)
    {
    case GDK_CONFIGURE:
      event->configure.x = x;
      event->configure.y = y;
      break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event->crossing.x = x;
      event->crossing.y = y;
      break;
    case GDK_SCROLL:
      event->scroll.x = x;
      event->scroll.y = y;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.x = x;
      event->button.y = y;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      event->touch.x = x;
      event->touch.y = y;
      break;
    case GDK_MOTION_NOTIFY:
      event->motion.x = x;
      event->motion.y = y;
      break;
    case GDK_TOUCHPAD_SWIPE:
      event->touchpad_swipe.x = x;
      event->touchpad_swipe.y = y;
      break;
    case GDK_TOUCHPAD_PINCH:
      event->touchpad_pinch.x = x;
      event->touchpad_pinch.y = y;
      break;
    default:
      break;
    }
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
  
  switch ((guint) event->any.type)
    {
    case GDK_BUTTON_PRESS:
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

  switch ((guint) event->any.type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      number = 1;
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

  switch ((guint) event->any.type)
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

void
gdk_event_set_keyval (GdkEvent *event,
                      guint     keyval)
{
  if (event->any.type == GDK_KEY_PRESS ||
      event->any.type == GDK_KEY_RELEASE)
    event->key.keyval = keyval;
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

  switch ((guint) event->any.type)
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
 * gdk_event_get_key_group:
 * @event: a #GdkEvent
 * @group: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_key_group (const GdkEvent *event,
                         guint          *group)
{
  gboolean fetched = TRUE;

  switch ((guint) event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      *group = event->key.group;
      break;
    default:
      *group = 0;
      fetched = FALSE;
      break;
    }

  return fetched;
}

/**
 * gdk_event_get_string:
 * @event: a #GdkEvent
 * @string: (out) (transfer none):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_string (const GdkEvent  *event,
                      const char     **string)
{
  gboolean fetched = TRUE;

  switch ((guint) event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      *string = event->key.string;
      break;
    default:
      *string = NULL;
      fetched = FALSE;
      break;
    }

  return fetched;
}

/**
 * gdk_event_get_key_is_modifier:
 * @event: a #GdkEvent
 * @is_modifier: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_key_is_modifier (const GdkEvent *event,
                               gboolean       *is_modifier)
{
  gboolean fetched = TRUE;

  switch ((guint) event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      *is_modifier = event->key.is_modifier;
      break;
    default:
      *is_modifier = FALSE;
      fetched = FALSE;
      break;
    }

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

  switch ((guint) event->any.type)
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

  switch ((guint) event->any.type)
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

  g_return_val_if_fail (event != NULL, FALSE);
  
  if (axis_use == GDK_AXIS_X || axis_use == GDK_AXIS_Y)
    {
      gdouble x, y;
      
      switch ((guint) event->any.type)
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
  else if (event->any.type == GDK_BUTTON_PRESS ||
	   event->any.type == GDK_BUTTON_RELEASE)
    {
      axes = event->button.axes;
    }
  else if (event->any.type == GDK_TOUCH_BEGIN ||
           event->any.type == GDK_TOUCH_UPDATE ||
           event->any.type == GDK_TOUCH_END ||
           event->any.type == GDK_TOUCH_CANCEL)
    {
      axes = event->touch.axes;
    }
  else if (event->any.type == GDK_MOTION_NOTIFY)
    {
      axes = event->motion.axes;
    }
  else
    return FALSE;

  return gdk_device_get_axis (event->any.device, axes, axis_use, value);
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
  g_set_object (&event->any.device, device);
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

  return event->any.device;
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
  g_set_object (&event->any.source_device, device);
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
  g_return_val_if_fail (event != NULL, NULL);

  if (event->any.source_device)
    return event->any.source_device;

  /* Fallback to event device */
  return gdk_event_get_device (event);
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

  if (event->any.type == GDK_BUTTON_PRESS)
    {
      const GdkEventButton *bevent = (const GdkEventButton *) event;
      GdkDisplay *display;
      GdkModifierType modifier;

      g_return_val_if_fail (GDK_IS_WINDOW (bevent->any.window), FALSE);

      if (bevent->button == GDK_BUTTON_SECONDARY &&
          ! (bevent->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)))
        return TRUE;

      display = gdk_window_get_display (bevent->any.window);

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

void
gdk_event_set_display (GdkEvent   *event,
                       GdkDisplay *display)
{
  event->any.display = display;
}

GdkDisplay *
gdk_event_get_display (const GdkEvent *event)
{
  if (event->any.display)
    return event->any.display;

  if (event->any.window)
    return gdk_window_get_display (event->any.window);

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

  if (event->any.type == GDK_TOUCH_BEGIN ||
      event->any.type == GDK_TOUCH_UPDATE ||
      event->any.type == GDK_TOUCH_END ||
      event->any.type == GDK_TOUCH_CANCEL)
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

static GList *
gdk_get_pending_window_state_event_link (GdkWindow *window)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GList *tmp_list;

  for (tmp_list = display->queued_events; tmp_list; tmp_list = tmp_list->next)
    {
      GdkEvent *event = tmp_list->data;

      if (event->any.type == GDK_WINDOW_STATE &&
          event->any.window == window)
        return tmp_list;
    }

  return NULL;
}

void
_gdk_set_window_state (GdkWindow      *window,
                       GdkWindowState  new_state)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkEvent *temp_event;
  GdkWindowState old;
  GList *pending_event_link;

  g_return_if_fail (window != NULL);

  if (new_state == window->state)
    return; /* No actual work to do, nothing changed. */

  temp_event = gdk_event_new (GDK_WINDOW_STATE);

  temp_event->any.window = g_object_ref (window);
  temp_event->any.send_event = FALSE;
  temp_event->window_state.new_window_state = new_state;

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

  temp_event->window_state.changed_mask = new_state ^ old;

  /* Actually update the field in GdkWindow, this is sort of an odd
   * place to do it, but seems like the safest since it ensures we expose no
   * inconsistent state to the user.
   */

  window->state = new_state;

  if (temp_event->window_state.changed_mask & GDK_WINDOW_STATE_WITHDRAWN)
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
      gdk_display_put_event (display, temp_event);
      break;
    case GDK_WINDOW_FOREIGN:
    case GDK_WINDOW_ROOT:
    case GDK_WINDOW_CHILD:
    default:
      break;
    }

  gdk_event_free (temp_event);
}

void
gdk_synthesize_window_state (GdkWindow     *window,
                             GdkWindowState unset_flags,
                             GdkWindowState set_flags)
{
  g_return_if_fail (window != NULL);

  _gdk_set_window_state (window, (window->state | set_flags) & ~unset_flags);
}

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

  return event->any.type;
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
  GdkDevice *device;

  device = gdk_event_get_device (event);

  return device ? gdk_device_get_seat (device) : NULL;
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
  if (event->any.type == GDK_BUTTON_PRESS ||
      event->any.type == GDK_BUTTON_RELEASE)
    return event->button.tool;
  else if (event->any.type == GDK_MOTION_NOTIFY)
    return event->motion.tool;

  return NULL;
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
  if (event->any.type == GDK_BUTTON_PRESS ||
      event->any.type == GDK_BUTTON_RELEASE)
    event->button.tool = tool;
  else if (event->any.type == GDK_MOTION_NOTIFY)
    event->motion.tool = tool;
}

void
gdk_event_set_scancode (GdkEvent *event,
                        guint16 scancode)
{
  if (event->any.type == GDK_KEY_PRESS ||
      event->any.type == GDK_KEY_RELEASE)
    event->key.key_scancode = scancode;
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
  if (event->any.type == GDK_KEY_PRESS ||
      event->any.type == GDK_KEY_RELEASE)
    return event->key.key_scancode;

  return 0;
}

void
gdk_event_set_user_data (GdkEvent *event,
                         GObject  *user_data)
{
  if (user_data)
    {
      g_object_set_qdata_full (G_OBJECT (event), quark_event_user_data,
                               g_object_ref (user_data),
                               g_object_unref);
    }
  else
    {
      g_object_steal_qdata (G_OBJECT (event), quark_event_user_data);
    }
}

GObject *
gdk_event_get_user_data (const GdkEvent *event)
{
  return g_object_get_qdata (G_OBJECT (event), quark_event_user_data);
}

gboolean
gdk_event_is_sent (const GdkEvent *event)
{
  if (!event)
    return FALSE;

  return event->any.send_event;
}

/**
 * gdk_event_get_drag_context:
 * @event: a #GdkEvent
 * @context: (out) (transfer none):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_drag_context (const GdkEvent  *event,
                            GdkDragContext **context)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_DRAG_ENTER ||
      event->any.type == GDK_DRAG_LEAVE ||
      event->any.type == GDK_DRAG_MOTION ||
      event->any.type == GDK_DRAG_STATUS ||
      event->any.type == GDK_DROP_START ||
      event->any.type == GDK_DROP_FINISHED)
    {
      *context = event->dnd.context;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_crossing_mode:
 * @event: a #GdkEvent
 * @mode: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_crossing_mode (const GdkEvent  *event,
                             GdkCrossingMode *mode)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_ENTER_NOTIFY ||
      event->any.type == GDK_LEAVE_NOTIFY)
    {
      *mode = event->crossing.mode;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_crossing_detail:
 * @event: a #GdkEvent
 * @detail: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_crossing_detail (const GdkEvent *event,
                               GdkNotifyType  *detail)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_ENTER_NOTIFY ||
      event->any.type == GDK_LEAVE_NOTIFY)
    {
      *detail = event->crossing.detail;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touchpad_gesture_phase:
 * @event: a #GdkEvent
 * @phase: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touchpad_gesture_phase (const GdkEvent          *event,
                                      GdkTouchpadGesturePhase *phase)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *phase = event->touchpad_pinch.phase;
      return TRUE;
    }
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    {
      *phase = event->touchpad_swipe.phase;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touchpad_gesture_n_fingers:
 * @event: a #GdkEvent
 * @n_fingers: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touchpad_gesture_n_fingers (const GdkEvent *event,
                                          guint          *n_fingers)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *n_fingers = event->touchpad_pinch.n_fingers;
      return TRUE;
    }
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    {
      *n_fingers = event->touchpad_swipe.n_fingers;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touchpad_deltas:
 * @event: a #GdkEvent
 * @dx: (out):
 * @dy: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touchpad_deltas (const GdkEvent *event,
                               double         *dx,
                               double         *dy)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *dx = event->touchpad_pinch.dx;
      *dy = event->touchpad_pinch.dy;
      return TRUE;
    }
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    {
      *dx = event->touchpad_swipe.dx;
      *dy = event->touchpad_swipe.dy;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touchpad_angle_delta:
 * @event: a #GdkEvent
 * @delta: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touchpad_angle_delta (const GdkEvent *event,
                                    double         *delta)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *delta = event->touchpad_pinch.angle_delta;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touchpad_scale:
 * @event: a #GdkEvent
 * @scale: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touchpad_scale (const GdkEvent *event,
                              double         *scale)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *scale = event->touchpad_pinch.scale;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_touch_emulating_pointer:
 * @event: a #GdkEvent
 * @emulating: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_touch_emulating_pointer (const GdkEvent *event,
                                       gboolean       *emulating)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_TOUCH_BEGIN ||
      event->any.type == GDK_TOUCH_UPDATE ||
      event->any.type == GDK_TOUCH_END)
    {
      *emulating = event->touch.emulating_pointer;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_grab_window:
 * @event: a #GdkEvent
 * @window: (out) (transfer none):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_grab_window (const GdkEvent  *event,
                           GdkWindow      **window)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_GRAB_BROKEN)
    {
      *window = event->grab_broken.grab_window;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_window_state:
 * @event: a #GdkEvent
 * @changed: (out):
 * @new_state: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_window_state (const GdkEvent *event,
                            GdkWindowState *changed,
                            GdkWindowState *new_state)

{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_WINDOW_STATE)
    {
      *changed = event->window_state.changed_mask;
      *new_state = event->window_state.new_window_state;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_focus_in:
 * @event: a #GdkEvent
 * @focus_in: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_focus_in (const GdkEvent *event,
                        gboolean       *focus_in)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_FOCUS_CHANGE)
    {
      *focus_in = event->focus_change.in;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_pad_group_mode:
 * @event: a #GdkEvent
 * @group: (out):
 * @mode: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_pad_group_mode (const GdkEvent *event,
                              guint          *group,
                              guint          *mode)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_PAD_GROUP_MODE)
    {
      *group = event->pad_group_mode.group;
      *mode = event->pad_group_mode.mode;
      return TRUE;
    }
  else if (event->any.type == GDK_PAD_BUTTON_PRESS ||
           event->any.type == GDK_PAD_BUTTON_RELEASE)
    {
      *group = event->pad_button.group;
      *mode = event->pad_button.mode;
      return TRUE;
    }
  else if (event->any.type == GDK_PAD_RING ||
           event->any.type == GDK_PAD_STRIP)
    {
      *group = event->pad_axis.group;
      *mode = event->pad_axis.mode;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_pad_button:
 * @event: a #GdkEvent
 * @button: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_pad_button (const GdkEvent *event,
                          guint          *button)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_PAD_BUTTON_PRESS ||
      event->any.type == GDK_PAD_BUTTON_RELEASE)
    {
      *button = event->pad_button.button;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_pad_axis_value:
 * @event: a #GdkEvent
 * @index: (out):
 * @value: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_pad_axis_value (const GdkEvent *event,
                              guint          *index,
                              gdouble        *value)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_PAD_RING ||
      event->any.type == GDK_PAD_STRIP)
    {
      *index = event->pad_axis.index;
      *value = event->pad_axis.value;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_property:
 * @event: a #GdkEvent
 * @property: (out):
 * @state: (out):
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_property (const GdkEvent   *event,
                        GdkAtom          *property,
                        GdkPropertyState *state)
{
  if (!event)
    return FALSE;

  if (event->any.type == GDK_PROPERTY_NOTIFY)
    {
      *property = event->property.atom;
      *state = event->property.state;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_axes:
 * @event: a #GdkEvent
 * @axes: (transfer none) (out) (array length=n_axes): the array of values for all axes
 * @n_axes: (out): the length of array
 * 
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_axes (GdkEvent  *event,
                    gdouble  **axes,
                    guint     *n_axes)
{
  GdkDevice *source_device;

  if (!event)
    return FALSE;

  source_device = gdk_event_get_source_device (event);

  if (!source_device)
    return FALSE;

  if (event->any.type == GDK_MOTION_NOTIFY)
    {
      *axes = event->motion.axes;
      *n_axes = gdk_device_get_n_axes (source_device);
      return TRUE;
    }
  else if (event->any.type == GDK_BUTTON_PRESS ||
           event->any.type == GDK_BUTTON_RELEASE)
    {
      *axes = event->button.axes;
      *n_axes = gdk_device_get_n_axes (source_device);
      return TRUE;
    }

  return FALSE;
}

