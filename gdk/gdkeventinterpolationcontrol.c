/* GDK - The GIMP Drawing Kit
 *
 * gdkeventinterpolationcontrol.c: input event interpolation
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

#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdk/gdkeventhistoryprivate.h"

/*
 * Maximum allowed time interval, in milliseconds, between the upcoming display
 * frame and the input event interpolation point. This limitation is necessary
 * in order to not get stuck with high latency due to transient hiccups in
 * the stream of received input events.
 *
 * Assuming a 60Hz display, frame time would be about 16.6ms. Since the most
 * recent input events in the interpolation history are from the previous frame
 * at the latest, there will be at least 16.6ms gap between the upcoming frame
 * and the most recent event. If due to some reason (display manager delay etc)
 * input events arrived a frame late, we already have at least 33ms gap.
 * Experimentally 40ms seems like a good hard limit for 60Hz displays, so about
 * 2.5 display frames.
 *
 * While the semi-arbitrary 40ms threshold works fine for screens having 60Hz
 * or higher refresh, it doesn't fit well screens with lower refresh rate. For
 * example, for laptops the screen refresh is often 50Hz, which means
 * 20ms/frame. So 40ms is only 2 frames which would not be enough. Then there
 * are 30Hz screens to consider, such as a 4K display over an HDMI v1.x
 * connection. So we set some relatively high hard latency limit, and
 * dynamically calculate a 'soft' limit based on the display refresh rate.
 */

#define MAX_INTERPOLATION_OFFSET_MS 100
#define INTERPOLATION_OFFSET_MAX_DISPLAY_FRAMES 3
#define INTERPOLATION_OFFSET_MAX_EVENT_FRAMES 3

/*
 * Grace period multipliers - when to stop the interpolation callback if no
 * input events were received for a while.
 */

#define INTERPOLATION_DISPLAY_GRACE 5
#define INTERPOLATION_EVENT_GRACE 10

typedef enum {
  GDK_INTERP_STATUS_ONGOING,
  GDK_INTERP_STATUS_TIMEOUT,
  GDK_INTERP_STATUS_DONE
} GdkInterpolationDeviceStatus;

/*
 * GdkDeviceInterpolator maintains a queue of active gestures for a specific
 * (slave) input device.
 *
 * A 'gesture' in this context is a sequence of input events, from the 'start'
 * event, via 'update' events, up to the 'stop' event. For example, a pinch
 * gesture would start with the 'phase begin', continue with update events and
 * end with either a 'phase end' or a 'phase cancel'. For each active gesture a
 * separate event history is maintained.
 *
 * The gesture queue is required because it is possible to start a new gesture
 * while the event history of a previous gesture is still being drained. If a
 * new gesture is started while another is still in effect, then the new events
 * are stored in a separate event history buffer, which is added to the queue.
 * Once interpolating the first gesture is done, we start emitting events for
 * the next one.
 *
 * In certain circumstances it would be possible to get several gestures in the
 * queue - for example, if the interval between consecutive frames is, say,
 * several seconds. This might happen under heavy system or application load.
 *
 * While the code allows for an unlimited number of queued gestures, in reality
 * it's pretty hard to trigger a second gesture before the previous one
 * finished. Usually only a single event history would be used.
 *
 * Interpolated events are extracted from the oldest gesture in the queue.
 * Newly received events are pushed to the newest gesture in the queue. Since
 * most of the time there will only be a single ongoing gesture, events will
 * usually be pushed to and extracted from the same history.
 *
 * Once all events of a specific gesture are exhausted, the gesture is
 * discarded and we start emitting events from the next one until. This is
 * repeated until there are no more gestures in the queue.
 */
typedef struct _GdkDeviceInterpolator {
  GPtrArray *gesture_queue;
  gint64 last_frame_event_received;
  gint64 last_time_event_received;
  gint64 event_interval_us;
  gint32 time_offset_target;
} GdkDeviceInterpolator;

/*
 * GdkEventInterpolationControl maintains a list of active input devices.
 *
 * Newly received, uninterpolated events are added to a per-input-device
 * gesture queue. Once some event is received, a frame "update" callback is
 * started, in order to emit interpolated events. These are emitted once per
 * frame for every active input device.
 */

struct _GdkEventInterpolationControl
{
  /* The owning window */
  GdkWindow *window;

  /* The following are shared across all ongoing gestures from all input devices */
  gint32 global_time_offset;
  gint32 global_time_offset_target;
  gulong interpolation_tick_id;
  gint64 previous_frame_time;
  gint64 previous_callback_time;
  gint64 previous_interpolation_point;

  /* The values are of type GdkDeviceInterpolator */
  GHashTable *device_interpolators;
};

static void stop_interpolation_callback (GdkEventInterpolationControl *control);

