/* GDK - The GIMP Drawing Kit
 *
 * gdkeventinterpolation.c: input event interpolation
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

#include "gdkeventinterpolationprivate.h"
#include "gdkeventsprivate.h"

#include "fallback-c89.c"

/*
 * TODO: explain how the interpolation works.
 */

/**
 * We need at least 2-3 display-frames worth of input events in the history
 * buffer to account for system-induced (display manager etc) latency - the
 * time it takes an event to reach this code. Display frames can last anywhere
 * between 33ms and about 4ms on today's monitors, corresponding to 30 FPS to
 * 240 FPS.
 *
 * We can also assume that input devices generating 250 events per second or
 * more will not require interpolation. Taking the extremes here, a combination
 * of an input device that generates an event every 4ms coupled with a monitor
 * capable of only 30 frames per second, we'll need about 8 events per frame so
 * a total of about 16 events in the buffer. In addition, to support fancier
 * interpolation methods in the future we'll need about 8 events in total and
 * maybe even more, so 16 events covers that as well.
 */
#define EVENT_HISTORY_MAX_ELEMENTS 16

/**
 * Used to determine the timestamp of a dummy 'null' absolute input event.
 * Corresponds to 1000 / 12 =~ 83 events/second which should be good enough
 * for the slowest (lowest event rate) input devices. Anyway this number is
 * not very important, it just has to be more or less equal to the time
 * interval between consecutive input events.
 */
#define RELATIVE_EVENT_HISTORY_DUMMY_EVENT_TIME 12

/*
 * Absolute events interpolaiton
 *
 * This section deals with the interpolation of absolute input events. These
 * include motion events, touch events etc. Basically any event for which we
 * receive absolute x,y coordinates.
 */

struct _GdkAbsoluteEventInterpolation
{
  GPtrArray *event_history;
};

GdkAbsoluteEventInterpolation *
gdk_absolute_event_interpolation_new (void)
{
  GdkAbsoluteEventInterpolation *interpolator;

  /* TODO use a ring buffer for event_history? */
  interpolator = g_slice_new (GdkAbsoluteEventInterpolation);
  interpolator->event_history = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  return interpolator;
}

void
gdk_absolute_event_interpolation_free (GdkAbsoluteEventInterpolation  *interpolator)
{
  g_ptr_array_unref (interpolator->event_history);
  g_slice_free (GdkAbsoluteEventInterpolation, interpolator);
}

void
gdk_absolute_event_interpolation_history_push (GdkAbsoluteEventInterpolation  *interpolator,
                                               GdkEvent                       *event)
{
  /* only support scroll events for now */
  g_return_if_fail (gdk_event_get_event_type (event) == GDK_SCROLL);
  g_return_if_fail (event->scroll.direction == GDK_SCROLL_SMOOTH);

  g_ptr_array_add (interpolator->event_history, g_object_ref (event));

  if (interpolator->event_history->len > EVENT_HISTORY_MAX_ELEMENTS)
    g_ptr_array_remove_index (interpolator->event_history, 0);
}

guint
gdk_absolute_event_interpolation_history_length (GdkAbsoluteEventInterpolation  *interpolator)
{
  return interpolator->event_history->len;
}

guint32
gdk_absolute_event_interpolation_offset_from_latest (GdkAbsoluteEventInterpolation  *interpolator,
                                                     gint64                          frame_time)
{
  gint64 timestamp_offset_from_latest_event;
  GdkEvent *latest_elem = NULL;

  latest_elem = g_ptr_array_index (interpolator->event_history, interpolator->event_history->len - 1);
  timestamp_offset_from_latest_event = (frame_time / 1000) - gdk_event_get_time (latest_elem);

  return (guint32)timestamp_offset_from_latest_event;
}

void
gdk_absolute_event_interpolation_history_reset (GdkAbsoluteEventInterpolation  *interpolator)
{
  if (interpolator->event_history->len == 0)
    return;

  g_ptr_array_remove_range (interpolator->event_history, 0,
                            interpolator->event_history->len);
}

/*
 * gdk_absolute_event_interpolation_interpolate_event has no side effects.
 * However that doesn't make it idempotent. For example when frame_time is
 * larger than the largest time stamp in the event fifo, the result will be
 * identical to the latest event. However if a later event is then added,
 * calling with the same frame_time can lead to different (interpolated)
 * values.
 *
 * FIXME handle wrap-around of frame_time and evtime. In addition frame_time is
 * gint64 and counts microseconds whereas evtime is guint32 and counts
 * milliseconds, handle that as well. Note that even though the scale of
 * frame_time is 1,000 times that of evtime, it still has a larger range of
 * values.
 */
