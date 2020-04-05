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
#include "gdkdragprivate.h"
#include "gdkdropprivate.h"
#include "gdkkeysprivate.h"
#include "gdk-private.h"

#include <string.h>
#include <math.h>


/**
 * SECTION:events
 * @Short_description: Functions for handling events from the window system
 * @Title: Events
 *
 * This section describes functions dealing with events from the window
 * system.
 *
 * In GTK applications the events are handled automatically by toplevel
 * widgets and passed on to the event controllers of appropriate widgets,
 * so these functions are rarely needed.
 */

/**
 * GdkEvent:
 *
 * The GdkEvent struct contains only private fields and
 * should not be accessed directly.
 */

/**
 * GdkEventSequence:
 *
 * GdkEventSequence is an opaque type representing a sequence
 * of related touch events.
 */


G_DEFINE_BOXED_TYPE (GdkEvent, gdk_event,
                     gdk_event_ref,
                     gdk_event_unref)

static gboolean
check_event_sanity (GdkEvent *event)
{
  if (event->any.device != NULL &&
      gdk_surface_get_display (event->any.surface) != gdk_device_get_display (event->any.device))
    {
      char *type = g_enum_to_string (GDK_TYPE_EVENT_TYPE, event->any.type);
      g_warning ("Event of type %s with mismatched device display", type);
      g_free (type);
      return FALSE;
    }

  if (event->any.source_device != NULL &&
      gdk_surface_get_display (event->any.surface) != gdk_device_get_display (event->any.source_device))
    {
      char *type = g_enum_to_string (GDK_TYPE_EVENT_TYPE, event->any.type);
      g_warning ("Event of type %s with mismatched source device display", type);
      g_free (type);
      return FALSE;
    }

  return TRUE;
}

void
_gdk_event_emit (GdkEvent *event)
{
  if (!check_event_sanity (event))
    return;

  if (gdk_drag_handle_source_event (event))
    return;

  gdk_surface_handle_event (event);
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

  tmp_list = g_queue_peek_head_link (&display->queued_events);
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
  g_queue_push_tail (&display->queued_events, event);

  return g_queue_peek_tail_link (&display->queued_events);
}

/**
 * _gdk_event_queue_insert_after:
 * @display: a #GdkDisplay
 * @sibling: Append after this event.
 * @event: Event to append.
 *
 * Appends an event after the specified event, or if it isn’t in
 * the queue, onto the tail of the event queue.
 */
