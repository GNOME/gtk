/* GDK - The GIMP Drawing Kit
 *
 * gdkeventhistory.c: input event history
 *
 * Copyright © 2019 Yariv Barkan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <stdio.h>

#include "gdkinternals.h"
#include "gdk/gdkeventhistoryprivate.h"

#include "fallback-c89.c"

/*
 * We need at least 2-3 display-frames worth of input events in the history
 * buffer to account for system-induced (display manager etc) latency - the
 * time it takes an event to reach this code. Display frames can last anywhere
 * from 33ms to about 4ms on today's monitors, corresponding to 30 FPS to 240
 * FPS.
 *
 * While the average USB-connected input device generates about 125 events per
 * second, high frequency devices such as 'gamer' mice can generate up to 1000
 * events per second. Taking the extremes here, a combination of an input
 * device that generates an event every millisecond, coupled with a monitor
 * capable of only 30 frames per second, we'll need about 33 events per frame
 * so a total of about 66 events in the buffer. In addition, to support fancier
 * interpolation methods in the future we'll need about 8 events in total and
 * maybe even more, so this is covered as well.
 */
#define EVENT_HISTORY_MAX_ELEMENTS 66

/*
 * Number of elements to consider when estimating the average time between
 * consecutive input events.
 */
#define POLL_INTERVAL_ESTIMATION_ELEMENTS 6

/*
 * Used to determine the timestamp of a dummy 'null' absolute input event.
 * Corresponds to 1000 / 12 =~ 83 events/second which should be good enough
 * for the slowest (lowest event rate) input devices. This number is not very
 * important, it just has to be more or less equal to the time interval between
 * consecutive input events.
 */
#define EVENT_HISTORY_DUMMY_POLLING_INTERVAL 12

/*
 * GdkAbsoluteEventHistory
 *
 * Event properties can be roughly classified as 'absolute', 'relative' and
 * 'discrete'. 'Absolute' properties are those for which we get the actual
 * value, for example the x,y coordinates. 'Relative' properties are those for
 * which we get delta values - we get the value relative to the previous event.
 * These include the (delta_x, delta_y) of precise scroll events, the scale of
 * pinch events etc. 'Discrete' properties are those with a discrete rather
 * than continuous set of values. For example the 'state' member of various
 * event types, the 'is_stop' field of scroll events etc.
 *
 * In this section we generally assume that the events only contain 'absolute'
 * and 'discrete' properties. 'Relative' properties are dealt with later, in
 * GdkEventHistory.
 */

struct _GdkAbsoluteEventHistory
{
  GPtrArray *events;

  /*
   * 'Scratch' buffers for interpolation. Declared at the 'instance' level in
   * order to avoid unnecessary allocations.
   */
  GArray *property_values;
  GArray *property_values_aux;
};

typedef struct _GdkAbsoluteEventHistory GdkAbsoluteEventHistory;

/*
 * Allocate an absolute events history.
 *
 * Should be freed with gdk_absolute_event_history_free().
 */
static GdkAbsoluteEventHistory *
gdk_absolute_event_history_new (void)
{
  GdkAbsoluteEventHistory *history;

  /* TODO use a ring buffer for the events? */
  history = g_slice_new (GdkAbsoluteEventHistory);
  history->events = g_ptr_array_new_with_free_func ((GDestroyNotify) gdk_event_free);
  history->property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  history->property_values_aux = g_array_new (FALSE, FALSE, sizeof (gdouble));

  return history;
}

/*
 * Free an absolute events history.
 */
static void
gdk_absolute_event_history_free (GdkAbsoluteEventHistory *history)
{
  g_ptr_array_unref (history->events);

  g_array_free (history->property_values, TRUE);
  g_array_free (history->property_values_aux, TRUE);

  g_slice_free (GdkAbsoluteEventHistory, history);
}

/*
 * Add an event to the history buffer.
 */