static gint
signum(gint x) {
    return (x > 0) - (x < 0);
}

static void
free_device_interpolators (GdkEventInterpolationControl *control)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, control->device_interpolators);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDeviceInterpolator *device_interpolator = value;

      if (device_interpolator->gesture_queue)
        g_ptr_array_unref (device_interpolator->gesture_queue);

      g_slice_free (GdkDeviceInterpolator, device_interpolator);
    }

  g_hash_table_destroy (control->device_interpolators);
}

/*
 * Allocate an event interpolation controller.
 *
 * Should be freed with gdk_event_interpolation_control_free().
 */
GdkEventInterpolationControl *
gdk_event_interpolation_control_new (GdkWindow *window)
{
  GdkEventInterpolationControl *control = g_slice_new (GdkEventInterpolationControl);

  control->window = window;
  control->global_time_offset = G_MININT32;
  control->global_time_offset_target = G_MININT32;
  control->interpolation_tick_id = 0;
  control->previous_frame_time = 0;
  control->previous_callback_time = 0;
  control->previous_interpolation_point = 0;
  control->device_interpolators = g_hash_table_new (NULL, NULL);

  return control;
}

/*
 * Free an event interpolation controller.
 */
void
gdk_event_interpolation_control_free (GdkEventInterpolationControl *control)
{
  stop_interpolation_callback (control);
  if (control->device_interpolators)
    free_device_interpolators (control);
  g_slice_free (GdkEventInterpolationControl, control);
}

/*
 * calculate_device_interpolation_offset:
 * @device_interpolator: a #GdkDeviceInterpolator
 * @frame_counter: the consecutive id of the next display frame
 * @frame_time: the time the next display frame is expected to be submitted
 * @current_time: the wall time when the callback was called
 *
 * Calculates the interpolation latency offset of a single input device. Please
 * see the comment for update_interpolation_offset() for an overview.
 *
 * In the following diagrams time is denoted in milliseconds. The vsync event
 * is marked by the '|' characters. Input events are marked 'I'. The frame
 * clock "update" signal callbacks, marked Cn, occur about 1ms after the vsync.
 * The processing takes place in these callbacks. On each callback we handle
 * the input events from the previous display frame. The delta between the time
 * of the upcoming frame and the most recent event is calculated once per
 * frame, and the maximum delta calculated so far is kept as the offset.
 *
 * Case 1: Input frequency is higher than the screen refresh. This is a common
 * case - for example a standard 125Hz USB mouse connected to a 60Hz monitor.
 * For ease of demonstration we'll assume a fictional display refresh of 100Hz
 * and input frequency of about 250Hz. So vsync is every 10ms and input events
 * occur every 4ms.
 *
 * Time      0        10        20        30        40        50        60
 * VSync     |---------|---------|---------|---------|---------|---------|-
 * Input        I   I   I   I   I : I   I   I   I   I : I   I   I   I   I
 * Callback1            C1        :         :         :
 * Offset1          [------------]:         :         :
 * Callback2                      C2        :         :
 * Offset2                      [----------]:         :
 * Callback3                                C3        :
 * Offset3                              [------------]:
 * Callback4                                          C4
 * Offset4                                          [----------]
 * ...
 *
 * In this case the first callback occurs at the 11ms mark. Two input events
 * were received so far, one at the 3ms mark and one at the 7ms mark. The next
 * vsync is at the 20ms mark, so the delta between the upcoming vsync and the
 * most recent event is (20 - 7) = 13. Since this is the first callback, we set
 * the offset to 13. In a similar fashion the delta in the 2nd callback is (30
 * - 19) = 11. We keep the offset at 13 since it's the biggest delta so far.
 * This process repeats in all the other callbacks.
 *
 * Case 2: Input frequency is lower than the screen refresh. This is the case,
 * for example, when an Apple external 'trackpad', which emits events at a
 * frequency of ~88Hz, is connected to a 240Hz "gaming" monitor. Here we'll
 * assume a fictional display refresh of 200Hz and input frequency of about
 * 83Hz. vsync is every 5ms, input events occur every 12ms.
 *
 * Time      0    5   10   15   20   25   30   35   40   45   50   55   60
 * VSync     |----|----|----|----|----|----|----|----|----|----|----|----|-
 * Input         I :    :    I    :      I           I           I
 * Callback1       C1   :    :    :
 * Offset1       [-----]:    :    :
 * Callback2            C2   :    :
 * Offset2       [----------]:    :
 * Callback3                 C3   :
 * Offset3       [---------------]:
 * Callback4                      C4
 * Offset4                   [--------]
 * ...
 *
 * Here the first callback occurs at the 6ms mark. Only a single input event
 * was received so far, at the 4ms mark. The upcoming vsync is at the 10ms mark
 * so the delta is (10 - 4) = 6. We set the offset to 6 since this is the first
 * delta. The next callback occurs at the 11ms mark. We still only have the
 * first input event so the delta for this callback will be (15 - 4) = 11.
 * Since the delta is bigger than the previous one we now set the offset to 11.
 * In a similar vain the delta, and the offset, for the third callback is 16.
 * In the 4th callback we finally see a new input event, at the 16ms mark, so
 * the delta will be (25 - 16) = 9. However the biggest delta so far is 16 and
 * that will be kept as the offset. This process repeats in all the other
 * callbacks.
 *
 * These diagrams might tempt one to believe that we can simply set the offset
 * to be equal to (event interval + frame interval). However this is not the
 * case. As explained in the comment for update_interpolation_offset(), we
 * calculate the offset from "frame time" rather than presentation time. There
 * is no guarantee for frame time other than that it is usually at a constant,
 * unknown delta from the presentation time. Furthermore, input might be
 * jittery - i.e. not arrive at regular intervals.
 */
