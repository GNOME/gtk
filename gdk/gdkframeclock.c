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
 * gdk_frame_clock_request_frame(). At that time, the frame clock
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
  BEFORE_PAINT,
  PAINT,
  AFTER_PAINT,
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
 * gdk_frame_clock_request_frame:
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
gdk_frame_clock_request_frame (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  GDK_FRAME_CLOCK_GET_IFACE (clock)->request_frame (clock);
}

/**
 * gdk_frame_clock_get_frame_requested:
 * @clock: the clock
 *
 * Gets whether a frame paint has been requested but has not been
 * performed.
 *
 *
 * Since: 3.0
 * Return value: TRUE if a frame paint is pending
 */
gboolean
gdk_frame_clock_get_frame_requested (GdkFrameClock *clock)
{
  g_return_val_if_fail (GDK_IS_FRAME_CLOCK (clock), FALSE);

  return GDK_FRAME_CLOCK_GET_IFACE (clock)->get_frame_requested (clock);
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

/**
 * gdk_frame_clock_paint:
 * @clock: the clock
 *
 * Emits the before-paint, paint, and after-paint signals. Used in
 * implementations of the #GdkFrameClock interface.
 */
void
gdk_frame_clock_paint (GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_FRAME_CLOCK (clock));

  g_signal_emit (G_OBJECT (clock),
                 signals[BEFORE_PAINT], 0);

  g_signal_emit (G_OBJECT (clock),
                 signals[PAINT], 0);

  g_signal_emit (G_OBJECT (clock),
                 signals[AFTER_PAINT], 0);
}