static void
gdk_absolute_event_history_history_push (GdkAbsoluteEventHistory *history,
                                         GdkEvent                *event)
{
  g_ptr_array_add (history->events, event);

  if (history->events->len > EVENT_HISTORY_MAX_ELEMENTS)
    g_ptr_array_remove_index (history->events, 0);
}

static guint
gdk_absolute_event_history_history_length (GdkAbsoluteEventHistory *history)
{
  return history->events->len;
}

/*
 * Returns the the most recent event in the event history or NULL if the event
 * history is empty.
 */
static GdkEvent *
gdk_absolute_event_history_get_newest_event (GdkAbsoluteEventHistory *history)
{
  if (history->events->len == 0)
    {
      return NULL;
    }

  return g_ptr_array_index (history->events, history->events->len - 1);
}

/*
 * Returns the timestamp of the most recent event in the event history.
 *
 * It's the responsibility of the caller to check if the history is empty
 * before calling this function.
 */
static guint32
gdk_absolute_event_history_newest_event_time (GdkAbsoluteEventHistory *history)
{
  GdkEvent *newest_elem = NULL;

  if (history->events->len == 0)
    {
      /* TODO maybe use an assertion here instead of just a warning */
      g_warning ("Returning '0' as the newest event time, since the event history is empty");
      return 0;
    }

  newest_elem = g_ptr_array_index (history->events, history->events->len - 1);

  return gdk_event_get_time (newest_elem);
}

/*
 * Returns the index of the most recent event in history with a timestamp
 * smaller-than or equal-to the timestamp argument.
 *
 * If the history is empty, or if no such event was found, '-1' will be
 * returned.
 */
static gint
gdk_absolute_event_history_newest_event_before (GdkAbsoluteEventHistory *history,
                                                guint32                  timestamp)
{
  gint i;
  GdkEvent *elem = NULL;

  if (history->events->len == 0)
    {
      /* No history, bail for now */
      g_warning ("Can't locate event, history is empty");
      return -1;
    }

  /* Find the first event with a time equal to or lower than the given timestamp */
  for (i = history->events->len - 1; i >= 0; i--)
    {
      elem = g_ptr_array_index (history->events, i);

      if (gdk_event_get_time (elem) <= timestamp)
        break;
    }

  /* if no matching event was found, i will be equal to '-1' here */
  return i;
}

/*
 * Return the average time between consecutive events, or '0' if the history
 * is too short.
 *
 * Use the last several events in order to prevent a skew in case of gaps in
 * the event stream. Gaps can happen, for example, when the user doesn't move
 * his fingers for a while.
 */
static guint32
gdk_absolute_event_history_average_event_interval (GdkAbsoluteEventHistory *history)
{
  GdkEvent *first_elem = NULL;
  GdkEvent *last_elem = NULL;
  guint32 first_elem_time;
  guint32 last_elem_time;
  guint num_elements_to_consider;
  guint first_index_to_consider;

  /* Need at least 2 events to get the time deltas */
  if (history->events->len < 2)
    return 0;

  /* We want to calculate the average time between the last consecutive
     POLL_INTERVAL_ESTIMATION_ELEMENTS events, or as many as we got if less */
  num_elements_to_consider = MIN (history->events->len, POLL_INTERVAL_ESTIMATION_ELEMENTS);
  first_index_to_consider = history->events->len - num_elements_to_consider;

  first_elem = g_ptr_array_index (history->events, first_index_to_consider);
  last_elem  = g_ptr_array_index (history->events, history->events->len - 1);

  first_elem_time = gdk_event_get_time (first_elem);
  last_elem_time  = gdk_event_get_time (last_elem);

  return (last_elem_time - first_elem_time) / (num_elements_to_consider - 1);
}

/*
 * Resets the event history.
 */
static void
gdk_absolute_event_history_history_reset (GdkAbsoluteEventHistory *history)
{
  if (history->events->len == 0)
    return;

  g_ptr_array_remove_range (history->events, 0,
                            history->events->len);
}