GdkEvent *
gdk_absolute_event_interpolation_interpolate_event (GdkAbsoluteEventInterpolation  *interpolator,
                                                    gint64                          frame_time)
{
  gint i;
  GdkEvent *elem = NULL;
  guint32 interpolation_point;

  if (interpolator->event_history->len == 0)
    {
      /* no history, bail for now */
      g_error ("Can't interpolate event, history is empty");
      return NULL;
    }

  interpolation_point = (frame_time / 1000);

  /* find the first timestamp equal to or lower than the interpolation point */
  for (i = interpolator->event_history->len - 1; i >= 0; i--)
    {
      elem = g_ptr_array_index (interpolator->event_history, i);

      if (gdk_event_get_time (elem) <= interpolation_point)
        break;
    }

  if (gdk_event_get_time (elem) == interpolation_point)
    {
      /* no interpolation necessary */
      return gdk_event_copy(elem);
    }
  else if (i == (interpolator->event_history->len - 1))
    {
      /* the interpolation point is more recent than all events in the history,
         use the last known value. TODO extrapolate the value? */
      return gdk_event_copy(elem);
    }
  else if (i >= 0)
    {
      /* we have 2 points */
      GdkEvent *first_elem;
      GdkEvent *second_elem;
      GdkEvent *interpolated_elem;
      guint32 first_elem_time;
      guint32 second_elem_time;
      gdouble first_elem_x;
      gdouble first_elem_y;
      gdouble second_elem_x;
      gdouble second_elem_y;
      gdouble ratio;
      gdouble interpolated_x;
      gdouble interpolated_y;
      GdkModifierType state;

      first_elem =  g_ptr_array_index (interpolator->event_history, i);
      second_elem = g_ptr_array_index (interpolator->event_history, i + 1);

      first_elem_time =  gdk_event_get_time (first_elem);
      second_elem_time = gdk_event_get_time (second_elem);

      ratio = (gdouble) (interpolation_point - first_elem_time) /
                        (second_elem_time    - first_elem_time);

      gdk_event_get_coords (first_elem, &first_elem_x, &first_elem_y);
      gdk_event_get_coords (second_elem, &second_elem_x, &second_elem_y);

      interpolated_x = (ratio * second_elem_x) + ((1.0 - ratio) * first_elem_x);
      interpolated_y = (ratio * second_elem_y) + ((1.0 - ratio) * first_elem_y);

      /* synthesize a new event */
      interpolated_elem = gdk_event_copy (first_elem);
      gdk_event_set_coords (interpolated_elem, interpolated_x, interpolated_y);

      /*
       * state is discreet so use a nearest-neighbor interpolation.
       * TODO there is no gdk_event_set_state so explicitly set scroll event state.
       *      for generic code we'll need to create gdk_event_set_state().
       */
      GdkEvent *state_elem = (ratio < 0.5) ? first_elem : second_elem;
      gdk_event_get_state (state_elem, &state);
      interpolated_elem->scroll.state = state;

      /*
       * TODO there is no gdk_event_set_time so explicitly set scroll event state.
       *      for generic code we'll need to create gdk_event_set_time().
       */
      interpolated_elem->scroll.time = interpolation_point;

      return interpolated_elem;
    }

  /* the interpolation point lies before the earliest event, bail for now */
  g_error ("Can't interpolate event, frame time earlier than first history element");

  return NULL;
}

/*
 * Relative events interpolaiton
 *
 * This section deals with the interpolation of relative input events. These
 * include scroll events, swipe events etc. Basically any event for which we
 * receive relative (delta) x,y coordinates.
 */

struct _GdkRelativeEventInterpolation
{
  GdkAbsoluteEventInterpolation *absolute_interpolator;

  /*
   * Latest uninterpolated x,y serve to keep track of the absolute position
   * of the input device by summing all of the input deltas.
   */
  gdouble latest_uninterpolated_x;
  gdouble latest_uninterpolated_y;

  /*
   * Latest interpolated x,y serve to keep track of the absolute interpolated
   * position, in order to calculate the position deltas for synthesized
   * events.
   */
  gdouble latest_interpolated_x;
  gdouble latest_interpolated_y;

  gint64 last_frame_time;
};

GdkRelativeEventInterpolation * gdk_relative_event_interpolation_new (void)
{
  GdkRelativeEventInterpolation *interpolator;

  interpolator = g_slice_new (GdkRelativeEventInterpolation);
  interpolator->absolute_interpolator = gdk_absolute_event_interpolation_new ();

  gdk_relative_event_interpolation_history_reset (interpolator);

  return interpolator;
}