static void
calculate_device_interpolation_offset (GdkDeviceInterpolator *device_interpolator,
                                       gint64                 frame_counter,
                                       gint64                 frame_time,
                                       gint64                 current_time)
{
  guint newest_gesture_index = device_interpolator->gesture_queue->len - 1;
  gint32 timestamp_offset_from_newest_event = 0;
  gboolean update_offset = FALSE;

  /* We always add events to the newest gesture */
  GdkEventHistory * accumulating_gesture =
    g_ptr_array_index (device_interpolator->gesture_queue, newest_gesture_index);

  /*
   * Update the target offset If we received an input event in the current or
   * the previous frame. This covers the case of input rate equal to or higher
   * than the display refresh.
   *
   * Note that in the current GDK implementation the frame counter is updated
   * right before the before-paint callback is fired, so the frame counter
   * delta would always be at least 1.
   */
  if (frame_counter - device_interpolator->last_frame_event_received <= 1)
    {
      update_offset = TRUE;
    }

  /*
   * Update the target offset If this is the first event in this gesture. That
   * covers the case of input rate lower than display refresh rate - e.g. a
   * 90Hz input device in conjunction with a 240Hz "gaming" monitor. In this
   * case we'll get several display frames with no input events after receiving
   * the first event, but we still want to update the offset in these frames.
   *
   * Note that it's possible for the history length to be equal to 0 while the
   * gesture is in progress - for example if no update event was received yet,
   * or if the history was reset due to a timeout.
   */
  if ((gdk_event_history_length (accumulating_gesture) == 1) &&
      (device_interpolator->event_interval_us == 0))
    {
      update_offset = TRUE;
    }

  /*
   * Update the target offset If we have more than a single event, and no more
   * than 1.5 * event interval passed since the last received event. This case
   * is for fine-tuning the offset for input devices with input rate lower than
   * the display refresh rate, since we might not get the optimal offset from
   * the first event alone. For example, we might not get an event for 3 frames
   * after the first event, then for 4 frames after the second event.
   */
  if ((gdk_event_history_length (accumulating_gesture) > 1) &&
      ((current_time - device_interpolator->last_time_event_received) <
       (1.5 * device_interpolator->event_interval_us)))
    {
      update_offset = TRUE;
    }

  /* Calculate the time delta between the upcoming frame and the newest event
   * in the event history. The interpolation point offset from the frame time
   * would have to be at least as big.
   *
   * frame_time is in us while event time is in ms. */
  if (update_offset)
    {
      timestamp_offset_from_newest_event = (frame_time / 1000) -
        gdk_event_history_newest_event_time (accumulating_gesture);
    }

  /* Adjust the target offset if it's too small. This single statement is the
   * core of this function. */
  if (device_interpolator->time_offset_target < timestamp_offset_from_newest_event)
    device_interpolator->time_offset_target = timestamp_offset_from_newest_event;
}

/*
 * calculate_global_target_interpolation_offset:
 * @control: a #GdkEventInterpolationControl
 * @frame_counter: the consecutive id of the next display frame
 * @frame_time: the time the next display frame is expected to be submitted
 * @current_time: the wall time when the callback was called
 * @largest_event_interval (out) (allow-none): the event interval of the
 *     slowest input device
 *
 * Calculates the global interpolation latency offset target.
 */