/*
 * Performs a linear interpolation between the enumerated properties of 2
 * events.
 */
static gboolean
gdk_absolute_event_history_linear_interp_props (GdkAbsoluteEventHistory  *history,
                                                GdkEvent                 *first_elem,
                                                GdkEvent                 *second_elem,
                                                gdouble                   ratio,
                                                GdkInterpolationCategory  category,
                                                GdkEvent                 *interpolated_elem)
{
  GArray *first_values = history->property_values;
  GArray *second_values = history->property_values_aux;
  gdouble fv;
  gdouble sv;
  int i;

  /* Get the values of the properties designated for interpolation */
  gdk_event_get_values_for_interpolation (first_elem, first_values, category);
  gdk_event_get_values_for_interpolation (second_elem, second_values, category);

  g_return_val_if_fail (first_values->len == second_values->len, FALSE);

  /* Interpolate the properties */
  for (i = 0; i < first_values->len; i++)
    {
      fv = g_array_index (first_values, gdouble, i);
      sv = g_array_index (second_values, gdouble, i);

      /* Use first_values to hold the interpolated values */
      g_array_index (first_values, gdouble, i) = (ratio * sv) + ((1.0 - ratio) * fv);
    }

  /* Update the event with the interpolated values */
  gdk_event_set_interpolated_values (interpolated_elem, first_values, category);

  return TRUE;
}

/*
 * Performs a linear interpolation between 2 events.
 *
 * Returns the interpolated event.
 */
static GdkEvent *
gdk_absolute_event_history_linear_interp (GdkAbsoluteEventHistory *history,
                                          GdkEvent                *first_elem,
                                          GdkEvent                *second_elem,
                                          guint32                  interpolation_point)
{
  GdkEvent *interpolated_elem;
  guint32 first_elem_time;
  guint32 second_elem_time;
  gdouble ratio;
  GdkModifierType state;

  first_elem_time  = gdk_event_get_time (first_elem);
  second_elem_time = gdk_event_get_time (second_elem);

  ratio = (gdouble) (interpolation_point - first_elem_time) /
                    (second_elem_time    - first_elem_time);

  /* Synthesize a new event */
  interpolated_elem = gdk_event_copy (first_elem);

  /* Interpolate 'relative' values. Relative properties actually hold
   * accumulated deltas */
  if (!gdk_absolute_event_history_linear_interp_props (history,
                                                       first_elem,
                                                       second_elem,
                                                       ratio,
                                                       GDK_INTERP_RELATIVE,
                                                       interpolated_elem))
    return NULL;

  /* Absolute values can be directly interpolated */
  if (!gdk_absolute_event_history_linear_interp_props (history,
                                                       first_elem,
                                                       second_elem,
                                                       ratio,
                                                       GDK_INTERP_ABSOLUTE,
                                                       interpolated_elem))
    return NULL;

  /* State is discrete so use a nearest-neighbor interpolation */
  GdkEvent *state_elem = (ratio < 0.5) ? first_elem : second_elem;
  gdk_event_get_state (state_elem, &state);
  gdk_event_set_state (interpolated_elem, state);

  /* Set interpolated event time */
  gdk_event_set_time (interpolated_elem, interpolation_point);

  return interpolated_elem;
}

/*
 * Returns a newly-allocated, interpolated event or NULL if it's impossible to
 * create one.
 *
 * This function deals with the interpolation of absolute properties. Absolute
 * interpolation is also used internally to interpolate relative properties. In
 * that case the relative properties are first converted to absolute ones, by
 * accumulating them before adding them to the history. After the interpolation
 * they are converted back to relative properties.
 *
 * gdk_absolute_event_history_interpolate_event() has no side effects. However
 * it isn't idempotent. For example when frame_time is larger than the largest
 * time stamp in the event fifo, the result will be identical to the newest
 * event. However if a more recent event is then added, calling with the same
 * frame_time can lead to different interpolated values.
 */