void gdk_relative_event_interpolation_free (GdkRelativeEventInterpolation  *interpolator)
{
  gdk_absolute_event_interpolation_free (interpolator->absolute_interpolator);
  g_slice_free (GdkRelativeEventInterpolation, interpolator);
}

void gdk_relative_event_interpolation_history_push (GdkRelativeEventInterpolation  *interpolator,
                                                    GdkEvent                       *event)
{
  gdouble delta_x;
  gdouble delta_y;

  g_return_if_fail (gdk_event_get_event_type (event) == GDK_SCROLL);
  g_return_if_fail (event->scroll.direction == GDK_SCROLL_SMOOTH);

  /*
   * add a dummy null event as the first absolute input position. this allows
   * us to reduce visible latency since we can immediately react to the first
   * input event.
   *
   * TODO when supporting interpolation methods requiring more data points,
   * more dummy events will have to created when the first real event arrives.
   */
  if (!gdk_absolute_event_interpolation_history_length (interpolator->absolute_interpolator))
    {
      GdkEvent *dummy_event = gdk_event_copy (event);

      gdk_event_set_coords (dummy_event, 0, 0);
      event->scroll.delta_x = 0;
      event->scroll.delta_y = 0;
      dummy_event->scroll.time = gdk_event_get_time (event) - RELATIVE_EVENT_HISTORY_DUMMY_EVENT_TIME;

      gdk_absolute_event_interpolation_history_push (interpolator->absolute_interpolator,
                                                     dummy_event);
    }

  /* convert relative events to absolute events and save them in the history buffer */
  gdk_event_get_scroll_deltas (event, &delta_x, &delta_y);

  /* TODO can we use the absolute event coordinates here? */
  interpolator->latest_uninterpolated_x += delta_x;
  interpolator->latest_uninterpolated_y += delta_y;

  /* FIXME do we really have to update the absolute event coordinates here? */
  gdk_event_set_coords (event,
                        interpolator->latest_uninterpolated_x,
                        interpolator->latest_uninterpolated_y);

  gdk_absolute_event_interpolation_history_push (interpolator->absolute_interpolator,
                                                 event);
}

guint
gdk_relative_event_interpolation_history_length (GdkRelativeEventInterpolation  *interpolator)
{
  return gdk_absolute_event_interpolation_history_length (interpolator->absolute_interpolator);
}

void
gdk_relative_event_interpolation_history_reset (GdkRelativeEventInterpolation  *interpolator)
{
  interpolator->latest_uninterpolated_x = 0;
  interpolator->latest_uninterpolated_y = 0;

  interpolator->latest_interpolated_x = 0;
  interpolator->latest_interpolated_y = 0;

  interpolator->last_frame_time = 0;

  gdk_absolute_event_interpolation_history_reset (interpolator->absolute_interpolator);
}

/*
 * unlike gdk_absolute_event_interpolation_interpolate_event which has no side
 * effects, gdk_relative_event_interpolation_interpolate_event has a mutable
 * state as it keeps track of the interpolated x,y position. That means that
 * even without any new events arriving, conscutive calls with the same
 * frame_time can yield different results.
 */
GdkEvent *
gdk_relative_event_interpolation_interpolate_event (GdkRelativeEventInterpolation  *interpolator,
                                                    gint64                          frame_time)
{
  GdkEvent *interpolated_event;
  gdouble interpolated_x;
  gdouble interpolated_y;

  interpolated_event = 
    gdk_absolute_event_interpolation_interpolate_event (interpolator->absolute_interpolator,
                                                        frame_time);

  if (!interpolated_event)
    return NULL;

  /* calculate the relative event */
  gdk_event_get_coords (interpolated_event, &interpolated_x, &interpolated_y);

  interpolated_event->scroll.delta_x = interpolated_x - interpolator->latest_interpolated_x;
  interpolated_event->scroll.delta_y = interpolated_y - interpolator->latest_interpolated_y;

  /* save the current interpolated position */
  interpolator->latest_interpolated_x = interpolated_x;
  interpolator->latest_interpolated_y = interpolated_y;

  return interpolated_event;
}

guint32
gdk_relative_event_interpolation_offset_from_latest (GdkRelativeEventInterpolation  *interpolator,
                                                     gint64                          frame_time)
{
  return gdk_absolute_event_interpolation_offset_from_latest (interpolator->absolute_interpolator,
                                                              frame_time);
}