static void
calculate_global_target_interpolation_offset (GdkEventInterpolationControl *control,
                                              gint64                        frame_counter,
                                              gint64                        frame_time,
                                              gint64                        current_time,
                                              guint32                      *largest_event_interval)
{
  GHashTableIter iter;
  gpointer key, value;
  guint32 max_event_interval = 0;

  /* Go over all input devices, calculate the offset from the newest event of each gesture */
  g_hash_table_iter_init (&iter, control->device_interpolators);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDeviceInterpolator *device_interpolator = value;

      calculate_device_interpolation_offset (device_interpolator,
                                             frame_counter,
                                             frame_time,
                                             current_time);

      /* Remember the largest event interval. This will be used later to limit the latency */
      max_event_interval = MAX (max_event_interval,
                                device_interpolator->event_interval_us);

      /* Adjust the target offset if it's too small. This single statement is the
       * core of this function. */
      if (control->global_time_offset_target < device_interpolator->time_offset_target)
        control->global_time_offset_target = device_interpolator->time_offset_target;
    }

  /* Apply the hard latency limit */
  if (control->global_time_offset_target > MAX_INTERPOLATION_OFFSET_MS)
    control->global_time_offset_target = MAX_INTERPOLATION_OFFSET_MS;

  if (largest_event_interval)
    *largest_event_interval = max_event_interval;
}

/*
 * In order to achieve smooth movement, the interpolation point has to lie
 * somewhere between the oldest and newest events in the event history. While
 * we could 'interpolate' events outside this boundary - in this case that
 * would be called 'extrapolation' - the accuracy would be reduced, since this
 * would effectively be predicing the future or guessing the past. Furthermore,
 * the interpolation point must move in lockstep to the presentation time.
 * That's because we want to calculate where the input device is located at the
 * time the frame is displayed, as opposed to when the event was generated.
 *
 * There is no point in trying to interpolate an event at the upcoming
 * presentation time, since the upcoming presentation time will always be later
 * then the latest event in the event history. In order to compensate for that,
 * we maintain a 'constant' offset from the presentation time. This offset is
 * the effective latency. The offset should be constant since, as explained
 * above, the interpolation point should move in lockstep with the presentation
 * time.
 *
 * The presentation time is usually not known in the first few display frames,
 * since we have to wait for the compositor to actually submit the frame in
 * order to get this information. If we'll wait till we can calculate the
 * presentation time we'll incur latency. So instead of using the presentation
 * time we use "frame time" as received from gdk_frame_clock_get_frame_time().
 * It is assumed that the frame time is at a constant offset from the actual
 * presentation time, otherwise no smooth animation would have been possible.
 *
 * It should be noted that some latency always exists. Even if no interpolation
 * takes place, the frame displayed will be affected only by events that were
 * received before the frame was sent to the screen. When interpolation is
 * enabled the latency is made explicit, and will be somewhat larger than the
 * usual latency due to the constraints mentioned above. In the current
 * implementation the value of control->global_time_offset is the effective
 * latency in milliseconds.
 *
 * The higher the offset the bigger the latency, so we would like the get the
 * minimal fixed offset that still guarantees we'll always get an adjusted
 * interpolation point within the event history. That optimum offset depends on
 * a variety of factors: input event frequency, input event jitter, display
 * frame duration etc. Instead of trying to use a complicated formula to
 * calculate the offset, we simply increase it dynamically until it no longer
 * changes.
 *
 * How do we know by how much to increase the offset, and when to stop doing
 * that? On every animation callback we adjust the target offset if it's not
 * large enough to make the interpolation point smaller than, or equal to, the
 * timestamp of the newest event. We update it to the minimal value that
 * satisfies this condition. Eventually the offset will no longer change.
 *
 * We maintain a single global latency offset since the interpolation point
 * must be the same for all gestures currently in progress. Otherwise the
 * interpolated events of different devices would have values corresponding to
 * different time stamps.
 *
 * The offset is reset whenever no gesture is in progress, so this mechanism
 * should work even when the conditions change - for example if a new gesture
 * originated from another input device, or if the window was moved to a
 * different monitor screen.
 */