void
_gdk_event_queue_insert_after (GdkDisplay *display,
                               GdkEvent   *sibling,
                               GdkEvent   *event)
{
  GList *prev = g_queue_find (&display->queued_events, sibling);

  if (prev)
    g_queue_insert_after (&display->queued_events, prev, event);
  else
    g_queue_push_tail (&display->queued_events, event);
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
 */
void
_gdk_event_queue_insert_before (GdkDisplay *display,
				GdkEvent   *sibling,
				GdkEvent   *event)
{
  GList *next = g_queue_find (&display->queued_events, sibling);

  if (next)
    g_queue_insert_before (&display->queued_events, next, event);
  else
    g_queue_push_head (&display->queued_events, event);
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
  g_queue_unlink (&display->queued_events, node);
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

static void
gdk_event_push_history (GdkEvent *event,
                        GdkEvent *history_event)
{
  GdkTimeCoord *hist;
  GdkDevice *device;
  gint i, n_axes;

  g_assert (event->any.type == GDK_MOTION_NOTIFY);
  g_assert (history_event->any.type == GDK_MOTION_NOTIFY);

  hist = g_new0 (GdkTimeCoord, 1);

  device = gdk_event_get_device (history_event);
  n_axes = gdk_device_get_n_axes (device);

  for (i = 0; i <= MIN (n_axes, GDK_MAX_TIMECOORD_AXES); i++)
    gdk_event_get_axis (history_event, i, &hist->axes[i]);

  event->motion.history = g_list_prepend (event->motion.history, hist);
}

void
_gdk_event_queue_handle_motion_compression (GdkDisplay *display)
{
  GList *tmp_list;
  GList *pending_motions = NULL;
  GdkSurface *pending_motion_surface = NULL;
  GdkDevice *pending_motion_device = NULL;
  GdkEvent *last_motion = NULL;

  /* If the last N events in the event queue are motion notify
   * events for the same surface, drop all but the last */

  tmp_list = g_queue_peek_tail_link (&display->queued_events);

  while (tmp_list)
    {
      GdkEvent *event = tmp_list->data;

      if (event->any.flags & GDK_EVENT_PENDING)
        break;

      if (event->any.type != GDK_MOTION_NOTIFY)
        break;

      if (pending_motion_surface != NULL &&
          pending_motion_surface != event->any.surface)
        break;

      if (pending_motion_device != NULL &&
          pending_motion_device != event->any.device)
        break;

      if (!last_motion)
        last_motion = event;

      pending_motion_surface = event->any.surface;
      pending_motion_device = event->any.device;
      pending_motions = tmp_list;

      tmp_list = tmp_list->prev;
    }

  while (pending_motions && pending_motions->next != NULL)
    {
      GList *next = pending_motions->next;

      if (last_motion &&
          (last_motion->motion.state &
           (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK |
            GDK_BUTTON4_MASK | GDK_BUTTON5_MASK)))
        gdk_event_push_history (last_motion, pending_motions->data);

      gdk_event_unref (pending_motions->data);
      g_queue_delete_link (&display->queued_events, pending_motions);
      pending_motions = next;
    }

  if (g_queue_get_length (&display->queued_events) == 1 &&
      g_queue_peek_head_link (&display->queued_events) == pending_motions)
    {
      GdkFrameClock *clock = gdk_surface_get_frame_clock (pending_motion_surface);
      if (clock) /* might be NULL if surface was destroyed */
	gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS);
    }
}

void
_gdk_event_queue_flush (GdkDisplay *display)
{
  GList *tmp_list;

  for (tmp_list = display->queued_events.head; tmp_list; tmp_list = tmp_list->next)
    {
      GdkEvent *event = tmp_list->data;
      event->any.flags |= GDK_EVENT_FLUSHED;
    }
}

/**
 * gdk_event_ref:
 * @event: a #GdkEvent
 *
 * Increase the ref count of @event.
 *
 * Returns: @event
 */
GdkEvent *
gdk_event_ref (GdkEvent *event)
{
  event->any.ref_count++;
  return event;
}

static void gdk_event_free (GdkEvent *event);

/**
 * gdk_event_unref:
 * @event: a #GdkEvent
 *
 * Decrease the ref count of @event, and free it
 * if the last reference is dropped.
 */
void
gdk_event_unref (GdkEvent *event)
{
  event->any.ref_count--;
  if (event->any.ref_count == 0)
    gdk_event_free (event);
}

/**
 * gdk_event_get_pointer_emulated:
 * @event: a #GdkEvent
 *
 * Returns whether this event is an 'emulated' pointer event (typically
 * from a touch event), as opposed to a real one.
 *
 * Returns: %TRUE if this event is emulated
 */
gboolean
gdk_event_get_pointer_emulated (GdkEvent *event)
{
  return event->any.pointer_emulated;
}

static void
gdk_event_free (GdkEvent *event)
{
  GdkDisplay *display;

  switch ((guint) event->any.type)
    {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      g_clear_object (&event->crossing.child_surface);
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      g_clear_object (&event->dnd.drop);
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

    case GDK_MOTION_NOTIFY:
      g_clear_object (&event->motion.tool);
      g_free (event->motion.axes);
      g_list_free_full (event->motion.history, g_free);
      break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      g_clear_object (&event->proximity.tool);
      break;

    case GDK_SCROLL:
      g_clear_object (&event->scroll.tool);
      break;

    default:
      break;
    }

  display = gdk_event_get_display (event);
  if (display)
    _gdk_display_event_data_free (display, event);

  if (event->any.surface)
    g_object_unref (event->any.surface);

  g_clear_object (&event->any.device);
  g_clear_object (&event->any.source_device);

  g_free (event);
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
gdk_event_get_axis (GdkEvent   *event,
		    GdkAxisUse  axis_use,
		    double     *value)
{
  double *axes;

  if (axis_use == GDK_AXIS_X || axis_use == GDK_AXIS_Y)
    {
      gdouble x, y;

      switch ((guint) event->any.type)
	{
        case GDK_MOTION_NOTIFY:
	  x = event->motion.x;
	  y = event->motion.y;
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
 **/
gboolean
gdk_event_triggers_context_menu (GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->any.type == GDK_BUTTON_PRESS)
    {
      GdkEventButton *bevent = (GdkEventButton *) event;
      GdkDisplay *display;
      GdkModifierType modifier;

      g_return_val_if_fail (GDK_IS_SURFACE (bevent->any.surface), FALSE);

      if (bevent->button == GDK_BUTTON_SECONDARY &&
          ! (bevent->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)))
        return TRUE;

      display = gdk_surface_get_display (bevent->any.surface);

      modifier = gdk_keymap_get_modifier_mask (gdk_display_get_keymap (display),
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

  if (!gdk_event_get_position (event1, &x1, &y1) ||
      !gdk_event_get_position (event2, &x2, &y2))
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
 **/
gboolean
gdk_events_get_center (GdkEvent *event1,
                       GdkEvent *event2,
                       gdouble  *x,
                       gdouble  *y)
{
  gdouble x1, x2, y1, y2;

  if (!gdk_event_get_position (event1, &x1, &y1) ||
      !gdk_event_get_position (event2, &x2, &y2))
    return FALSE;

  if (x)
    *x = (x2 + x1) / 2;

  if (y)
    *y = (y2 + y1) / 2;

  return TRUE;
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
 * gdk_event_get_axes:
 * @event: a #GdkEvent
 * @axes: (transfer none) (out) (array length=n_axes): the array of values for all axes
 * @n_axes: (out): the length of array
 *
 * Extracts all axis values from an event.
 *
 * Returns: %TRUE on success, otherwise %FALSE
 **/
gboolean
gdk_event_get_axes (GdkEvent  *event,
                    double   **axes,
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
  else if (event->any.type == GDK_TOUCH_BEGIN ||
           event->any.type == GDK_TOUCH_UPDATE ||
           event->any.type == GDK_TOUCH_END ||
           event->any.type == GDK_TOUCH_CANCEL)
    {
      *axes = event->touch.axes;
      *n_axes = gdk_device_get_n_axes (source_device);
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_event_get_motion_history:
 * @event: a #GdkEvent of type %GDK_MOTION_NOTIFY
 *
 * Retrieves the history of the @event motion, as a list of time and
 * coordinates.
 *
 * Returns: (transfer container) (element-type GdkTimeCoord) (nullable): a list
 *   of time and coordinates
 */
GList *
gdk_event_get_motion_history (GdkEvent       *event)
{
  if (event->any.type != GDK_MOTION_NOTIFY)
    return NULL;
  return g_list_reverse (g_list_copy (event->motion.history));
}

GdkEvent *
gdk_event_button_new (GdkEventType     type,
                      GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDevice       *source_device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      guint            button,
                      double           x,
                      double           y,
                      double          *axes)
{
  GdkEventButton *event;

  g_return_val_if_fail (type == GDK_BUTTON_PRESS || 
                        type == GDK_BUTTON_RELEASE, NULL);

  event = g_new0 (GdkEventButton, 1);
  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->tool = tool ? g_object_ref (tool) : NULL;
  event->axes = NULL;
  event->state = state;
  event->button = button;
  event->x = x;
  event->y = y;
  event->axes = axes;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_motion_new (GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDevice       *source_device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      double           x,
                      double           y,
                      double          *axes)
{
  GdkEventMotion *event = g_new0 (GdkEventMotion, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_MOTION_NOTIFY;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->tool = tool ? g_object_ref (tool) : NULL;
  event->state = state;
  event->x = x;
  event->y = y;
  event->axes = axes;
  event->state = state;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_crossing_new (GdkEventType     type,
                        GdkSurface      *surface,
                        GdkDevice       *device,
                        GdkDevice       *source_device,
                        guint32          time,
                        GdkModifierType  state,
                        double           x,
                        double           y,
                        GdkCrossingMode  mode,
                        GdkNotifyType    detail)
{
  GdkEventCrossing *event;

  g_return_val_if_fail (type == GDK_ENTER_NOTIFY ||
                        type == GDK_LEAVE_NOTIFY, NULL);

  event = g_new0 (GdkEventCrossing, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->state = state;
  event->x = x;
  event->y = y;
  event->mode = mode;
  event->detail = detail;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_proximity_new (GdkEventType   type,
                         GdkSurface    *surface,
                         GdkDevice     *device,
                         GdkDevice     *source_device,
                         GdkDeviceTool *tool,
                         guint32        time)
{
  GdkEventProximity *event;

  g_return_val_if_fail (type == GDK_PROXIMITY_IN ||
                        type == GDK_PROXIMITY_OUT, NULL);

  event = g_new0 (GdkEventProximity, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->tool = tool ? g_object_ref (tool) : NULL;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_key_new (GdkEventType      type,
                   GdkSurface       *surface,
                   GdkDevice        *device,
                   GdkDevice        *source_device,
                   guint32           time,
                   guint             keycode,
                   GdkModifierType   state,
                   gboolean          is_modifier,
                   GdkTranslatedKey *translated,
                   GdkTranslatedKey *no_lock)
{
  GdkEventKey *event;

  g_return_val_if_fail (type == GDK_KEY_PRESS ||
                        type == GDK_KEY_RELEASE, NULL);

  event = g_new0 (GdkEventKey, 1);
  
  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->keycode = keycode;
  event->state = state;
  event->any.key_is_modifier = is_modifier;
  event->translated[0] = *translated;
  event->translated[1] = *no_lock;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_configure_new (GdkSurface *surface,
                         int         width,
                         int         height)
{
  GdkEventConfigure *event = g_new0 (GdkEventConfigure, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_CONFIGURE;
  event->any.time = GDK_CURRENT_TIME;
  event->any.surface = g_object_ref (surface);
  event->width = width;
  event->height = height;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_delete_new (GdkSurface *surface)
{
  GdkEventAny *event = g_new0 (GdkEventAny, 1);

  event->ref_count = 1;
  event->type = GDK_DELETE;
  event->time = GDK_CURRENT_TIME;
  event->surface = g_object_ref (surface);

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_focus_new (GdkSurface *surface,
                     GdkDevice  *device,
                     GdkDevice  *source_device,
                     gboolean    focus_in)
{
  GdkEventFocus *event = g_new0 (GdkEventFocus, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_FOCUS_CHANGE;
  event->any.time = GDK_CURRENT_TIME;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->any.focus_in = focus_in;

  return  (GdkEvent *)event;
}

GdkEvent *
gdk_event_scroll_new (GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDevice       *source_device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      double           delta_x,
                      double           delta_y,
                      gboolean         is_stop)
{
  GdkEventScroll *event = g_new0 (GdkEventScroll, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_SCROLL;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->tool = tool ? g_object_ref (tool) : NULL;
  event->state = state;
  event->x = NAN;
  event->y = NAN;
  event->direction = GDK_SCROLL_SMOOTH;
  event->delta_x = delta_x;
  event->delta_y = delta_y;
  event->any.scroll_is_stop = is_stop;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_discrete_scroll_new (GdkSurface         *surface,
                               GdkDevice          *device,
                               GdkDevice          *source_device,
                               GdkDeviceTool      *tool,
                               guint32             time,
                               GdkModifierType     state,
                               GdkScrollDirection  direction,
                               gboolean            emulated)
{
  GdkEventScroll *event = g_new0 (GdkEventScroll, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_SCROLL;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->tool = tool ? g_object_ref (tool) : NULL;
  event->state = state;
  event->x = NAN;
  event->y = NAN;
  event->direction = direction;
  event->any.pointer_emulated = emulated;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_touch_new (GdkEventType      type,
                     GdkEventSequence *sequence,
                     GdkSurface       *surface,
                     GdkDevice        *device,
                     GdkDevice        *source_device,
                     guint32           time,
                     GdkModifierType   state,
                     double            x,
                     double            y,
                     double           *axes,
                     gboolean          emulating)
{
  GdkEventTouch *event;

  g_return_val_if_fail (type == GDK_TOUCH_BEGIN ||
                        type == GDK_TOUCH_END ||
                        type == GDK_TOUCH_UPDATE ||
                        type == GDK_TOUCH_CANCEL, NULL);

  event = g_new0 (GdkEventTouch, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->sequence = sequence;
  event->state = state;
  event->x = x;
  event->y = y;
  event->axes = axes;
  event->any.touch_emulating = emulating;
  event->any.pointer_emulated = emulating;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_touchpad_swipe_new (GdkSurface *surface,
                              GdkDevice  *device,
                              GdkDevice  *source_device,
                              guint32     time,
                              GdkModifierType state,
                              GdkTouchpadGesturePhase phase,
                              double      x,
                              double      y,
                              int         n_fingers,
                              double      dx,
                              double      dy)
{
  GdkEventTouchpadSwipe *event = g_new0 (GdkEventTouchpadSwipe, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_TOUCHPAD_SWIPE;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->state = state;
  event->phase = phase;
  event->x = x;
  event->y = y;
  event->dx = dx;
  event->dy = dy;
  event->n_fingers = n_fingers;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_touchpad_pinch_new (GdkSurface *surface,
                              GdkDevice  *device,
                              GdkDevice  *source_device,
                              guint32     time,
                              GdkModifierType state,
                              GdkTouchpadGesturePhase phase,
                              double      x,
                              double      y,
                              int         n_fingers,
                              double      dx,
                              double      dy,
                              double      scale,
                              double      angle_delta)
{
  GdkEventTouchpadPinch *event = g_new0 (GdkEventTouchpadPinch, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_TOUCHPAD_PINCH;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->state = state;
  event->phase = phase;
  event->x = x;
  event->y = y;
  event->dx = dx;
  event->dy = dy;
  event->n_fingers = n_fingers;
  event->scale = scale;
  event->angle_delta = angle_delta;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_pad_ring_new (GdkSurface *surface,
                        GdkDevice  *device,
                        GdkDevice  *source_device,
                        guint32     time,
                        guint       group,
                        guint       index,
                        guint       mode,
                        double      value)
{
  GdkEventPadAxis *event = g_new0 (GdkEventPadAxis, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_PAD_RING;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->group = group;
  event->index = index;
  event->mode = mode;
  event->value = value;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_pad_strip_new (GdkSurface *surface,
                         GdkDevice  *device,
                         GdkDevice  *source_device,
                         guint32     time,
                         guint       group,
                         guint       index,
                         guint       mode,
                         double      value)
{
  GdkEventPadAxis *event = g_new0 (GdkEventPadAxis, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_PAD_STRIP;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->group = group;
  event->index = index;
  event->mode = mode;
  event->value = value;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_pad_button_new (GdkEventType  type,
                          GdkSurface   *surface,
                          GdkDevice    *device,
                          GdkDevice    *source_device,
                          guint32       time,
                          guint         group,
                          guint         button,
                          guint         mode)
{
  GdkEventPadButton *event;

  g_return_val_if_fail (type == GDK_PAD_BUTTON_PRESS ||
                        type == GDK_PAD_BUTTON_RELEASE, NULL);

  event = g_new0 (GdkEventPadButton, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->group = group;
  event->button = button;
  event->mode = mode;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_pad_group_mode_new (GdkSurface *surface,
                              GdkDevice  *device,
                              GdkDevice  *source_device,
                              guint32     time,
                              guint       group,
                              guint       mode)
{
  GdkEventPadGroupMode *event = g_new0 (GdkEventPadGroupMode, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_PAD_GROUP_MODE;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->group = group;
  event->mode = mode;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_drag_new (GdkEventType  type,
                    GdkSurface   *surface,
                    GdkDevice    *device,
                    GdkDrop      *drop,
                    guint32       time,
                    double        x,
                    double        y)
{
  GdkEventDND *event;

  g_return_val_if_fail (type == GDK_DRAG_ENTER ||
                        type == GDK_DRAG_MOTION ||
                        type == GDK_DRAG_LEAVE ||
                        type == GDK_DROP_START, NULL);

  event = g_new0 (GdkEventDND, 1);

  event->any.ref_count = 1;
  event->any.type = type;
  event->any.time = time;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->drop = g_object_ref (drop);
  event->x = x;
  event->y = y;

  return (GdkEvent *)event;
}

GdkEvent *
gdk_event_grab_broken_new (GdkSurface *surface,
                           GdkDevice  *device,
                           GdkDevice  *source_device,
                           GdkSurface *grab_surface,
                           gboolean    implicit)
{
  GdkEventGrabBroken *event = g_new0 (GdkEventGrabBroken, 1);

  event->any.ref_count = 1;
  event->any.type = GDK_GRAB_BROKEN;
  event->any.time = GDK_CURRENT_TIME;
  event->any.surface = g_object_ref (surface);
  event->any.device = g_object_ref (device);
  event->any.source_device = g_object_ref (source_device);
  event->grab_surface = grab_surface;
  event->implicit = implicit;
  event->keyboard = gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD;

  return (GdkEvent *)event;
}

/**
 * gdk_event_get_event_type:
 * @event: a #GdkEvent
 *
 * Retrieves the type of the event.
 *
 * Returns: a #GdkEventType
 */
GdkEventType
gdk_event_get_event_type (GdkEvent *event)
{
  return event->any.type;
}

/**
 * gdk_event_get_surface:
 * @event: a #GdkEvent
 *
 * Extracts the #GdkSurface associated with an event.
 *
 * Returns: (transfer none): The #GdkSurface associated with the event
 */
GdkSurface *
gdk_event_get_surface (GdkEvent *event)
{
  return event->any.surface;
}

/**
 * gdk_event_get_device:
 * @event: a #GdkEvent.
 *
 * Returns the device of an event.
 *
 * Returns: (nullable) (transfer none): a #GdkDevice, or %NULL.
 **/
GdkDevice *
gdk_event_get_device (GdkEvent *event)
{
  return event->any.device;
}

/**
 * gdk_event_get_source_device:
 * @event: a #GdkEvent
 *
 * This function returns the hardware (slave) #GdkDevice that has
 * triggered the event, falling back to the virtual (master) device
 * (as in gdk_event_get_device()) if the event wasn’t caused by
 * interaction with a hardware device. This may happen for example
 * in synthesized crossing events after a #GdkSurface updates its
 * geometry or a grab is acquired/released.
 *
 * If the event does not contain a device field, this function will
 * return %NULL.
 *
 * Returns: (nullable) (transfer none): a #GdkDevice, or %NULL.
 **/
GdkDevice *
gdk_event_get_source_device (GdkEvent *event)
{
  if (event->any.source_device)
    return event->any.source_device;

  return event->any.device;
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
 * Note: the #GdkDeviceTools will be constant during
 * the application lifetime, if settings must be stored
 * persistently across runs, see gdk_device_tool_get_serial()
 *
 * Returns: (transfer none): The current device tool, or %NULL
 **/
GdkDeviceTool *
gdk_event_get_device_tool (GdkEvent *event)
{
  if (event->any.type == GDK_BUTTON_PRESS ||
      event->any.type == GDK_BUTTON_RELEASE)
    return event->button.tool;
  else if (event->any.type == GDK_MOTION_NOTIFY)
    return event->motion.tool;
  else if (event->any.type == GDK_PROXIMITY_IN ||
           event->any.type == GDK_PROXIMITY_OUT)
    return event->proximity.tool;
  else if (event->any.type == GDK_SCROLL)
    return event->scroll.tool;

  return NULL;
}

/**
 * gdk_event_get_time:
 * @event: a #GdkEvent
 * 
 * Returns the time stamp from @event, if there is one; otherwise
 * returns #GDK_CURRENT_TIME.
 * 
 * Returns: time stamp field from @event
 **/
guint32
gdk_event_get_time (GdkEvent *event)
{
  return event->any.time;
}

/**
 * gdk_event_get_display:
 * @event: a #GdkEvent
 *
 * Retrieves the #GdkDisplay associated to the @event.
 *
 * Returns: (transfer none) (nullable): a #GdkDisplay
 */
GdkDisplay *
gdk_event_get_display (GdkEvent *event)
{
  if (event->any.surface)
    return gdk_surface_get_display (event->any.surface);

  return NULL;
}

/**
 * gdk_event_get_event_sequence:
 * @event: a #GdkEvent
 *
 * If @event is a touch event, returns the #GdkEventSequence
 * to which the event belongs. Otherwise, return %NULL.
 *
 * Returns: (transfer none): the event sequence that the event belongs to
 */
GdkEventSequence *
gdk_event_get_event_sequence (GdkEvent *event)
{
  switch ((int) event->any.type)
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      return event->touch.sequence;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      return (GdkEventSequence *) event->dnd.drop;

    default:
      return NULL;
    }
}

/**
 * gdk_event_get_modifier_state:
 * @event: a #GdkEvent
 *
 * Returns the modifier state field of an event.
 *
 * Returns: the modifier state of @event
 **/
GdkModifierType
gdk_event_get_modifier_state (GdkEvent *event)
{
#if 0
  g_return_val_if_fail (event->any.type == GDK_ENTER_NOTIFY ||
                        event->any.type == GDK_LEAVE_NOTIFY ||
                        event->any.type == GDK_BUTTON_PRESS ||
                        event->any.type == GDK_BUTTON_RELEASE ||
                        event->any.type == GDK_MOTION_NOTIFY ||
                        event->any.type == GDK_TOUCH_BEGIN ||
                        event->any.type == GDK_TOUCH_UPDATE ||
                        event->any.type == GDK_TOUCH_END ||
                        event->any.type == GDK_TOUCH_CANCEL ||
                        event->any.type == GDK_TOUCHPAD_SWIPE ||
                        event->any.type == GDK_TOUCHPAD_PINCH||
                        event->any.type == GDK_SCROLL ||
                        event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);
#endif

  switch ((int)event->any.type)
    {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      return event->crossing.state;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return event->button.state;
    case GDK_MOTION_NOTIFY:
      return event->motion.state;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      return event->touch.state;
    case GDK_TOUCHPAD_SWIPE:
      return event->touchpad_swipe.state;
    case GDK_TOUCHPAD_PINCH:
      return event->touchpad_pinch.state;
    case GDK_SCROLL:
      return event->scroll.state;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return event->key.state;
    default:
      /* no state field */
      break;
    }

  return 0;
}

/**
 * gdk_event_get_position:
 * @event: a #GdkEvent
 * @x: (out): location to put event surface x coordinate
 * @y: (out): location to put event surface y coordinate
 *
 * Extract the event surface relative x/y coordinates from an event.
 **/
gboolean
gdk_event_get_position (GdkEvent *event,
                        double   *x,
                        double   *y)
{
#if 0
  g_return_if_fail (event->any.type == GDK_ENTER_NOTIFY ||
                    event->any.type == GDK_LEAVE_NOTIFY ||
                    event->any.type == GDK_BUTTON_PRESS ||
                    event->any.type == GDK_BUTTON_RELEASE ||
                    event->any.type == GDK_MOTION_NOTIFY ||
                    event->any.type == GDK_TOUCH_BEGIN ||
                    event->any.type == GDK_TOUCH_UPDATE ||
                    event->any.type == GDK_TOUCH_END ||
                    event->any.type == GDK_TOUCH_CANCEL ||
                    event->any.type == GDK_TOUCHPAD_SWIPE ||
                    event->any.type == GDK_TOUCHPAD_PINCH||
                    event->any.type == GDK_DRAG_ENTER ||
                    event->any.type == GDK_DRAG_LEAVE ||
                    event->any.type == GDK_DRAG_MOTION ||
                    event->any.type == GDK_DROP_START);
#endif

  switch ((int)event->any.type)
    {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      *x = event->crossing.x;
      *y = event->crossing.y;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      *x = event->button.x;
      *y = event->button.y;
      break;
    case GDK_MOTION_NOTIFY:
      *x = event->motion.x;
      *y = event->motion.y;
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      *x = event->touch.x;
      *y = event->touch.y;
      break;
    case GDK_TOUCHPAD_SWIPE:
      *x = event->touchpad_swipe.x;
      *y = event->touchpad_swipe.y;
      break;
    case GDK_TOUCHPAD_PINCH:
      *x = event->touchpad_pinch.x;
      *y = event->touchpad_pinch.y;
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      *x = event->dnd.x;
      *y = event->dnd.y;
      break;
    default:
      /* no position */
      *x = NAN;
      *y = NAN;
      return FALSE;
    }

  return TRUE;
}

/**
 * gdk_button_event_get_button:
 * @event: a button event
 *
 * Extract the button number from a button event.
 *
 * Returns: the button of @event
 **/
guint
gdk_button_event_get_button (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_BUTTON_PRESS ||
                        event->any.type == GDK_BUTTON_RELEASE, 0);
  
  return event->button.button;
}

/**
 * gdk_key_event_get_keyval:
 * @event: a key event
 *
 * Extracts the keyval from a key event.
 *
 * Returns: the keyval of @event
 */
guint
gdk_key_event_get_keyval (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);

  return event->key.translated[0].keyval;
}

/**
 * gdk_key_event_get_keycode:
 * @event: a key event
 *
 * Extracts the keycode from a key event.
 *
 * Returns: the keycode of @event
 */
guint
gdk_key_event_get_keycode (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);

  return event->key.keycode;
}

/**
 * gdk_key_event_get_level:
 * @event: a key event
 *
 * Extracts the shift level from a key event.
 *
 * Returns: the shift level of @event
 */
guint
gdk_key_event_get_level (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);

  return event->key.translated[0].level;
}

/**
 * gdk_key_event_get_layout:
 * @event: a key event
 *
 * Extracts the layout from a key event.
 *
 * Returns: the layout of @event
 */
guint
gdk_key_event_get_layout (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);

  return event->key.translated[0].layout;
}

/**
 * gdk_key_event_get_consumed_modifiers:
 * @event: a key event
 *
 * Extracts the consumed modifiers from a key event.
 *
 * Returns: the consumed modifiers or @event
 */
GdkModifierType
gdk_key_event_get_consumed_modifiers (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, 0);

  return event->key.translated[0].consumed;
}

/**
 * gdk_key_event_is_modifier:
 * @event: a key event
 *
 * Extracts whether the key event is for a modifier key.
 *
 * Returns: %TRUE if the @event is for a modifier key
 */
gboolean
gdk_key_event_is_modifier (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, FALSE);

  return event->any.key_is_modifier;
}

/**
 * gdk_configure_event_get_size:
 * @event: a configure event
 * @width: (out): return location for surface width
 * @height: (out): return location for surface height
 *
 * Extracts the surface size from a configure event.
 */
void
gdk_configure_event_get_size (GdkEvent *event,
                              int      *width,
                              int      *height)
{
  g_return_if_fail (event->any.type == GDK_CONFIGURE);

  *width = event->configure.width;
  *height = event->configure.height;
}

/**
 * gdk_touch_event_get_emulating_pointer:
 * @event: a touch event
 *
 * Extracts whether a touch event is emulating a pointer event.
 *
 * Returns: %TRUE if @event is emulating
 **/
gboolean
gdk_touch_event_get_emulating_pointer (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_TOUCH_BEGIN ||
                        event->any.type == GDK_TOUCH_UPDATE ||
                        event->any.type == GDK_TOUCH_END ||
                        event->any.type == GDK_TOUCH_CANCEL, FALSE);

  return event->any.touch_emulating;
}

/**
 * gdk_crossing_event_get_mode:
 * @event: a crossing event
 *
 * Extracts the crossing mode from a crossing event.
 *
 * Returns: the mode of @event
 */
GdkCrossingMode
gdk_crossing_event_get_mode (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_ENTER_NOTIFY ||
                        event->any.type == GDK_LEAVE_NOTIFY, 0);

  return event->crossing.mode;
}

/**
 * gdk_crossing_event_get_detail:
 * @event: a crossing event
 *
 * Extracts the notify detail from a crossing event.
 *
 * Returns: the notify detail of @event
 */
GdkNotifyType
gdk_crossing_event_get_detail (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_ENTER_NOTIFY ||
                        event->any.type == GDK_LEAVE_NOTIFY, 0);

  return event->crossing.detail;
}

/**
 * gdk_focus_event_get_in:
 * @event: a focus change event
 *
 * Extracts whether this event is about focus entering or
 * leaving the surface.
 *
 * Returns: %TRUE of the focus is entering
 */
gboolean
gdk_focus_event_get_in (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_FOCUS_CHANGE, FALSE);

  return event->any.focus_in;
}

/**
 * gdk_scroll_event_get_direction:
 * @event: a scroll event
 *
 * Extracts the direction of a scroll event.
 *
 * Returns: the scroll direction of @event
 */
GdkScrollDirection
gdk_scroll_event_get_direction (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_SCROLL, 0);

  return event->scroll.direction;
}

/**
 * gdk_scroll_event_get_deltas:
 * @event: a scroll event
 * @delta_x: (out): return location for x scroll delta
 * @delta_y: (out): return location for y scroll delta
 *
 * Extracts the scroll deltas of a scroll event.
 *
 * The deltas will be zero unless the scroll direction
 * is %GDK_SCROLL_SMOOTH.
 */
void
gdk_scroll_event_get_deltas (GdkEvent *event,
                             double   *delta_x,
                             double   *delta_y)
{
  g_return_if_fail (event->any.type == GDK_SCROLL);

  *delta_x = event->scroll.delta_x;
  *delta_y = event->scroll.delta_y;
}

/**
 * gdk_scroll_event_is_stop:
 * @event: a scroll event
 *
 * Check whether a scroll event is a stop scroll event. Scroll sequences
 * with smooth scroll information may provide a stop scroll event once the
 * interaction with the device finishes, e.g. by lifting a finger. This
 * stop scroll event is the signal that a widget may trigger kinetic
 * scrolling based on the current velocity.
 *
 * Stop scroll events always have a delta of 0/0.
 *
 * Returns: %TRUE if the event is a scroll stop event
 */
gboolean
gdk_scroll_event_is_stop (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_SCROLL, FALSE);

  return event->any.scroll_is_stop;
}

/**
 * gdk_touchpad_event_get_gesture_phase:
 * @event: a touchpad #GdkEvent
 *
 * Extracts the touchpad gesture phase from a touchpad event.
 *
 * Returns: the gesture phase of @event
 **/
GdkTouchpadGesturePhase
gdk_touchpad_event_get_gesture_phase (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_TOUCHPAD_PINCH ||
                        event->any.type == GDK_TOUCHPAD_SWIPE, 0);

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    return event->touchpad_pinch.phase;
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    return event->touchpad_swipe.phase;

  return 0;
}

/**
 * gdk_touchpad_event_get_n_fingers:
 * @event: a touchpad event
 *
 * Extracts the number of fingers from a touchpad event.
 *
 * Returns: the number of fingers for @event
 **/
guint
gdk_touchpad_event_get_n_fingers (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_TOUCHPAD_PINCH ||
                        event->any.type == GDK_TOUCHPAD_SWIPE, 0);

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    return event->touchpad_pinch.n_fingers;
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    return event->touchpad_swipe.n_fingers;

  return 0;
}

/**
 * gdk_touchpad_event_get_deltas:
 * @event: a touchpad event
 * @dx: (out): return location for x
 * @dy: (out): return location for y
 *
 * Extracts delta information from a touchpad event.
 **/
void
gdk_touchpad_event_get_deltas (GdkEvent *event,
                               double   *dx,
                               double   *dy)
{
  g_return_if_fail (event->any.type == GDK_TOUCHPAD_PINCH ||
                    event->any.type == GDK_TOUCHPAD_SWIPE);

  if (event->any.type == GDK_TOUCHPAD_PINCH)
    {
      *dx = event->touchpad_pinch.dx;
      *dy = event->touchpad_pinch.dy;
    }
  else if (event->any.type == GDK_TOUCHPAD_SWIPE)
    {
      *dx = event->touchpad_swipe.dx;
      *dy = event->touchpad_swipe.dy;
    }
  else
    {
      *dx = NAN;
      *dy = NAN;
    }
}

/**
 * gdk_touchpad_pinch_event_get_angle_delta:
 * @event: a touchpad pinch event
 *
 * Extracts the angle delta from a touchpad pinch event.
 *
 * Returns: the angle delta of @event
 */
double
gdk_touchpad_pinch_event_get_angle_delta (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_TOUCHPAD_PINCH, 0);

  return event->touchpad_pinch.angle_delta;
}

/**
 * gdk_touchpad_pinch_event_get_scale:
 * @event: a touchpad pinch event
 *
 * Extracts the scale from a touchpad pinch event.
 *
 * Returns: the scale of @event
 **/
double
gdk_touchpad_pinch_event_get_scale (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_TOUCHPAD_PINCH, 0);

  return event->touchpad_pinch.scale;
}

/**
 * gdk_pad_button_event_get_button:
 * @event: a pad button event
 *
 * Extracts information about the pressed button from
 * a pad event.
 *
 * Returns: the button of @event
 **/
guint
gdk_pad_button_event_get_button (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_PAD_BUTTON_PRESS ||
                        event->any.type == GDK_PAD_BUTTON_RELEASE, 0);

  return  event->pad_button.button;
}

/**
 * gdk_pad_axis_event_get_value:
 * @event: a pad strip or ring event
 * @index: (out): Return location for the axis index
 * @value: (out): Return location for the axis value
 *
 * Extracts the information from a pad strip or ring event.
 **/
void
gdk_pad_axis_event_get_value (GdkEvent       *event,
                              guint          *index,
                              gdouble        *value)
{
  g_return_if_fail (event->any.type == GDK_PAD_RING ||
                    event->any.type == GDK_PAD_STRIP);

  *index = event->pad_axis.index;
  *value = event->pad_axis.value;
}

/**
 * gdk_pad_event_get_group_mode:
 * @event: a pad event
 * @group: (out): return location for the group
 * @mode: (out): return location for the mode
 *
 * Extracts group and mode information from a pad event.
 **/
void
gdk_pad_event_get_group_mode (GdkEvent *event,
                              guint    *group,
                              guint    *mode)
{
  g_return_if_fail (event->any.type == GDK_PAD_GROUP_MODE ||
                    event->any.type == GDK_PAD_BUTTON_PRESS ||
                    event->any.type == GDK_PAD_BUTTON_RELEASE ||
                    event->any.type == GDK_PAD_RING ||
                    event->any.type == GDK_PAD_STRIP);

  switch ((guint)event->any.type)
    {
    case GDK_PAD_GROUP_MODE:
      *group = event->pad_group_mode.group;
      *mode = event->pad_group_mode.mode;
      break;
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
      *group = event->pad_button.group;
      *mode = event->pad_button.mode;
      break;
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
      *group = event->pad_axis.group;
      *mode = event->pad_axis.mode;
      break;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gdk_drag_event_get_drop:
 * @event: a DND event
 *
 * Gets the #GdkDrop from a DND event.
 *
 * Returns: (transfer none) (nullable): the drop
 **/
GdkDrop *
gdk_drag_event_get_drop (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_DRAG_ENTER ||
                        event->any.type == GDK_DRAG_LEAVE ||
                        event->any.type == GDK_DRAG_MOTION ||
                        event->any.type == GDK_DROP_START, NULL);

  return event->dnd.drop;
}

/**
 * gdk_grab_broken_event_get_grab_surface:
 * @event: a grab broken event
 *
 * Extracts the grab surface from a grab broken event.
 *
 * Returns: (transfer none): the grab surface of @event
 **/
GdkSurface *
gdk_grab_broken_event_get_grab_surface (GdkEvent *event)
{
  g_return_val_if_fail (event->any.type == GDK_GRAB_BROKEN, NULL);

  return event->grab_broken.grab_surface;
}

/**
 * gdk_key_event_matches:
 * @event: a key #GdkEvent
 * @keyval: the keyval to match
 * @modifiers: the modifiers to match
 *
 * Matches a key event against a keyboard shortcut that is specified
 * as a keyval and modifiers. Partial matches are possible where the
 * combination matches if the currently active group is ignored.
 *
 * Note that we ignore Caps Lock for matching.
 *
 * Returns: a GdkKeyMatch value describing whether @event matches
 */
GdkKeyMatch
gdk_key_event_matches (GdkEvent        *event,
                       guint            keyval,
                       GdkModifierType  modifiers)
{
  guint keycode;
  GdkModifierType state;
  GdkModifierType mask;
  guint ev_keyval;
  int layout;
  int level;
  GdkModifierType consumed_modifiers;
  GdkModifierType shift_group_mask;
  gboolean group_mod_is_accel_mod = FALSE;

  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, GDK_KEY_MATCH_NONE);

  keycode = event->key.keycode;
  state = event->key.state & ~GDK_LOCK_MASK;
  ev_keyval = event->key.translated[1].keyval;
  layout = event->key.translated[1].layout;
  level = event->key.translated[1].level;
  consumed_modifiers = event->key.translated[1].consumed;

  mask = gdk_keymap_get_modifier_mask (keymap,
                                       GDK_MODIFIER_INTENT_DEFAULT_MOD_MASK);

  /* if the group-toggling modifier is part of the default accel mod
   * mask, and it is active, disable it for matching
   */
  shift_group_mask = gdk_keymap_get_modifier_mask (keymap,
                                                   GDK_MODIFIER_INTENT_SHIFT_GROUP);
  if (mask & shift_group_mask)
    group_mod_is_accel_mod = TRUE;

  if ((modifiers & ~consumed_modifiers & mask) == (state & ~consumed_modifiers & mask))
    {
      /* modifier match */
      GdkKeymapKey *keys;
      int n_keys;
      int i;
      guint key;

      /* Shift gets consumed and applied for the event,
       * so apply it to our keyval to match
       */
      key = keyval;
      if (modifiers & GDK_SHIFT_MASK)
        {
          if (key == GDK_KEY_Tab)
            key = GDK_KEY_ISO_Left_Tab;
          else
            key = gdk_keyval_to_upper (key);
        }

      if (ev_keyval == key && /* exact match */
          (!group_mod_is_accel_mod ||
           (state & shift_group_mask) == (modifiers & shift_group_mask)))
        {
          return GDK_KEY_MATCH_EXACT;
        }

      gdk_display_map_keyval (gdk_event_get_display (event), keyval, &keys, &n_keys);

      for (i = 0; i < n_keys; i++)
        {
          if (keys[i].keycode == keycode &&
              keys[i].level == level &&
              /* Only match for group if it's an accel mod */
              (!group_mod_is_accel_mod || keys[i].group == layout))
            {
              /* partial match */
              g_free (keys);

              return GDK_KEY_MATCH_PARTIAL;
            }
        }

      g_free (keys);
    }


  return GDK_KEY_MATCH_NONE;
}

/**
 * gdk_key_event_get_match:
 * @event: a key #GdkEvent
 * @keyval: (out): return location for a keyval
 * @modifiers: (out): return location for modifiers
 *
 * Gets a keyval and modifier combination that will cause
 * gdk_event_match() to successfully match the given event.
 *
 * Returns: %TRUE on success
 */
gboolean
gdk_key_event_get_match (GdkEvent        *event,
                         guint           *keyval,
                         GdkModifierType *modifiers)
{
  GdkKeymap *keymap;
  GdkModifierType mask;
  guint key;
  guint accel_key;
  GdkModifierType accel_mods;
  GdkModifierType consumed_modifiers;

  g_return_val_if_fail (event->any.type == GDK_KEY_PRESS ||
                        event->any.type == GDK_KEY_RELEASE, FALSE);

  keymap = gdk_display_get_keymap (gdk_event_get_display (event));

  mask = gdk_keymap_get_modifier_mask (keymap,
                                       GDK_MODIFIER_INTENT_DEFAULT_MOD_MASK);

  accel_key = event->key.translated[1].keyval;
  accel_mods = event->key.state;
  consumed_modifiers = event->key.translated[1].consumed;

  if (accel_key == GDK_KEY_Sys_Req &&
      (accel_mods & GDK_ALT_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      *keyval = GDK_KEY_Print;
      *modifiers = accel_mods & mask;
      return TRUE;
    }

  key = gdk_keyval_to_lower (accel_key);

  if (key == GDK_KEY_ISO_Left_Tab)
    key = GDK_KEY_Tab;

  accel_mods &= mask & ~consumed_modifiers;

  if (accel_key != key)
    accel_mods |= GDK_SHIFT_MASK;

  *keyval = key;
  *modifiers = accel_mods;

  return TRUE;
}
