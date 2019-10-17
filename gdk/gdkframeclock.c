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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkframeclockprivate.h"
#include "gdkinternals.h"

/**
 * SECTION:gdkframeclock
 * @Short_description: Frame clock syncs painting to a window or display
 * @Title: Frame clock
 *
 * A #GdkFrameClock tells the application when to update and repaint a
 * window. This may be synced to the vertical refresh rate of the
 * monitor, for example. Even when the frame clock uses a simple timer
 * rather than a hardware-based vertical sync, the frame clock helps
 * because it ensures everything paints at the same time (reducing the
 * total number of frames). The frame clock can also automatically
 * stop painting when it knows the frames will not be visible, or
 * scale back animation framerates.
 *
 * #GdkFrameClock is designed to be compatible with an OpenGL-based
 * implementation or with mozRequestAnimationFrame in Firefox,
 * for example.
 *
 * A frame clock is idle until someone requests a frame with
 * gdk_frame_clock_request_phase(). At some later point that makes
 * sense for the synchronization being implemented, the clock will
 * process a frame and emit signals for each phase that has been
 * requested. (See the signals of the #GdkFrameClock class for
 * documentation of the phases. %GDK_FRAME_CLOCK_PHASE_UPDATE and the
 * #GdkFrameClock::update signal are most interesting for application
 * writers, and are used to update the animations, using the frame time
 * given by gdk_frame_clock_get_frame_time().
 *
 * The frame time is reported in microseconds and generally in the same
 * timescale as g_get_monotonic_time(), however, it is not the same
 * as g_get_monotonic_time(). The frame time does not advance during
 * the time a frame is being painted, and outside of a frame, an attempt
 * is made so that all calls to gdk_frame_clock_get_frame_time() that
 * are called at a “similar” time get the same value. This means that
 * if different animations are timed by looking at the difference in
 * time between an initial value from gdk_frame_clock_get_frame_time()
 * and the value inside the #GdkFrameClock::update signal of the clock,
 * they will stay exactly synchronized.
 */

/**
 * GdkFrameClock:
 *
 * The GdkFrameClock struct contains only private fields and
 * should not be accessed directly.
 */