static void
update_interpolation_offset (GdkEventInterpolationControl *control,
                             gint64                        frame_counter,
                             gint64                        frame_time,
                             gint64                        refresh_interval,
                             gint64                        current_time)
{
  guint32 max_event_interval = 0;

  /* Calculate the target interpolation latency offset, as well as the maximum
   * input event interval among all active devices */
  calculate_global_target_interpolation_offset (control,
                                                frame_counter,
                                                frame_time,
                                                current_time,
                                                &max_event_interval);

  /* In order to get smooth animation, control->global_time_offset should stay
   * relatively constant. This is one reason to "ease" updating it. */
  if (control->global_time_offset < control->global_time_offset_target)
    {
      if (control->previous_frame_time == 0)
        {
          /* First callback for this gesture, no need to "ease" updating the offset */
          control->global_time_offset = control->global_time_offset_target;
        }
      else
        {
          /*
           * It's possible that the frame interval is larger than the event interval - say
           * a 60Hz display coupled with a 500Hz mouse. The reverse is possible as well, e.g.
           * a 360Hz "gaming" monitor with a ~90Hz touchpad. We have to account for both cases.
           */
          gint64 frame_interval_ms = MAX (refresh_interval / 1000, 1);
          guint32 offset_soft_limit = MAX (INTERPOLATION_OFFSET_MAX_DISPLAY_FRAMES * frame_interval_ms,
                                           INTERPOLATION_OFFSET_MAX_EVENT_FRAMES * max_event_interval );

          /* Apply the soft latency limit */
          if (control->global_time_offset_target > offset_soft_limit)
            control->global_time_offset_target = offset_soft_limit;

          /* Gesture animation is in progress, "ease" updating the offset to prevent back-jumps.
           * This can happen if the offset is suddenly larger than a frame interval, which can
           * cause us to ask for a value in time earlier than the one of the last callback. So
           * allow the offset to change by at most half a frame interval each display frame. */
          gint32 delta_to_target = control->global_time_offset_target - control->global_time_offset;
          control->global_time_offset += signum(delta_to_target) * MIN (ABS (delta_to_target),
                                                                        (frame_interval_ms + 1) / 2);
        }
    }
}

static void
emit_interpolated_event (GdkEventHistory *animating_gesture,
                         GdkEvent        *interpolated_event,
                         GdkEvent        *start_event)
{
  /* Send a start event if one wasn't sent yet. We only send a start event once
     we receive either an 'update' or a 'stop' event. */
  if (start_event)
    {
      /* Set the start event time to be the same as the interpolated event */
      gdk_event_set_time (start_event, gdk_event_get_time (interpolated_event));

      /* Emit the start event */
      _gdk_event_emit (start_event);
      gdk_event_free (start_event);

      gdk_event_history_set_start_event (animating_gesture, NULL);
    }

  /* Emit the interpolated event */
  _gdk_event_emit (interpolated_event);
  gdk_event_free (interpolated_event);
}

/*
 * Once a stop event has been received, we keep emitting interpolated events
 * until the history buffer is drained. Only then we emit a stop event.
 */
static void
handle_stop_event (GdkDeviceInterpolator *device_interpolator,
                   GdkEvent              *start_event,
                   GdkEvent              *stop_event,
                   gint64                 adjusted_interpolation_point)
{
  /* We always emit events from the oldest gesture */
  GdkEventHistory * animating_gesture =
    g_ptr_array_index (device_interpolator->gesture_queue, 0);

  /* Emit a stop event once all events have been interpolated. An empty history
   * buffer is considered to have been interpolated. Note that here we use the
   * current adjusted interpolation point rather then then previous one, since
   * if we had uninterpolated events for this frame then they have already been
   * emitted by interpolate_device_events(). */
  gboolean send_stop =
    gdk_event_history_length (animating_gesture) ?
      gdk_event_history_all_existing_events_emitted (animating_gesture,
                                                     adjusted_interpolation_point) :
      TRUE;

  if (send_stop)
    {
      /* Set the stop event time to be 1ms after the previous event, just in case. */
      gdk_event_set_time (stop_event, (adjusted_interpolation_point / 1000) + 1);

      /* We are done with the current gesture */
      g_ptr_array_remove_index (device_interpolator->gesture_queue, 0);

      /* Emit the stop event */
      emit_interpolated_event (animating_gesture,
                               stop_event,
                               start_event);
    }
}

/*
 * We don't know beforehand when the next input event is going to arrive -
 * maybe we simply didn't receive the next event yet, and maybe the user
 * stopped moving his fingers on the trackpad, so no new event will arrive in
 * the near future. We also can't depend on 'stop' events, because not all
 * event types have them.
 *
 * We don't want the callback to continue firing indefinitely since this is a
 * waste of energy. So we use a heuristic to detect if we should stop the
 * interpolation callback when no input event was received lately.
 *
 * We assume that if at least INTERPOLATION_DISPLAY_GRACE display frames, or
 * INTERPOLATION_EVENT_GRACE input frames, whichever is higher time-wise, has
 * passed without receiving an input event, then it means that the gesture has
 * (possibly temporarily) stopped.
 *
 * In any case, as long as there are uninterpolated events in the history
 * buffer we don't stop the callback.
 */
