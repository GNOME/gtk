/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Yariv Barkan
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
#include "gtkeventinterpolationprivate.h"

#include <stdio.h>

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

struct _GtkAbsoluteEventInterpolation
{
  GArray *event_history;
};

GtkAbsoluteEventInterpolation *
gtk_absolute_event_interpolation_new (void)
{
  GtkAbsoluteEventInterpolation *interpolator;

  /* TODO use a ring buffer for event_history? */
  interpolator = g_slice_new (GtkAbsoluteEventInterpolation);
  interpolator->event_history = g_array_new (FALSE, FALSE,
                                             sizeof (AbsoluteEventHistoryElement));

  return interpolator;
}

void
gtk_absolute_event_interpolation_free (GtkAbsoluteEventInterpolation  *interpolator)
{
  g_array_unref (interpolator->event_history);
  g_slice_free (GtkAbsoluteEventInterpolation, interpolator);
}

void
gtk_absolute_event_interpolation_history_push (GtkAbsoluteEventInterpolation  *interpolator,
                                               guint32                         evtime,
                                               guint                           modifier_state,
                                               gdouble                         x,
                                               gdouble                         y)
{
  AbsoluteEventHistoryElement new_item;

  new_item.evtime = evtime;
  new_item.modifier_state = modifier_state;
  new_item.x = x;
  new_item.y = y;

  g_array_append_val (interpolator->event_history, new_item);

  if (interpolator->event_history->len > EVENT_HISTORY_MAX_ELEMENTS)
    g_array_remove_range (interpolator->event_history, 0, 1);
}

static guint
gtk_absolute_event_interpolation_history_length (GtkAbsoluteEventInterpolation  *interpolator)
{
  return interpolator->event_history->len;
}

guint32
gtk_absolute_event_interpolation_offset_from_latest (GtkAbsoluteEventInterpolation  *interpolator,
                                                     gint64                          frame_time)
{
  gint64 timestamp_offset_from_latest_event;
  AbsoluteEventHistoryElement *latest_elem = NULL;

  latest_elem = &g_array_index (interpolator->event_history, AbsoluteEventHistoryElement, interpolator->event_history->len -1);
  timestamp_offset_from_latest_event = (frame_time / 1000) - latest_elem->evtime;

  return (guint32)timestamp_offset_from_latest_event;
}

void
gtk_absolute_event_interpolation_history_reset (GtkAbsoluteEventInterpolation  *interpolator)
{
  if (interpolator->event_history->len == 0)
    return;

  g_array_remove_range (interpolator->event_history, 0,
                        interpolator->event_history->len);
}

/*
 * gtk_absolute_event_interpolation_interpolate_event has no side effects.
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
void
gtk_absolute_event_interpolation_interpolate_event (GtkAbsoluteEventInterpolation  *interpolator,
                                                    gint64                          frame_time,
                                                    AbsoluteEventHistoryElement    *interpolated_item)
{
  gint i;
  AbsoluteEventHistoryElement *elem = NULL;
  guint32 interpolation_point;
  gdouble interpolated_x;
  gdouble interpolated_y;

  /* zero init the modifier flags by default */
  interpolated_item->modifier_state = 0;

  if (interpolator->event_history->len == 0)
    {
      /* no history, bail for now */
      interpolated_item->x = 0;
      interpolated_item->y = 0;

      return;
    }

  interpolation_point = (frame_time / 1000);

  /* find the first timestamp equal to or lower than the interpolation point */
  for (i = interpolator->event_history->len - 1; i >= 0; i--)
    {
      elem = &g_array_index (interpolator->event_history, AbsoluteEventHistoryElement, i);

      if (elem->evtime <= interpolation_point)
        break;
    }

  if (elem->evtime == interpolation_point)
    {
      /* no interpolation necessary */
      interpolated_x = elem->x;
      interpolated_y = elem->y;
      interpolated_item->modifier_state = elem->modifier_state;
    }
  else if (i == (interpolator->event_history->len - 1))
    {
      /* the interpolation point is more recent than all events in the history,
         use the last known value. TODO extrapolate the value? */
      interpolated_x = elem->x;
      interpolated_y = elem->y;
      interpolated_item->modifier_state = elem->modifier_state;
    }
  else if (i >= 0)
    {
      /* we have 2 points */
      AbsoluteEventHistoryElement *first_elem;
      AbsoluteEventHistoryElement *second_elem;
      gdouble ratio;

      first_elem =  &g_array_index (interpolator->event_history, AbsoluteEventHistoryElement, i);
      second_elem = &g_array_index (interpolator->event_history, AbsoluteEventHistoryElement, i + 1);

      ratio = (gdouble)(interpolation_point - first_elem->evtime) /
                       (second_elem->evtime - first_elem->evtime);

      interpolated_x = (ratio * second_elem->x) + ((1.0 - ratio) * first_elem->x);
      interpolated_y = (ratio * second_elem->y) + ((1.0 - ratio) * first_elem->y);

      /* modifier_state is discreet so use a nearest-neighbor interpolation */
      interpolated_item->modifier_state = (ratio < 0.5) ?
        first_elem->modifier_state :
        second_elem->modifier_state;
    }
  else
    {
      /* the interpolation point lies before the earliest event, bail for now */
      interpolated_x = 0;
      interpolated_y = 0;
    }

  interpolated_item->x = interpolated_x;
  interpolated_item->y = interpolated_y;
}