static GdkEvent *
gdk_absolute_event_history_interpolate_event (GdkAbsoluteEventHistory *history,
                                              gint64                   frame_time)
{
  gint i;
  GdkEvent *elem = NULL;
  guint32 interpolation_point;

  if (history->events->len == 0)
    {
      /* No history, bail for now */
      g_warning ("Interpolate event: can't interpolate event, history is empty");
      return NULL;
    }

  /* frame_time is measured in microseconds, event time in milliseconds */
  interpolation_point = (frame_time / 1000);

  /* Find the first timestamp equal to or lower than the interpolation point */
  i = gdk_absolute_event_history_newest_event_before (history,
                                                      interpolation_point);

  if (i >= 0)
    {
      elem = g_ptr_array_index (history->events, i);
    }
  else
    {
      /* The interpolation point lies before the oldest event. This is a
         non-critical situation. */
      GDK_NOTE (EVENTS, g_message ("Can't interpolate event, frame time earlier than first history element"));
      return NULL;
    }

  if (gdk_event_get_time (elem) == interpolation_point)
    {
      /* No interpolation necessary */
      return gdk_event_copy(elem);
    }
  else if (i == (history->events->len - 1))
    {
      /* The interpolation point is more recent than all events in the history,
         use the last known value. This can happen legitimately - for example
         when the fingers stay immobile on a touch device, the device no longer
         emits events even though the frame callback keeps firing.
         TODO extrapolate the value? */
      GDK_NOTE (EVENTS, g_message ("Interpolation point more recent than newest event"));
      return gdk_event_copy(elem);
    }
  else
    {
      /* The interpolation point lies between 2 consecutive events */
      GdkEvent *first_elem  = g_ptr_array_index (history->events, i);
      GdkEvent *second_elem = g_ptr_array_index (history->events, i + 1);
      GdkEvent *interpolated_elem;

      interpolated_elem = gdk_absolute_event_history_linear_interp (history,
                                                                    first_elem,
                                                                    second_elem,
                                                                    interpolation_point);
      if (!interpolated_elem)
        g_warning ("Interpolate event: can't interpolate event, number of properties don't match");

      return interpolated_elem;
    }
}

/*
 * GdkEventHistory
 *
 * This section mainly deals with the accumulation and interpolation of
 * relative properties. These include the delta_x and delta_y of precise scroll
 * events, the angle_delta of pinch events etc. Basically any properties for
 * which we receive relative (delta) values.
 *
 * The relative history uses an absolute history internally, both for history
 * bookkeeping and for doing the actual interpolation. Relative properties are
 * converted to absolute ones, by accumulating them, before saving the event in
 * the history buffer. After the interpolation, the relevant properties are
 * converted back to relative ones, by calculating the delta from the
 * accumulated values of the already-emitted events.
 *
 * Absolute properties are simply saved unchanged in the history buffer.
 */

struct _GdkEventHistory
{
  GdkAbsoluteEventHistory *absolute_history;

  /*
   * start_event and stop_event hold the special 'signaling' events.
   *
   * start_event holds the event signaling the start of a gesture. For example
   * a GdkEventTouchpadPinch with phase GDK_TOUCHPAD_GESTURE_PHASE_BEGIN, a
   * GdkEventTouch with type GDK_TOUCH_BEGIN etc.
   *
   * stop_event holds the event signaling the end of a gesture. For example
   * a GdkEventTouchpadPinch with phase GDK_TOUCHPAD_GESTURE_PHASE_END, a
   * GdkEventScroll with is_stop set to TRUE etc.
   */
  GdkEvent* start_event;
  GdkEvent* stop_event;

  /*
   * accumulated_interpolated_event holds the accumulated interpolated values.
   * These are used to calculate the corresponding properties for synthesized
   * events.
   *
   * For example, for precise scroll events, the 'delta_x' and 'delta_y'
   * properties will be accumulated.
   */
  GdkEvent* accumulated_interpolated_event;