static gboolean
device_time_out (GdkEventInterpolationControl *control,
                 GdkDeviceInterpolator        *device_interpolator,
                 gint64                        frame_time,
                 gint64                        current_time,
                 gint64                        refresh_interval)
{
  gint64 timestamp_offset_from_newest_event;
  gint64 event_interval;
  gint64 grace_period;

  /* No timeout if this is the first callback for this gesture */
  if (control->previous_frame_time == 0)
    {
      return FALSE;
    }

  /* If the event history is too short then event_interval will be set to 0. In
   * that case only the display frames will affect the grace period. */
  event_interval = device_interpolator->event_interval_us;

  /* The grace period takes into account the display interval as well as the
   * events interval */
  grace_period = MAX (refresh_interval * INTERPOLATION_DISPLAY_GRACE,
                      event_interval * INTERPOLATION_EVENT_GRACE);

  /* Calculate how much time passed since an input event was last received. */
  timestamp_offset_from_newest_event =
      current_time - device_interpolator->last_time_event_received;

  return (timestamp_offset_from_newest_event > grace_period);
}

static void
reset_device_interpolator (GdkDeviceInterpolator *device_interpolator)
{
  device_interpolator->last_frame_event_received = 0;
  device_interpolator->last_time_event_received = 0;
  device_interpolator->event_interval_us = 0;
  device_interpolator->time_offset_target = G_MININT32;
}

static GdkInterpolationDeviceStatus
interpolate_device_events (GdkEventInterpolationControl *control,
                           GdkDeviceInterpolator        *device_interpolator,
                           gint64                        adjusted_interpolation_point,
                           gint64                        frame_time,
                           gint64                        current_time,
                           gint64                        refresh_interval)
{
  gboolean timeout =
    device_time_out (control,
                     device_interpolator,
                     frame_time,
                     current_time,
                     refresh_interval);

  while (TRUE)
    {
      /* num_queued_gestures will be >= 1 since we remove devices with no active gestures */
      guint num_queued_gestures = device_interpolator->gesture_queue->len;

      /* We always emit events from the oldest gesture */
      GdkEventHistory * animating_gesture =
        g_ptr_array_index (device_interpolator->gesture_queue, 0);

      /* Get the start and stop events (any of them might be null if not received yet) */
      GdkEvent *start_event = gdk_event_history_get_start_event (animating_gesture);
      GdkEvent *stop_event = gdk_event_history_get_stop_event (animating_gesture);

      /* If there are some uninterpolated events then we still got work to do.
       * It's possible for the event history to be empty. That can happen, for
       * example, if the user didn't move his fingers for a while, triggering
       * the device_time_out() check above which resets the history, then later
       * lifted his fingers without moving them. */
      gboolean has_uninterpolated_events =
        (gdk_event_history_length (animating_gesture) > 0 &&
         !gdk_event_history_all_existing_events_emitted (animating_gesture,
                                                         control->previous_interpolation_point));

      /* Handle time outs */
      if (num_queued_gestures == 1 && timeout && !stop_event && !has_uninterpolated_events)
        {
          /* Reset the history in order to prevent 'jumps' when we start
             receiving events again. */
          gdk_event_history_reset (animating_gesture);
          reset_device_interpolator (device_interpolator);
          return GDK_INTERP_STATUS_TIMEOUT;
        }

      /* Handle "normal" (position update) events */
      if (has_uninterpolated_events)
        {
          /* Synthesize an interpolated event */
          GdkEvent *interpolated_event =
            gdk_event_history_interpolate_event (animating_gesture,
                                                 adjusted_interpolation_point);

          /* We might not get an event, for example when the adjusted interpolation
           * point is earlier than the timestamp of the oldest event. */
          if (interpolated_event)
            emit_interpolated_event (animating_gesture,
                                     interpolated_event,
                                     start_event);
        }

      /* Handle stop events */
      if (stop_event)
        {
          /* handle_stop_event() will remove the current gesture if the gesture
           * ended and all of its events were interpolated */
          handle_stop_event (device_interpolator,
                             start_event,
                             stop_event,
                             adjusted_interpolation_point);

          /* If no gestures remain for the device, remove it */
          if (!device_interpolator->gesture_queue->len)
            {
              g_ptr_array_unref (device_interpolator->gesture_queue);
              g_slice_free (GdkDeviceInterpolator, device_interpolator);

              return GDK_INTERP_STATUS_DONE;
            }
        }

      /* If no gesture has been deleted then we have no more gestures to
       * interpolate in this display frame */
      if (num_queued_gestures == device_interpolator->gesture_queue->len)
        break;

      num_queued_gestures = device_interpolator->gesture_queue->len;
    }

  return GDK_INTERP_STATUS_ONGOING;
}

/*
 * interpolation_tick_callback() is responsible for emitting the interpolated
 * events, one event per display frame.
 *
 * There are several reasons to use an animation callback as opposed to, say,
 * directly replacing received events with interpolated ones.
 *
 * One reason is that when the screen refresh rate is higher than the input
 * event rate, there would be display frames in which no input event is
 * received. By using the animation callback we ensure that every display frame
 * receives an input event.
 *
 * Another reason is to prevent 'jumps' when receiving a stop event. Without
 * the animation callback, the history would have to be flushed when receiving
 * a stop event. With the animation we can drain the event history at the frame
 * rate, and only emit the stop event after all of the events in the history
 * were interpolated.
 */