enum {
  FLUSH_EVENTS,
  BEFORE_PAINT,
  UPDATE,
  LAYOUT,
  PAINT,
  AFTER_PAINT,
  RESUME_EVENTS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static guint fps_counter;

#define FRAME_HISTORY_MAX_LENGTH 16

struct _GdkFrameClockPrivate
{
  gint64 frame_counter;
  gint n_timings;
  gint current;
  GdkFrameTimings *timings[FRAME_HISTORY_MAX_LENGTH];
  gint n_freeze_inhibitors;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GdkFrameClock, gdk_frame_clock, G_TYPE_OBJECT)

static void
_gdk_frame_clock_freeze (GdkFrameClock *clock);

static void
gdk_frame_clock_finalize (GObject *object)
{
  GdkFrameClockPrivate *priv = GDK_FRAME_CLOCK (object)->priv;
  int i;

  for (i = 0; i < FRAME_HISTORY_MAX_LENGTH; i++)
    if (priv->timings[i] != 0)
      gdk_frame_timings_unref (priv->timings[i]);

  G_OBJECT_CLASS (gdk_frame_clock_parent_class)->finalize (object);
}

static void
gdk_frame_clock_constructed (GObject *object)
{
  G_OBJECT_CLASS (gdk_frame_clock_parent_class)->constructed (object);

  _gdk_frame_clock_freeze (GDK_FRAME_CLOCK (object));
}

static void
gdk_frame_clock_class_init (GdkFrameClockClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;

  gobject_class->finalize     = gdk_frame_clock_finalize;
  gobject_class->constructed  = gdk_frame_clock_constructed;

  /**
   * GdkFrameClock::flush-events:
   * @clock: the frame clock emitting the signal
   *
   * This signal is used to flush pending motion events that
   * are being batched up and compressed together. Applications
   * should not handle this signal.
   */
  signals[FLUSH_EVENTS] =
    g_signal_new (g_intern_static_string ("flush-events"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::before-paint:
   * @clock: the frame clock emitting the signal
   *
   * This signal begins processing of the frame. Applications
   * should generally not handle this signal.
   */
  signals[BEFORE_PAINT] =
    g_signal_new (g_intern_static_string ("before-paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::update:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted as the first step of toolkit and
   * application processing of the frame. Animations should
   * be updated using gdk_frame_clock_get_frame_time().
   * Applications can connect directly to this signal, or
   * use gtk_widget_add_tick_callback() as a more convenient
   * interface.
   */
  signals[UPDATE] =
    g_signal_new (g_intern_static_string ("update"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::layout:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted as the second step of toolkit and
   * application processing of the frame. Any work to update
   * sizes and positions of application elements should be
   * performed. GTK normally handles this internally.
   */
  signals[LAYOUT] =
    g_signal_new (g_intern_static_string ("layout"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::paint:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted as the third step of toolkit and
   * application processing of the frame. The frame is
   * repainted. GDK normally handles this internally and
   * produces expose events, which are turned into GTK
   * #GtkWidget::draw signals.
   */
  signals[PAINT] =
    g_signal_new (g_intern_static_string ("paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::after-paint:
   * @clock: the frame clock emitting the signal
   *
   * This signal ends processing of the frame. Applications
   * should generally not handle this signal.
   */
  signals[AFTER_PAINT] =
    g_signal_new (g_intern_static_string ("after-paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::resume-events:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted after processing of the frame is
   * finished, and is handled internally by GTK to resume normal
   * event processing. Applications should not handle this signal.
   */
  signals[RESUME_EVENTS] =
    g_signal_new (g_intern_static_string ("resume-events"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gdk_frame_clock_init (GdkFrameClock *clock)
{
  GdkFrameClockPrivate *priv;

  clock->priv = priv = gdk_frame_clock_get_instance_private (clock);

  priv->frame_counter = -1;
  priv->current = FRAME_HISTORY_MAX_LENGTH - 1;

#ifdef G_ENABLE_DEBUG
  if (fps_counter == 0)
    fps_counter = gdk_profiler_define_counter ("fps", "Frames per Second");
#endif
}

/**
 * gdk_frame_clock_get_frame_time:
 * @frame_clock: a #GdkFrameClock
 *
 * Gets the time that should currently be used for animations.  Inside
 * the processing of a frame, it’s the time used to compute the
 * animation position of everything in a frame. Outside of a frame, it's
 * the time of the conceptual “previous frame,” which may be either
 * the actual previous frame time, or if that’s too old, an updated
 * time.
 *
 * Returns: a timestamp in microseconds, in the timescale of
 *  of g_get_monotonic_time().
 */
gint64
gdk_frame_clock_get_frame_time (GdkFrameClock *frame_clock)
{
  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (frame_clock), 0);

  return GDK_FRAME_CLOCK_GET_CLASS (frame_clock)->get_frame_time (frame_clock);
}

/**
 * gdk_frame_clock_request_phase:
 * @frame_clock: a #GdkFrameClock
 * @phase: the phase that is requested
 *
 * Asks the frame clock to run a particular phase. The signal
 * corresponding the requested phase will be emitted the next
 * time the frame clock processes. Multiple calls to
 * gdk_frame_clock_request_phase() will be combined together
 * and only one frame processed. If you are displaying animated
 * content and want to continually request the
 * %GDK_FRAME_CLOCK_PHASE_UPDATE phase for a period of time,
 * you should use gdk_frame_clock_begin_updating() instead, since
 * this allows GTK to adjust system parameters to get maximally
 * smooth animations.
 */
void
gdk_frame_clock_request_phase (GdkFrameClock      *frame_clock,
                               GdkFrameClockPhase  phase)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (frame_clock));

  GDK_FRAME_CLOCK_GET_CLASS (frame_clock)->request_phase (frame_clock, phase);
}

/**
 * gdk_frame_clock_begin_updating:
 * @frame_clock: a #GdkFrameClock
 *
 * Starts updates for an animation. Until a matching call to
 * gdk_frame_clock_end_updating() is made, the frame clock will continually
 * request a new frame with the %GDK_FRAME_CLOCK_PHASE_UPDATE phase.
 * This function may be called multiple times and frames will be
 * requested until gdk_frame_clock_end_updating() is called the same
 * number of times.
 */
void
gdk_frame_clock_begin_updating (GdkFrameClock *frame_clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (frame_clock));

  GDK_FRAME_CLOCK_GET_CLASS (frame_clock)->begin_updating (frame_clock);
}

/**
 * gdk_frame_clock_end_updating:
 * @frame_clock: a #GdkFrameClock
 *
 * Stops updates for an animation. See the documentation for
 * gdk_frame_clock_begin_updating().
 */
void
gdk_frame_clock_end_updating (GdkFrameClock *frame_clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (frame_clock));

  GDK_FRAME_CLOCK_GET_CLASS (frame_clock)->end_updating (frame_clock);
}

static void
_gdk_frame_clock_freeze (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_CLASS (clock)->freeze (clock);
}


static void
_gdk_frame_clock_thaw (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_CLASS (clock)->thaw (clock);
}

void
_gdk_frame_clock_inhibit_freeze (GdkFrameClock *clock)
{
  GdkFrameClockPrivate *priv;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  priv = clock->priv;

  priv->n_freeze_inhibitors++;
  if (priv->n_freeze_inhibitors == 1)
    _gdk_frame_clock_thaw (clock);
}


void
_gdk_frame_clock_uninhibit_freeze (GdkFrameClock *clock)
{
  GdkFrameClockPrivate *priv;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  priv = clock->priv;

  priv->n_freeze_inhibitors--;
  if (priv->n_freeze_inhibitors == 0)
    _gdk_frame_clock_freeze (clock);
}

/**
 * gdk_frame_clock_get_frame_counter:
 * @frame_clock: a #GdkFrameClock
 *
 * A #GdkFrameClock maintains a 64-bit counter that increments for
 * each frame drawn.
 *
 * Returns: inside frame processing, the value of the frame counter
 *  for the current frame. Outside of frame processing, the frame
 *   counter for the last frame.
 */
gint64
gdk_frame_clock_get_frame_counter (GdkFrameClock *frame_clock)
{
  GdkFrameClockPrivate *priv;

  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (frame_clock), 0);

  priv = frame_clock->priv;

  return priv->frame_counter;
}

/**
 * gdk_frame_clock_get_history_start:
 * @frame_clock: a #GdkFrameClock
 *
 * #GdkFrameClock internally keeps a history of #GdkFrameTimings
 * objects for recent frames that can be retrieved with
 * gdk_frame_clock_get_timings(). The set of stored frames
 * is the set from the counter values given by
 * gdk_frame_clock_get_history_start() and
 * gdk_frame_clock_get_frame_counter(), inclusive.
 *
 * Returns: the frame counter value for the oldest frame
 *  that is available in the internal frame history of the
 *  #GdkFrameClock.
 */
gint64
gdk_frame_clock_get_history_start (GdkFrameClock *frame_clock)
{
  GdkFrameClockPrivate *priv;

  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (frame_clock), 0);

  priv = frame_clock->priv;

  return priv->frame_counter + 1 - priv->n_timings;
}

void
_gdk_frame_clock_begin_frame (GdkFrameClock *frame_clock)
{
  GdkFrameClockPrivate *priv;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (frame_clock));

  priv = frame_clock->priv;

  priv->frame_counter++;
  priv->current = (priv->current + 1) % FRAME_HISTORY_MAX_LENGTH;

  /* Try to steal the previous frame timing instead of discarding
   * and allocating a new one.
   */
  if G_LIKELY (priv->n_timings == FRAME_HISTORY_MAX_LENGTH &&
               _gdk_frame_timings_steal (priv->timings[priv->current],
                                         priv->frame_counter))
    return;

  if (priv->n_timings < FRAME_HISTORY_MAX_LENGTH)
    priv->n_timings++;
  else
    gdk_frame_timings_unref (priv->timings[priv->current]);

  priv->timings[priv->current] = _gdk_frame_timings_new (priv->frame_counter);
}

/**
 * gdk_frame_clock_get_timings:
 * @frame_clock: a #GdkFrameClock
 * @frame_counter: the frame counter value identifying the frame to
 *  be received.
 *
 * Retrieves a #GdkFrameTimings object holding timing information
 * for the current frame or a recent frame. The #GdkFrameTimings
 * object may not yet be complete: see gdk_frame_timings_get_complete().
 *
 * Returns: (nullable) (transfer none): the #GdkFrameTimings object for
 *  the specified frame, or %NULL if it is not available. See
 *  gdk_frame_clock_get_history_start().
 */
GdkFrameTimings *
gdk_frame_clock_get_timings (GdkFrameClock *frame_clock,
                             gint64         frame_counter)
{
  GdkFrameClockPrivate *priv;
  gint pos;

  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (frame_clock), NULL);

  priv = frame_clock->priv;

  if (frame_counter > priv->frame_counter)
    return NULL;

  if (frame_counter <= priv->frame_counter - priv->n_timings)
    return NULL;

  pos = (priv->current - (priv->frame_counter - frame_counter) + FRAME_HISTORY_MAX_LENGTH) % FRAME_HISTORY_MAX_LENGTH;

  return priv->timings[pos];
}

/**
 * gdk_frame_clock_get_current_timings:
 * @frame_clock: a #GdkFrameClock
 *
 * Gets the frame timings for the current frame.
 *
 * Returns: (nullable) (transfer none): the #GdkFrameTimings for the
 *  frame currently being processed, or even no frame is being
 *  processed, for the previous frame. Before any frames have been
 *  processed, returns %NULL.
 */
GdkFrameTimings *
gdk_frame_clock_get_current_timings (GdkFrameClock *frame_clock)
{
  GdkFrameClockPrivate *priv;

  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (frame_clock), 0);

  priv = frame_clock->priv;

  return gdk_frame_clock_get_timings (frame_clock, priv->frame_counter);
}


#ifdef G_ENABLE_DEBUG
void
_gdk_frame_clock_debug_print_timings (GdkFrameClock   *clock,
                                      GdkFrameTimings *timings)
{
  GString *str;

  gint64 previous_frame_time = 0;
  GdkFrameTimings *previous_timings = gdk_frame_clock_get_timings (clock,
                                                                   timings->frame_counter - 1);

  if (previous_timings != NULL)
    previous_frame_time = previous_timings->frame_time;

  str = g_string_new ("");

  g_string_append_printf (str, "%5" G_GINT64_FORMAT ":", timings->frame_counter);
  if (previous_frame_time != 0)
    {
      g_string_append_printf (str, " interval=%-4.1f", (timings->frame_time - previous_frame_time) / 1000.);
      g_string_append_printf (str, timings->slept_before ?  " (sleep)" : "        ");
    }
  if (timings->layout_start_time != 0)
    g_string_append_printf (str, " layout_start=%-4.1f", (timings->layout_start_time - timings->frame_time) / 1000.);
  if (timings->paint_start_time != 0)
    g_string_append_printf (str, " paint_start=%-4.1f", (timings->paint_start_time - timings->frame_time) / 1000.);
  if (timings->frame_end_time != 0)
    g_string_append_printf (str, " frame_end=%-4.1f", (timings->frame_end_time - timings->frame_time) / 1000.);
  if (timings->presentation_time != 0)
    g_string_append_printf (str, " present=%-4.1f", (timings->presentation_time - timings->frame_time) / 1000.);
  if (timings->predicted_presentation_time != 0)
    g_string_append_printf (str, " predicted=%-4.1f", (timings->predicted_presentation_time - timings->frame_time) / 1000.);
  if (timings->refresh_interval != 0)
    g_string_append_printf (str, " refresh_interval=%-4.1f", timings->refresh_interval / 1000.);

  g_message ("%s", str->str);
  g_string_free (str, TRUE);
}
#endif /* G_ENABLE_DEBUG */

#define DEFAULT_REFRESH_INTERVAL 16667 /* 16.7ms (1/60th second) */
#define MAX_HISTORY_AGE 150000         /* 150ms */

/**
 * gdk_frame_clock_get_refresh_info:
 * @frame_clock: a #GdkFrameClock
 * @base_time: base time for determining a presentaton time
 * @refresh_interval_return: (out) (optional): a location to store the
 * determined refresh interval, or %NULL. A default refresh interval of
 * 1/60th of a second will be stored if no history is present.
 * @presentation_time_return: (out): a location to store the next
 *  candidate presentation time after the given base time.
 *  0 will be will be stored if no history is present.
 *
 * Using the frame history stored in the frame clock, finds the last
 * known presentation time and refresh interval, and assuming that
 * presentation times are separated by the refresh interval,
 * predicts a presentation time that is a multiple of the refresh
 * interval after the last presentation time, and later than @base_time.
 */
void
gdk_frame_clock_get_refresh_info (GdkFrameClock *frame_clock,
                                  gint64         base_time,
                                  gint64        *refresh_interval_return,
                                  gint64        *presentation_time_return)
{
  gint64 frame_counter;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (frame_clock));

  frame_counter = gdk_frame_clock_get_frame_counter (frame_clock);

  if (presentation_time_return)
    *presentation_time_return = 0;
  if (refresh_interval_return)
    *refresh_interval_return = DEFAULT_REFRESH_INTERVAL;

  while (TRUE)
    {
      GdkFrameTimings *timings = gdk_frame_clock_get_timings (frame_clock, frame_counter);
      gint64 presentation_time;
      gint64 refresh_interval;

      if (timings == NULL)
        return;

      refresh_interval = timings->refresh_interval;
      presentation_time = timings->presentation_time;

      if (presentation_time != 0)
        {
          if (presentation_time > base_time - MAX_HISTORY_AGE &&
              presentation_time_return)
            {
              if (refresh_interval == 0)
                refresh_interval = DEFAULT_REFRESH_INTERVAL;

              if (refresh_interval_return)
                *refresh_interval_return = refresh_interval;

              while (presentation_time < base_time)
                presentation_time += refresh_interval;

              if (presentation_time_return)
                *presentation_time_return = presentation_time;
            }

          return;
        }

      frame_counter--;
    }
}

void
_gdk_frame_clock_emit_flush_events (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[FLUSH_EVENTS], 0);
}

void
_gdk_frame_clock_emit_before_paint (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[BEFORE_PAINT], 0);
}

void
_gdk_frame_clock_emit_update (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[UPDATE], 0);
}

void
_gdk_frame_clock_emit_layout (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[LAYOUT], 0);
}

void
_gdk_frame_clock_emit_paint (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[PAINT], 0);
}