  /*
   * 'Scratch' buffers for interpolation. Declared at the 'class' level in
   * order to avoid unnecessary allocations.
   */
  GArray *property_values;
  GArray *property_values_aux;

  /*
   * Just for debug warning
   */
  gint64 previous_interpolation_point;
};

/*
 * gdk_event_history_new:
 *
 * Allocate an events history.
 *
 * Returns: a newly-allocated #GdkEventHistory. The returned #GdkEventHistory
 *   should be freed with gdk_event_history_free().
 */
GdkEventHistory *
gdk_event_history_new (void)
{
  GdkEventHistory *history;

  history = g_slice_new (GdkEventHistory);

  history->absolute_history = gdk_absolute_event_history_new ();
  history->start_event = NULL;
  history->stop_event = NULL;
  history->accumulated_interpolated_event = NULL;
  history->property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  history->property_values_aux = g_array_new (FALSE, FALSE, sizeof (gdouble));
  history->previous_interpolation_point = 0;

  gdk_event_history_reset (history);

  return history;
}

/*
 * gdk_event_history_free:
 * @history: a #GdkEventHistory
 *
 * Frees a #GdkEventHistory along with any associated resources.
 */
void
gdk_event_history_free (GdkEventHistory *history)
{
  if (history->accumulated_interpolated_event)
    {
      gdk_event_free (history->accumulated_interpolated_event);
      history->accumulated_interpolated_event = NULL;
    }

  g_array_free (history->property_values, TRUE);
  g_array_free (history->property_values_aux, TRUE);

  gdk_absolute_event_history_free (history->absolute_history);
  g_slice_free (GdkEventHistory, history);
}

/*
 * Adds a dummy null event as the first absolute input position. This allows us
 * to reduce visible latency since we can immediately react to the first input
 * event.
 *
 * TODO when we add support for non-linear interpolation methods, we can
 * start with linear, since we'll have 2 data points once the first event is
 * received. Then with each additional event we can use more and more
 * sophisticated methods.
 */
static GdkEvent *
gdk_event_history_push_dummy (GdkEventHistory *history,
                              GdkEvent        *event)
{
  GdkEvent *dummy_event = gdk_event_copy (event);
  guint32 dummy_event_time;
  GArray *values = history->property_values;
  int i;

  /* We only care about the number of values here. Once we know that number we
     can reset these values. */
  gdk_event_get_relative_values_for_interpolation (dummy_event, values);

  for (i = 0; i < values->len; i++)
    g_array_index (values, gdouble, i) = 0;

  gdk_event_set_interpolated_relative_values (dummy_event, values);

  /* Set the dummy event time to about one polling interval before the given event */
  dummy_event_time = gdk_event_get_time (dummy_event) - EVENT_HISTORY_DUMMY_POLLING_INTERVAL;
  gdk_event_set_time (dummy_event, dummy_event_time);

  gdk_absolute_event_history_history_push (history->absolute_history,
                                           dummy_event);

  return dummy_event;
}

/*
 * Return TRUE if interpolation is supported for the given event type, FALSE
 * otherwise.
 */
static gboolean
gdk_event_history_supported (GdkEvent *event)
{
  switch (gdk_event_get_event_type (event))
  {
    case GDK_SCROLL:
      /* Only support precise scroll events */
      return (event->scroll.direction == GDK_SCROLL_SMOOTH);
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      return TRUE;
    default:
      /* The event is unsupported */
      break;
  }
  return FALSE;
}

/*
 * gdk_event_history_push:
 * @history: a #GdkEventHistory
 * @event: a #GdkEvent.
 *
 * Adds an event to the history buffer.
 *
 * Relative event properties are converted to absolute ones by accumulating
 * them before adding the event to the history.
 */