static void
interpolation_tick_callback (GdkFrameClock                *frame_clock,
                             GdkEventInterpolationControl *control)
{
  gint64 frame_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  gint64 frame_time = gdk_frame_clock_get_frame_time (frame_clock);
  gint64 refresh_interval;
  gint64 current_time = g_get_monotonic_time ();
  gint64 adjusted_interpolation_point;
  GHashTableIter iter;
  gpointer key, value;
  gboolean stop_callback;

  gdk_frame_clock_get_refresh_info (frame_clock,
                                    control->previous_frame_time,
                                    &refresh_interval, NULL);

  /* Calculate the interpolation point in time, adjusted for latency */
  update_interpolation_offset (control, frame_counter, frame_time, refresh_interval, current_time);
  adjusted_interpolation_point = frame_time - (control->global_time_offset * 1000);

  /* By default stop the callback if no input event was received lately and no
   * uninterpolated events remained. */
  stop_callback = TRUE;

  /* Go over all input devices, emit interpolated events when applicable */
  g_hash_table_iter_init (&iter, control->device_interpolators);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDeviceInterpolator *device_interpolator = value;

      GdkInterpolationDeviceStatus device_status =
        interpolate_device_events (control,
                                   device_interpolator,
                                   adjusted_interpolation_point,
                                   frame_time,
                                   current_time,
                                   refresh_interval);

      switch (device_status)
        {
        case GDK_INTERP_STATUS_DONE:
          /* Remove the device from the hash table if all of its gestures completed */
          g_hash_table_iter_remove (&iter);
          break;

        case GDK_INTERP_STATUS_ONGOING:
          /* Don't stop the callback if at least one device still got work to */
          stop_callback = FALSE;
          break;

        case GDK_INTERP_STATUS_TIMEOUT:
          /* Do nothing. If all devices return either timeout or done we'll stop the callback */
          break;
        }
    }

  /* Stop the callback if there are no active gestures */
  if (stop_callback)
    stop_interpolation_callback (control);

  control->previous_frame_time = frame_time;
  control->previous_callback_time = current_time;
  control->previous_interpolation_point = adjusted_interpolation_point;
}

static void
start_interpolation_callback (GdkEventInterpolationControl *control)
{
  GdkFrameClock *frame_clock = gdk_window_get_frame_clock (control->window);

  if (frame_clock && !control->interpolation_tick_id)
    {
      /* A gesture was continued after not moving the fingers for a while, or a
       * new gesture was started after all previous gestures finished being
       * interpolated. Either way we can reset the latency counters. */
      control->global_time_offset = G_MININT32;
      control->global_time_offset_target = G_MININT32;
      control->previous_frame_time = 0;
      control->previous_callback_time = 0;
      control->previous_interpolation_point = 0;

      /* start the interpolation animation callback */
      control->interpolation_tick_id = g_signal_connect (frame_clock, "before-paint",
                                                         G_CALLBACK (interpolation_tick_callback),
                                                         control);
      gdk_frame_clock_begin_updating (frame_clock);
    }
}

static void
stop_interpolation_callback (GdkEventInterpolationControl *control)
{
  GdkFrameClock *frame_clock = gdk_window_get_frame_clock (control->window);

  /* Stop animation */
  if (frame_clock && control->interpolation_tick_id)
    {
      gdk_frame_clock_end_updating (frame_clock);
      g_signal_handler_disconnect (frame_clock, control->interpolation_tick_id);
      control->interpolation_tick_id = 0;
    }
}

static gboolean
is_gesture_start (GdkEvent *event)
{
  return (((gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE) &&
            (event->touchpad_swipe.phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)) ||
          ((gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH) &&
            (event->touchpad_pinch.phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)));
}

static gboolean
is_gesture_end (GdkEvent *event)
{
  return (((gdk_event_get_event_type (event) == GDK_SCROLL) &&
           gdk_event_is_scroll_stop_event (event)) ||
          ((gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE) &&
           ((event->touchpad_swipe.phase == GDK_TOUCHPAD_GESTURE_PHASE_END) ||
            (event->touchpad_swipe.phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL))) ||
          ((gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH) &&
           ((event->touchpad_pinch.phase == GDK_TOUCHPAD_GESTURE_PHASE_END) ||
            (event->touchpad_pinch.phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL))));
}

