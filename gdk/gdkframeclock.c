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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2010.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkframeclock.h"

G_DEFINE_INTERFACE (GdkFrameClockTarget, gdk_frame_clock_target, G_TYPE_OBJECT)

static void
gdk_frame_clock_target_default_init (GdkFrameClockTargetInterface *iface)
{
}

void gdk_frame_clock_target_set_clock (GdkFrameClockTarget *target,
                                       GdkFrameClock       *clock)
{
  GDK_FRAME_CLOCK_TARGET_GET_IFACE (target)->set_clock (target, clock);
}

/**
 * SECTION:frameclock
 * @Short_description: Frame clock syncs painting to a window or display
 * @Title: Frame clock
 *
 * A #GdkFrameClock tells the application when to repaint a window.
 * This may be synced to the vertical refresh rate of the monitor, for
 * example. Even when the frame clock uses a simple timer rather than
 * a hardware-based vertical sync, the frame clock helps because it
 * ensures everything paints at the same time (reducing the total
 * number of frames). The frame clock can also automatically stop
 * painting when it knows the frames will not be visible, or scale back
 * animation framerates.
 *
 * #GdkFrameClock is designed to be compatible with an OpenGL-based
 * implementation or with mozRequestAnimationFrame in Firefox,
 * for example.
 *
 * A frame clock is idle until someone requests a frame with
 * gdk_frame_clock_request_phase(). At that time, the frame clock
 * emits its GdkFrameClock:frame-requested signal if no frame was
 * already pending.
 *
 * At some later time after the frame is requested, the frame clock
 * MAY indicate that a frame should be painted. To paint a frame the
 * clock will: Emit GdkFrameClock:before-paint; update the frame time
 * in the default handler for GdkFrameClock:before-paint; emit
 * GdkFrameClock:paint; emit GdkFrameClock:after-paint.  The app
 * should paint in a handler for the paint signal.
 *
 * If a given frame is not painted (the clock is idle), the frame time
 * should still update to a conceptual "last frame." i.e. the frame
 * time will keep moving forward roughly with wall clock time.
 *
 * The frame time is in milliseconds. However, it should not be
 * thought of as having any particular relationship to wall clock
 * time. Unlike wall clock time, it "snaps" to conceptual frame times
 * so is low-resolution; it is guaranteed to never move backward (so
 * say you reset your computer clock, the frame clock will not reset);
 * and the frame clock is allowed to drift. For example nicer
 * results when painting with vertical refresh sync may be obtained by
 * painting as rapidly as possible, but always incrementing the frame
 * time by the frame length on each frame. This results in a frame
 * time that doesn't have a lot to do with wall clock time.
 */

G_DEFINE_INTERFACE (GdkFrameClock, gdk_frame_clock, G_TYPE_OBJECT)