/*
 * Relative events interpolaiton
 *
 * This section deals with the interpolation of relative input events. These
 * include scroll events, swipe events etc. Basically any event for which we
 * receive relative (delta) x,y coordinates.
 */

struct _GtkRelativeEventInterpolation
{
  GtkAbsoluteEventInterpolation *absolute_interpolator;

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

GtkRelativeEventInterpolation * gtk_relative_event_interpolation_new (void)
{
  GtkRelativeEventInterpolation *interpolator;

  interpolator = g_slice_new (GtkRelativeEventInterpolation);
  interpolator->absolute_interpolator = gtk_absolute_event_interpolation_new ();

  gtk_relative_event_interpolation_history_reset (interpolator);

  return interpolator;
}

void gtk_relative_event_interpolation_free (GtkRelativeEventInterpolation  *interpolator)
{
  gtk_absolute_event_interpolation_free (interpolator->absolute_interpolator);
  g_slice_free (GtkRelativeEventInterpolation, interpolator);
}

void gtk_relative_event_interpolation_history_push (GtkRelativeEventInterpolation  *interpolator,
                                                    guint32                         evtime,
                                                    guint                           modifier_state,
                                                    gdouble                         delta_x,
                                                    gdouble                         delta_y)
{
  /*
   * add a dummy null event as the first absolute input position. this allows
   * us to reduce visible latency since we can immediately react to the first
   * input event.
   *
   * TODO when supporting interpolation methods requiring more data points,
   * more dummy events will have to created when the first real event arrives.
   */
  if (!gtk_absolute_event_interpolation_history_length (interpolator->absolute_interpolator))
    gtk_absolute_event_interpolation_history_push (interpolator->absolute_interpolator,
                                                   evtime - RELATIVE_EVENT_HISTORY_DUMMY_EVENT_TIME,
                                                   modifier_state,
                                                   0,
                                                   0);

  /* convert relative events to absolute events and save them in the history buffer */
  interpolator->latest_uninterpolated_x += delta_x;
  interpolator->latest_uninterpolated_y += delta_y;

  gtk_absolute_event_interpolation_history_push (interpolator->absolute_interpolator,
                                                 evtime,
                                                 modifier_state,
                                                 interpolator->latest_uninterpolated_x,
                                                 interpolator->latest_uninterpolated_y);
}

void
gtk_relative_event_interpolation_history_reset (GtkRelativeEventInterpolation  *interpolator)
{
  interpolator->latest_uninterpolated_x = 0;
  interpolator->latest_uninterpolated_y = 0;

  interpolator->latest_interpolated_x = 0;
  interpolator->latest_interpolated_y = 0;

  interpolator->last_frame_time = 0;

  gtk_absolute_event_interpolation_history_reset (interpolator->absolute_interpolator);
}

/*
 * unlike gtk_absolute_event_interpolation_interpolate_event which has no side
 * effects, gtk_relative_event_interpolation_interpolate_event has a mutable
 * state as it keeps track of the interpolated x,y position. That means that
 * even without any new events arriving, conscutive calls with the same
 * frame_time can yield different results.
 */
void
gtk_relative_event_interpolation_interpolate_event (GtkRelativeEventInterpolation  *interpolator,
                                                    gint64                          frame_time,
                                                    guint                          *modifier_state,
                                                    gdouble                        *delta_x,
                                                    gdouble                        *delta_y)
{
  AbsoluteEventHistoryElement absolute_interpolated_item;

  gtk_absolute_event_interpolation_interpolate_event (interpolator->absolute_interpolator,
                                                      frame_time,
                                                      &absolute_interpolated_item);

  /* calculate the relative event */
  *delta_x = absolute_interpolated_item.x - interpolator->latest_interpolated_x;
  *delta_y = absolute_interpolated_item.y - interpolator->latest_interpolated_y;

  *modifier_state = absolute_interpolated_item.modifier_state;

  /* save the current interpolated position */
  interpolator->latest_interpolated_x = absolute_interpolated_item.x;
  interpolator->latest_interpolated_y = absolute_interpolated_item.y;
}

guint32
gtk_relative_event_interpolation_offset_from_latest (GtkRelativeEventInterpolation  *interpolator,
                                                     gint64                          frame_time)
{
  return gtk_absolute_event_interpolation_offset_from_latest (interpolator->absolute_interpolator,
                                                              frame_time);
}