static GdkDeviceInterpolator *
get_or_create_device_interpolator (GdkEventInterpolationControl *control,
                                   GdkDevice                    *device)
{
  GdkDeviceInterpolator *device_interpolator;

  device_interpolator = g_hash_table_lookup (control->device_interpolators, device);

  if (G_UNLIKELY (!device_interpolator))
    {
      device_interpolator = g_slice_new (GdkDeviceInterpolator);

      reset_device_interpolator (device_interpolator);
      device_interpolator->gesture_queue =
        g_ptr_array_new_with_free_func ((GDestroyNotify) gdk_event_history_free);

      g_hash_table_insert (control->device_interpolators,
                           device, device_interpolator);
    }

  return device_interpolator;
}

static GdkEventHistory *
device_accumulating_gesture (GdkDeviceInterpolator *device_interpolator)
{
  GdkEventHistory *accumulating_gesture;
  gboolean new_gesture_started = FALSE;

  /* Check if some gesture is already in progress */
  if (device_interpolator->gesture_queue->len == 0)
    {
      /* No gesture was in progress */
      new_gesture_started = TRUE;
    }
  else
    {
      /* A gesture is already in progress. We always push events to the
       * last gesture in the queue */
      accumulating_gesture =
        g_ptr_array_index (device_interpolator->gesture_queue,
                           device_interpolator->gesture_queue->len - 1);

      if (gdk_event_history_get_stop_event (accumulating_gesture))
        {
          /* Previous gesture received a stop event, start a new gesture */
          new_gesture_started = TRUE;
        }
    }

  if (new_gesture_started)
    {
      accumulating_gesture = gdk_event_history_new ();
      g_ptr_array_add (device_interpolator->gesture_queue,
                       accumulating_gesture);
    }

  return accumulating_gesture;
}

static void
update_device_stats (GdkEventInterpolationControl *control,
                     GdkDeviceInterpolator        *device_interpolator,
                     GdkEventHistory              *accumulating_gesture)
{
  GdkFrameClock *frame_clock;

  /* Keep the wall-time of the newest received event. Use wall time since
   * an event could stall for a while before reaching here */
  frame_clock = gdk_window_get_frame_clock (control->window);

  device_interpolator->last_frame_event_received = gdk_frame_clock_get_frame_counter (frame_clock);
  device_interpolator->last_time_event_received = g_get_monotonic_time ();

  if (device_interpolator->event_interval_us == 0 &&
      gdk_event_history_length (accumulating_gesture) == 2)
    {
      /*
       * Once we have 2 "update" events we can estimate the input frame
       * interval. We add 1ms to account for rounding - e.g. if the actual
       * interval is 10.5ms we might get 10ms as the average duration. This is
       * also necessary for handling input devices which emit about 1000
       * events/second such as some gaming mice. In this case the event
       * duration might be 0 because the timestamp accuracy is 1ms, so adding 1
       * ensures a non-0 event interval.
       */
      device_interpolator->event_interval_us = 1000 *
        (gdk_event_history_average_event_interval (accumulating_gesture) + 1);
    }
}

static void
add_event_to_history (GdkEventInterpolationControl *control,
                      GdkDeviceInterpolator        *device_interpolator,
                      GdkEvent                     *event)
{
  GdkEventHistory *accumulating_gesture;

  /* Get or create the accumulating gesture of the device */
  accumulating_gesture = device_accumulating_gesture (device_interpolator);

  /* Insert the new event to the history */
  if (is_gesture_start (event))
    {
      gdk_event_history_set_start_event (accumulating_gesture,
                                         event);
    }
  else if (is_gesture_end (event))
    {
      gdk_event_history_set_stop_event (accumulating_gesture,
                                        event);
    }
  else /* Gesture update */
    {
      gdk_event_history_push (accumulating_gesture,
                              event);

      update_device_stats (control,
                           device_interpolator,
                           accumulating_gesture);
    }
}

/*
 * gdk_event_interpolation_control_add:
 * @control: a #GdkEventInterpolationControl
 * @event: a #GdkEvent
 *
 * Adds the event to the event history of the corresponding device. Starts the
 * interpolation animation callback if necessary.
 *
 * Returns: The number of events to remove from the original event queue.
 */
int
gdk_event_interpolation_control_add (GdkEventInterpolationControl *control,
                                     GdkEvent                     *event)
{
  GdkDeviceInterpolator *device_interpolator;
  GdkDevice *device;

  /* Start the animation if it is not already in progress */
  start_interpolation_callback (control);

  /* Look for the device-specific interpolation data */
  device = gdk_event_get_source_device (event);
  device_interpolator = get_or_create_device_interpolator (control, device);

  /* Add the event to the device-specific event history */
  add_event_to_history (control,
                        device_interpolator,
                        event);

  /* All interpolated events will be emitted from the callback */
  return 2;
}