void
gdk_event_history_push (GdkEventHistory *history,
                        GdkEvent        *event)
{
  GdkEvent* newest_absolute_event;
  GdkEvent *saved_event;
  GArray *acc_uninterpolated_values = history->property_values;
  GArray *new_values = history->property_values_aux;
  int i;

  g_return_if_fail (gdk_event_history_supported (event));

  if (history->stop_event)
    {
      g_warning ("Can't add events to a history buffer which has already received a stop event");
      return;
    }

  /* Add a dummy null event as the first absolute input position */
  if (!gdk_absolute_event_history_history_length (history->absolute_history))
    {
      GdkEvent *dummy_event = gdk_event_history_push_dummy (history, event);
      history->accumulated_interpolated_event = gdk_event_copy (dummy_event);
    }

  /* The newest event in the event history holds the accumulated uninterpolated properties */
  newest_absolute_event =
    gdk_absolute_event_history_get_newest_event (history->absolute_history);

  gdk_event_get_relative_values_for_interpolation (newest_absolute_event, acc_uninterpolated_values);
  gdk_event_get_relative_values_for_interpolation (event, new_values);

  g_return_if_fail (acc_uninterpolated_values->len == new_values->len);

  /* Convert relative properties to absolute ones by accumulating them */
  for (i = 0; i < acc_uninterpolated_values->len; i++)
    g_array_index (acc_uninterpolated_values, gdouble, i) += g_array_index (new_values, gdouble, i);

  /* save the new event in the history buffer */
  saved_event = gdk_event_copy (event);
  gdk_event_set_interpolated_relative_values (saved_event, acc_uninterpolated_values);
  gdk_absolute_event_history_history_push (history->absolute_history,
                                           saved_event);
}

/*
 * gdk_event_history_length:
 * @history: a #GdkEventHistory
 *
 * Returns: The number of elements in the event history buffer, including the
 *   dummy event.
 */
guint
gdk_event_history_length (GdkEventHistory *history)
{
  return gdk_absolute_event_history_history_length (history->absolute_history);
}

/*
 * Resets the event history and associated properties.
 *
 * 'Start' and 'Stop' events are *not* reset as part of the history reset. That's
 * because the history can be reset while a gesture is still in progress, for
 * example if the user stopped moving his fingers but didn't lift them off the
 * touchpad. Another reason is that once a stop event has been received it's
 * illegal to receive any more events for the same gesture history.
 */
void
gdk_event_history_reset (GdkEventHistory *history)
{
  if (history->accumulated_interpolated_event)
    {
      gdk_event_free (history->accumulated_interpolated_event);
      history->accumulated_interpolated_event = NULL;
    }

  gdk_absolute_event_history_history_reset (history->absolute_history);
}

/*
 * warn_if_going_back_in_time:
 * @history: a #GdkEventHistory
 * @frame_time: (in): the interpolation point, given in microseconds
 *
 * Just for sanity, issue a warning if the requested interpolation point is
 * earlier than the previous one.
 */
static void
warn_if_going_back_in_time (GdkEventHistory *history,
                            gint64           frame_time)
{
  if (frame_time < history->previous_interpolation_point)
    g_warning ("Trying to interpolate a point in time earlier then the last one");

  history->previous_interpolation_point = frame_time;
}

/*
 * gdk_event_history_interpolate_event:
 * @history: a #GdkEventHistory
 * @frame_time: (in): the interpolation point, given in microseconds
 *
 * Generates an event with the relevant properties interpolated to the given
 * frame_time.
 *
 * Unlike gdk_absolute_event_history_interpolate_event() which has no side
 * effects, gdk_event_history_interpolate_event() has a mutable state - it
 * keeps track of the interpolated x,y position. That means that even without
 * any new events arriving, consecutive calls with the same frame_time can
 * yield different results.
 *
 * Returns: (nullable): a newly-allocated, interpolated event or %NULL if it's
 *   impossible to create one.
 */