enum {
  FRAME_REQUESTED,
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

static void
gdk_frame_clock_default_init (GdkFrameClockInterface *iface)
{
  /**
   * GdkFrameClock::frame-requested:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted when a frame is not pending, and
   * gdk_frame_clock_request_frame() is called to request a frame.
   */
  signals[FRAME_REQUESTED] =
    g_signal_new (g_intern_static_string ("frame-requested"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::flush-events:
   * @clock: the frame clock emitting the signal
   *
   * FIXME.
   */
  signals[FLUSH_EVENTS] =
    g_signal_new (g_intern_static_string ("flush-events"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::before-paint:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted immediately before the paint signal and
   * indicates that the frame time has been updated, and signal
   * handlers should perform any preparatory work before painting.
   */
  signals[BEFORE_PAINT] =
    g_signal_new (g_intern_static_string ("before-paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::update:
   * @clock: the frame clock emitting the signal
   *
   * FIXME.
   */
  signals[UPDATE] =
    g_signal_new (g_intern_static_string ("update"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::layout:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted immediately before the paint signal and
   * indicates that the frame time has been updated, and signal
   * handlers should perform any preparatory work before painting.
   */
  signals[LAYOUT] =
    g_signal_new (g_intern_static_string ("layout"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::paint:
   * @clock: the frame clock emitting the signal
   *
   * Signal handlers for this signal should paint the window, screen,
   * or whatever they normally paint.
   */
  signals[PAINT] =
    g_signal_new (g_intern_static_string ("paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::after-paint:
   * @clock: the frame clock emitting the signal
   *
   * This signal is emitted immediately after the paint signal and
   * allows signal handlers to do anything they'd like to do after
   * painting has been completed. This is a relatively good time to do
   * "expensive" processing in order to get it done in between frames.
   */
  signals[AFTER_PAINT] =
    g_signal_new (g_intern_static_string ("after-paint"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GdkFrameClock::resume-events:
   * @clock: the frame clock emitting the signal
   *
   * FIXME.
   */
  signals[RESUME_EVENTS] =
    g_signal_new (g_intern_static_string ("resume-events"),
                  GDK_TYPE_FRAME_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

/**
 * gdk_frame_clock_get_frame_time:
 * @clock: the clock
 *
 * Gets the time that should currently be used for animations.  Inside
 * a paint, it's the time used to compute the animation position of
 * everything in a frame. Outside a paint, it's the time of the
 * conceptual "previous frame," which may be either the actual
 * previous frame time, or if that's too old, an updated time.
 *
 * The returned time has no relationship to wall clock time.  It
 * increases roughly at 1 millisecond per wall clock millisecond, and
 * it never decreases, but its value is only meaningful relative to
 * previous frame clock times.
 *
 *
 * Since: 3.0
 * Return value: a timestamp in milliseconds
 */
guint64
gdk_frame_clock_get_frame_time (GdkFrameClock *clock)
{
  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (clock), 0);

  return GDK_FRAME_CLOCK_GET_IFACE (clock)->get_frame_time (clock);
}

/**
 * gdk_frame_clock_request_phase:
 * @clock: the clock
 *
 * Asks the frame clock to paint a frame. The frame
 * may or may not ever be painted (the frame clock may
 * stop itself for whatever reason), but the goal in
 * normal circumstances would be to paint the frame
 * at the next expected frame time. For example
 * if the clock is running at 60fps the frame would
 * ideally be painted within 1000/60=16 milliseconds.
 *
 * Since: 3.0
 */
void
gdk_frame_clock_request_phase (GdkFrameClock      *clock,
                               GdkFrameClockPhase  phase)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_IFACE (clock)->request_phase (clock, phase);
}


void
gdk_frame_clock_freeze (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_IFACE (clock)->freeze (clock);
}


void
gdk_frame_clock_thaw (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_IFACE (clock)->thaw (clock);
}

/**
 * gdk_frame_clock_get_history:
 * @clock: the clock
 *
 * Gets the #GdkFrameHistory for the frame clock.
 *
 * Since: 3.8
 * Return value: (transfer none): the frame history object
 */
GdkFrameHistory *
gdk_frame_clock_get_history (GdkFrameClock *clock)
{
  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (clock), NULL);

  return GDK_FRAME_CLOCK_GET_IFACE (clock)->get_history (clock);
}

/**
 * gdk_frame_clock_get_requested:
 * @clock: the clock
 *
 * Gets whether a frame paint has been requested but has not been
 * performed.
 *
 *
 * Since: 3.0
 * Return value: TRUE if a frame paint is pending
 */
GdkFrameClockPhase
gdk_frame_clock_get_requested (GdkFrameClock *clock)
{
  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (clock), FALSE);

  return GDK_FRAME_CLOCK_GET_IFACE (clock)->get_requested (clock);
}

/**
 * gdk_frame_clock_get_frame_time_val:
 * @clock: the clock
 * @timeval: #GTimeVal to fill in with frame time
 *
 * Like gdk_frame_clock_get_frame_time() but returns the time as a
 * #GTimeVal which may be handy with some APIs (such as
 * #GdkPixbufAnimation).
 */
void
gdk_frame_clock_get_frame_time_val (GdkFrameClock *clock,
                                    GTimeVal      *timeval)
{
  guint64 time_ms;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  time_ms = gdk_frame_clock_get_frame_time (clock);

  timeval->tv_sec = time_ms / 1000;
  timeval->tv_usec = (time_ms % 1000) * 1000;
}

/**
 * gdk_frame_clock_frame_requested:
 * @clock: the clock
 *
 * Emits the frame-requested signal. Used in implementations of the
 * #GdkFrameClock interface.
 */
void
gdk_frame_clock_frame_requested (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  g_signal_emit (G_OBJECT (clock),
                 signals[FRAME_REQUESTED], 0);
}

GdkFrameTimings *
gdk_frame_clock_get_current_frame_timings (GdkFrameClock *clock)
{
  GdkFrameHistory *history;
  gint64 frame_counter;

  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (clock), 0);

  history = gdk_frame_clock_get_history (clock);
  frame_counter = gdk_frame_history_get_frame_counter (history);
  return gdk_frame_history_get_timings (history, frame_counter);
}


#define DEFAULT_REFRESH_INTERVAL 16667 /* 16.7ms (1/60th second) */
#define MAX_HISTORY_AGE 150000         /* 150ms */

void
gdk_frame_clock_get_refresh_info (GdkFrameClock *clock,
                                  gint64         base_time,
                                  gint64        *refresh_interval_return,
                                  gint64        *presentation_time_return)
{
  GdkFrameHistory *history;
  gint64 frame_counter;

  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  history = gdk_frame_clock_get_history (clock);
  frame_counter = gdk_frame_history_get_frame_counter (history);

  if (presentation_time_return)
    *presentation_time_return = 0;
  if (refresh_interval_return)
    *refresh_interval_return = DEFAULT_REFRESH_INTERVAL;

  while (TRUE)
    {
      GdkFrameTimings *timings = gdk_frame_history_get_timings (history, frame_counter);
      gint64 presentation_time;
      gint64 refresh_interval;

      if (timings == NULL)
        return;

      refresh_interval = gdk_frame_timings_get_refresh_interval (timings);
      presentation_time = gdk_frame_timings_get_presentation_time (timings);

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