void
_gdk_frame_clock_emit_after_paint (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[AFTER_PAINT], 0);
}

void
_gdk_frame_clock_emit_resume_events (GdkFrameClock *frame_clock)
{
  g_signal_emit (frame_clock, signals[RESUME_EVENTS], 0);
}

#ifdef G_ENABLE_DEBUG
static gint64
guess_refresh_interval (GdkFrameClock *frame_clock)
{
  gint64 interval;
  gint64 i;

  interval = G_MAXINT64;

  for (i = gdk_frame_clock_get_history_start (frame_clock);
       i < gdk_frame_clock_get_frame_counter (frame_clock);
       i++)
    {
      GdkFrameTimings *t, *before;
      gint64 ts, before_ts;

      t = gdk_frame_clock_get_timings (frame_clock, i);
      before = gdk_frame_clock_get_timings (frame_clock, i - 1);
      if (t == NULL || before == NULL)
        continue;

      ts = gdk_frame_timings_get_frame_time (t);
      before_ts = gdk_frame_timings_get_frame_time (before);
      if (ts == 0 || before_ts == 0)
        continue;

      interval = MIN (interval, ts - before_ts);
    }

  if (interval == G_MAXINT64)
    return 0;

  return interval;
}

static double
frame_clock_get_fps (GdkFrameClock *frame_clock)
{
  GdkFrameTimings *start, *end;
  gint64 start_counter, end_counter;
  gint64 start_timestamp, end_timestamp;
  gint64 interval;

  start_counter = gdk_frame_clock_get_history_start (frame_clock);
  end_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  start = gdk_frame_clock_get_timings (frame_clock, start_counter);
  for (end = gdk_frame_clock_get_timings (frame_clock, end_counter);
       end_counter > start_counter && end != NULL && !gdk_frame_timings_get_complete (end);
       end = gdk_frame_clock_get_timings (frame_clock, end_counter))
    end_counter--;
  if (end_counter - start_counter < 4)
    return 0.0;

  start_timestamp = gdk_frame_timings_get_presentation_time (start);
  end_timestamp = gdk_frame_timings_get_presentation_time (end);
  if (start_timestamp == 0 || end_timestamp == 0)
    {
      start_timestamp = gdk_frame_timings_get_frame_time (start);
      end_timestamp = gdk_frame_timings_get_frame_time (end);
    }
  interval = gdk_frame_timings_get_refresh_interval (end);
  if (interval == 0)
    {
      interval = guess_refresh_interval (frame_clock);
      if (interval == 0)
        return 0.0;
    }   
    
  return ((double) end_counter - start_counter) * G_USEC_PER_SEC / (end_timestamp - start_timestamp);
}
#endif

void
_gdk_frame_clock_add_timings_to_profiler (GdkFrameClock   *clock,
                                          GdkFrameTimings *timings)
{
#ifdef G_ENABLE_DEBUG
  gdk_profiler_add_mark (timings->frame_time * 1000,
                         (timings->frame_end_time - timings->frame_time) * 1000,
                         "frame", "");

  if (timings->layout_start_time != 0)
    gdk_profiler_add_mark (timings->layout_start_time * 1000,
                           (timings->paint_start_time - timings->layout_start_time) * 1000,
                            "layout", "");

  if (timings->paint_start_time != 0)
    gdk_profiler_add_mark (timings->paint_start_time * 1000,
                           (timings->frame_end_time - timings->paint_start_time) * 1000,
                            "paint", "");

  if (timings->presentation_time != 0)
    gdk_profiler_add_mark (timings->presentation_time * 1000,
                           0,
                           "presentation", "");

  gdk_profiler_set_counter (fps_counter,
                            timings->frame_end_time * 1000,
                            frame_clock_get_fps (clock)); 
#endif
}