GdkEvent *
gdk_event_history_interpolate_event (GdkEventHistory *history,
                                     gint64           frame_time)
{
  GdkEvent *interpolated_event;
  GArray *acc_interpolated_values = history->property_values;
  GArray *new_values = history->property_values_aux;
  int i;

  warn_if_going_back_in_time (history, frame_time);

  interpolated_event =
    gdk_absolute_event_history_interpolate_event (history->absolute_history,
                                                  frame_time);

  if (!interpolated_event)
    return NULL;

  /* Calculate the relative properties */
  gdk_event_get_relative_values_for_interpolation (history->accumulated_interpolated_event, acc_interpolated_values);
  gdk_event_get_relative_values_for_interpolation (interpolated_event, new_values);

  g_return_val_if_fail (acc_interpolated_values->len == new_values->len, NULL);

  /* Convert the absolute interpolated properties to relative ones */
  for (i = 0; i < acc_interpolated_values->len; i++)
    {
      /* Calculate delta value for synthesized interpolated event */
      g_array_index (new_values, gdouble, i) -= g_array_index (acc_interpolated_values, gdouble, i);

      /* Accumulate interpolated values */
      g_array_index (acc_interpolated_values, gdouble, i) += g_array_index (new_values, gdouble, i);
    }

  /* Save the values */
  gdk_event_set_interpolated_relative_values (history->accumulated_interpolated_event, acc_interpolated_values);
  gdk_event_set_interpolated_relative_values (interpolated_event, new_values);

  return interpolated_event;
}

/*
 * Returns the timestamp of the most recent event in the event history.
 *
 * It's the responsibility of the caller to check if the history is empty
 * before calling this function.
 */
guint32
gdk_event_history_newest_event_time (GdkEventHistory *history)
{
  return gdk_absolute_event_history_newest_event_time (history->absolute_history);
}

/*
 * Sets the 'start' event. This event should be freed by the caller.
 */
void
gdk_event_history_set_start_event (GdkEventHistory *history,
                                   GdkEvent        *event)
{
  GdkEvent *saved_event = event ? gdk_event_copy (event) : NULL;
  history->start_event = saved_event;
}

/*
 * Returns the 'start' event, NULL if it wasn't set.
 */
GdkEvent *
gdk_event_history_get_start_event (GdkEventHistory *history)
{
  return history->start_event;
}

/*
 * Sets the 'stop' event. This event should be freed by the caller.
 */
void
gdk_event_history_set_stop_event (GdkEventHistory *history,
                                  GdkEvent        *event)
{
  GdkEvent *saved_event = event ? gdk_event_copy (event) : NULL;
  history->stop_event = saved_event;
}

/*
 * Returns the 'stop' event, NULL if it wasn't set.
 */
GdkEvent *
gdk_event_history_get_stop_event (GdkEventHistory *history)
{
  return history->stop_event;
}

/*
 * Return the average time between consecutive events, or '0' if the history
 * is too short.
 *
 * Use the last several events in order to prevent a skew in case of gaps in
 * the event stream. Gaps can happen, for example, when the user doesn't move
 * his fingers for a while.
 */
guint32
gdk_event_history_average_event_interval (GdkEventHistory *history)
{
  return gdk_absolute_event_history_average_event_interval (history->absolute_history);
}

/*
 * Check if all existing events have already been interpolated.
 *
 * Returns 'true' if the interpolation point is equal to or greater than the
 * timestamp of the newest event.
 *
 * An empty history is considered to be interpolated.
 */
gboolean
gdk_event_history_all_existing_events_emitted (GdkEventHistory *history,
                                               gint64           interpolation_point)
{
  /* If the interpolation point is equal to or greater than the timestamp of
     the newest event in the history, then all events in the history buffer
     have been interpolated. */
  guint32 interpolation_point_ms = (interpolation_point / 1000);
  guint32 newest_uninterpolated_event_time =
    gdk_event_history_newest_event_time (history);

  /* An empty history is considered to be interpolated */
  if (!gdk_event_history_length (history))
    return TRUE;

  return (interpolation_point_ms >= newest_uninterpolated_event_time);
}
