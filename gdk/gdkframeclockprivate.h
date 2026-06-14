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

/* Uninstalled header, internal to GDK */

#pragma once

#include <gdk/gdkframeclock.h>

G_BEGIN_DECLS

/* little hacks to avoid requiring 2.88 */
#if GLIB_CHECK_VERSION(2, 87, 3)
#undef G_NSEC_PER_SEC
static inline uint64_t
avoid_deprecation_monotonic_time_ns (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_get_monotonic_time_ns ();
G_GNUC_END_IGNORE_DEPRECATIONS
}
#define g_get_monotonic_time_ns() avoid_deprecation_monotonic_time_ns()
#else
#define g_get_monotonic_time_ns() ((uint64_t) 1000 * g_get_monotonic_time ())
#endif
#define G_NSEC_PER_SEC G_GUINT64_CONSTANT(1000000000)

/**
 * GdkFrameClock:
 * @parent_instance: The parent instance.
 */
struct _GdkFrameClock
{
  GObject parent_instance;
};

/**
 * GdkFrameClockClass:
 * @parent_class: The parent class.

 * @get_frame_time: Gets the time that should currently be used for
 *    animations.
 * @request_phase: Asks the frame clock to run a particular phase.
 * @begin_updating: Starts updates for an animation.
 * @end_updating: Stops updates for an animation.
 * @freeze: 
 * @thaw: 
 */
struct _GdkFrameClockClass
{
  GObjectClass parent_class;

  /*< public >*/

  gint64   (* get_frame_time) (GdkFrameClock *clock);

  void     (* request_phase)  (GdkFrameClock      *clock,
                               GdkFrameClockPhase  phase);
  void     (* begin_updating) (GdkFrameClock      *clock);
  void     (* end_updating)   (GdkFrameClock      *clock);

  void     (* start)             (GdkFrameClock *clock);
  void     (* stop)              (GdkFrameClock *clock);

  /* signals */
  /* void (* flush_events)       (GdkFrameClock *clock); */
  /* void (* before_paint)       (GdkFrameClock *clock); */
  /* void (* update)             (GdkFrameClock *clock); */
  /* void (* layout)             (GdkFrameClock *clock); */
  /* void (* paint)              (GdkFrameClock *clock); */
  /* void (* after_paint)        (GdkFrameClock *clock); */
  /* void (* resume_events)      (GdkFrameClock *clock); */
};

void gdk_frame_clock_start               (GdkFrameClock *clock);
void gdk_frame_clock_stop                (GdkFrameClock *clock);
gboolean gdk_frame_clock_is_stopped      (GdkFrameClock *clock);

void _gdk_frame_clock_begin_frame         (GdkFrameClock   *clock,
                                           gint64           monotonic_time);
void _gdk_frame_clock_debug_print_timings (GdkFrameClock   *clock,
                                           GdkFrameTimings *timings);
void _gdk_frame_clock_add_timings_to_profiler (GdkFrameClock *frame_clock,
                                               GdkFrameTimings *timings);

void _gdk_frame_clock_emit_flush_events  (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_before_paint  (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_update        (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_layout        (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_paint         (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_after_paint   (GdkFrameClock *frame_clock);
void _gdk_frame_clock_emit_resume_events (GdkFrameClock *frame_clock);

void            gdk_frame_clock_submitted                       (GdkFrameClock          *self,
                                                                 gint64                  frame_counter,
                                                                 uint64_t                refresh);
void            gdk_frame_clock_discarded                       (GdkFrameClock          *self,
                                                                 gint64                  frame_counter);
void            gdk_frame_clock_presented                       (GdkFrameClock          *self,
                                                                 gint64                  frame_counter,
                                                                 uint64_t                presentation_time,
                                                                 uint64_t                refresh);

G_END_DECLS

