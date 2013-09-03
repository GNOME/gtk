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

#include "gdkinternals.h"
#include "gdkframeclockprivate.h"
#include "gdkframeclockidle.h"
#include "gdk.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define FRAME_INTERVAL 16667 /* microseconds */

struct _GdkFrameClockIdlePrivate
{
  GTimer *timer;
  /* timer_base is used to avoid ever going backward */
  gint64 timer_base;
  gint64 frame_time;
  gint64 min_next_frame_time;
  gint64 sleep_serial;

  guint flush_idle_id;
  guint paint_idle_id;
  guint freeze_count;
  guint updating_count;

  GdkFrameClockPhase requested;
  GdkFrameClockPhase phase;

  guint in_paint_idle : 1;
#ifdef G_OS_WIN32
  guint begin_period : 1;
#endif
};

static gboolean gdk_frame_clock_flush_idle (void *data);
static gboolean gdk_frame_clock_paint_idle (void *data);

G_DEFINE_TYPE_WITH_PRIVATE (GdkFrameClockIdle, gdk_frame_clock_idle, GDK_TYPE_FRAME_CLOCK)

static gint64 sleep_serial;
static gint64 sleep_source_prepare_time;
static GSource *sleep_source;

static gboolean
sleep_source_prepare (GSource *source,
                      gint    *timeout)
{
  sleep_source_prepare_time = g_source_get_time (source);
  *timeout = -1;
  return FALSE;
}

static gboolean
sleep_source_check (GSource *source)
{
  if (g_source_get_time (source) != sleep_source_prepare_time)
    sleep_serial++;

  return FALSE;
}

static gboolean
sleep_source_dispatch (GSource     *source,
                       GSourceFunc  callback,
                       gpointer     user_data)
{
  return TRUE;
}

static GSourceFuncs sleep_source_funcs = {
  sleep_source_prepare,
  sleep_source_check,
  sleep_source_dispatch,
  NULL /* finalize */
};

static gint64
get_sleep_serial (void)
{
  if (sleep_source == NULL)
    {
      sleep_source = g_source_new (&sleep_source_funcs, sizeof (GSource));

      g_source_set_priority (sleep_source, G_PRIORITY_HIGH);
      g_source_attach (sleep_source, NULL);
      g_source_unref (sleep_source);
    }

  return sleep_serial;
}

static void
gdk_frame_clock_idle_init (GdkFrameClockIdle *frame_clock_idle)
{
  GdkFrameClockIdlePrivate *priv;

  frame_clock_idle->priv = priv =
    gdk_frame_clock_idle_get_instance_private (frame_clock_idle);

  priv->freeze_count = 0;
}

static void
gdk_frame_clock_idle_dispose (GObject *object)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (object)->priv;

  if (priv->flush_idle_id != 0)
    {
      g_source_remove (priv->flush_idle_id);
      priv->flush_idle_id = 0;
    }

  if (priv->paint_idle_id != 0)
    {
      g_source_remove (priv->paint_idle_id);
      priv->paint_idle_id = 0;
    }

#ifdef G_OS_WIN32
  if (priv->begin_period) 
    {
      timeEndPeriod(1);
      priv->begin_period = FALSE;
    }
#endif

  G_OBJECT_CLASS (gdk_frame_clock_idle_parent_class)->dispose (object);
}

static gint64
compute_frame_time (GdkFrameClockIdle *idle)
{
  GdkFrameClockIdlePrivate *priv = idle->priv;
  gint64 computed_frame_time;
  gint64 elapsed;

  elapsed = g_get_monotonic_time () + priv->timer_base;
  if (elapsed < priv->frame_time)
    {
      /* clock went backward. adapt to that by forevermore increasing
       * timer_base.  For now, assume we've gone forward in time 1ms.
       */
      /* hmm. just fix GTimer? */
      computed_frame_time = priv->frame_time + 1;
      priv->timer_base += (priv->frame_time - elapsed) + 1;
    }
  else
    {
      computed_frame_time = elapsed;
    }

  return computed_frame_time;
}

static gint64
gdk_frame_clock_idle_get_frame_time (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;
  gint64 computed_frame_time;

  /* can't change frame time during a paint */
  if (priv->phase != GDK_FRAME_CLOCK_PHASE_NONE &&
      priv->phase != GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS)
    return priv->frame_time;

  /* Outside a paint, pick something close to "now" */
  computed_frame_time = compute_frame_time (GDK_FRAME_CLOCK_IDLE (clock));

  /* 16ms is 60fps. We only update frame time that often because we'd
   * like to try to keep animations on the same start times.
   * get_frame_time() would normally be used outside of a paint to
   * record an animation start time for example.
   */
  if ((computed_frame_time - priv->frame_time) > FRAME_INTERVAL)
    priv->frame_time = computed_frame_time;

  return priv->frame_time;
}

#define RUN_FLUSH_IDLE(priv)                                            \
  ((priv)->freeze_count == 0 &&                                         \
   ((priv)->requested & GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0)

/* The reason why we track updating_count separately here and don't
 * just add GDK_FRAME_CLOCK_PHASE_UPDATE into ->request on every frame
 * is so that we can avoid doing one more frame when an animation
 * is cancelled.
 */
#define RUN_PAINT_IDLE(priv)                                            \
  ((priv)->freeze_count == 0 &&                                         \
   (((priv)->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0 ||   \
    (priv)->updating_count > 0))

static void
maybe_start_idle (GdkFrameClockIdle *clock_idle)
{
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  if (RUN_FLUSH_IDLE (priv) || RUN_PAINT_IDLE (priv))
    {
      guint min_interval = 0;

      if (priv->min_next_frame_time != 0)
        {
          gint64 now = compute_frame_time (clock_idle);
          gint64 min_interval_us = MAX (priv->min_next_frame_time, now) - now;
          min_interval = (min_interval_us + 500) / 1000;
        }

      if (priv->flush_idle_id == 0 && RUN_FLUSH_IDLE (priv))
        {
          priv->flush_idle_id = gdk_threads_add_timeout_full (GDK_PRIORITY_EVENTS + 1,
                                                              min_interval,
                                                              gdk_frame_clock_flush_idle,
                                                              g_object_ref (clock_idle),
                                                              (GDestroyNotify) g_object_unref);
        }

      if (!priv->in_paint_idle &&
	  priv->paint_idle_id == 0 && RUN_PAINT_IDLE (priv))
        {
          priv->paint_idle_id = gdk_threads_add_timeout_full (GDK_PRIORITY_REDRAW,
                                                              min_interval,
                                                              gdk_frame_clock_paint_idle,
                                                              g_object_ref (clock_idle),
                                                              (GDestroyNotify) g_object_unref);
        }
    }
}

static void
maybe_stop_idle (GdkFrameClockIdle *clock_idle)
{
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  if (priv->flush_idle_id != 0 && !RUN_FLUSH_IDLE (priv))
    {
      g_source_remove (priv->flush_idle_id);
      priv->flush_idle_id = 0;
    }

  if (priv->paint_idle_id != 0 && !RUN_PAINT_IDLE (priv))
    {
      g_source_remove (priv->paint_idle_id);
      priv->paint_idle_id = 0;
    }
}

static gint64
compute_min_next_frame_time (GdkFrameClockIdle *clock_idle,
                             gint64             last_frame_time)
{
  gint64 presentation_time;
  gint64 refresh_interval;

  gdk_frame_clock_get_refresh_info (GDK_FRAME_CLOCK (clock_idle),
                                    last_frame_time,
                                    &refresh_interval, &presentation_time);

  if (presentation_time == 0)
    return last_frame_time + refresh_interval;
  else
    return presentation_time + refresh_interval / 2;
}

static gboolean
gdk_frame_clock_flush_idle (void *data)
{
  GdkFrameClock *clock = GDK_FRAME_CLOCK (data);
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->flush_idle_id = 0;

  if (priv->phase != GDK_FRAME_CLOCK_PHASE_NONE)
    return FALSE;

  priv->phase = GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS;
  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS;

  g_signal_emit_by_name (G_OBJECT (clock), "flush-events");

  if ((priv->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0 ||
      priv->updating_count > 0)
    priv->phase = GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;
  else
    priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

  return FALSE;
}

static gboolean
gdk_frame_clock_paint_idle (void *data)
{
  GdkFrameClock *clock = GDK_FRAME_CLOCK (data);
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;
  gboolean skip_to_resume_events;
  GdkFrameTimings *timings = NULL;

  priv->paint_idle_id = 0;
  priv->in_paint_idle = TRUE;
  priv->min_next_frame_time = 0;

  skip_to_resume_events =
    (priv->requested & ~(GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS | GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS)) == 0 &&
    priv->updating_count == 0;

  if (priv->phase > GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT)
    {
      timings = gdk_frame_clock_get_current_timings (clock);
    }

  if (!skip_to_resume_events)
    {
      switch (priv->phase)
        {
        case GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS:
          break;
        case GDK_FRAME_CLOCK_PHASE_NONE:
        case GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT:
          if (priv->freeze_count == 0)
            {
              priv->frame_time = compute_frame_time (clock_idle);

              _gdk_frame_clock_begin_frame (clock);
              timings = gdk_frame_clock_get_current_timings (clock);

              timings->frame_time = priv->frame_time;
              timings->slept_before = priv->sleep_serial != get_sleep_serial ();

              priv->phase = GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;

              /* We always emit ::before-paint and ::after-paint if
               * any of the intermediate phases are requested and
               * they don't get repeated if you freeze/thaw while
               * in them.
               */
              priv->requested &= ~GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;
              g_signal_emit_by_name (G_OBJECT (clock), "before-paint");
              priv->phase = GDK_FRAME_CLOCK_PHASE_UPDATE;
            }
          /* fallthrough */
        case GDK_FRAME_CLOCK_PHASE_UPDATE:
          if (priv->freeze_count == 0)
            {
              if ((priv->requested & GDK_FRAME_CLOCK_PHASE_UPDATE) != 0 ||
                  priv->updating_count > 0)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_UPDATE;
                  g_signal_emit_by_name (G_OBJECT (clock), "update");
                }
            }
          /* fallthrough */
        case GDK_FRAME_CLOCK_PHASE_LAYOUT:
          if (priv->freeze_count == 0)
            {
	      int iter;
#ifdef G_ENABLE_DEBUG
              if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_LAYOUT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT))
                    timings->layout_start_time = g_get_monotonic_time ();
                }
#endif /* G_ENABLE_DEBUG */

              priv->phase = GDK_FRAME_CLOCK_PHASE_LAYOUT;
	      /* We loop in the layout phase, because we don't want to progress
	       * into the paint phase with invalid size allocations. This may
	       * happen in some situation like races between user window
	       * resizes and natural size changes.
	       */
	      iter = 0;
              while ((priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT) &&
		     priv->freeze_count == 0 && iter++ < 4)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_LAYOUT;
                  g_signal_emit_by_name (G_OBJECT (clock), "layout");
                }
	      if (iter == 5)
		g_warning ("gdk-frame-clock: layout continuously requested, giving up after 4 tries");
            }
          /* fallthrough */
        case GDK_FRAME_CLOCK_PHASE_PAINT:
          if (priv->freeze_count == 0)
            {
#ifdef G_ENABLE_DEBUG
              if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_PAINT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT))
                    timings->paint_start_time = g_get_monotonic_time ();
                }
#endif /* G_ENABLE_DEBUG */

              priv->phase = GDK_FRAME_CLOCK_PHASE_PAINT;
              if (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_PAINT;
                  g_signal_emit_by_name (G_OBJECT (clock), "paint");
                }
            }
          /* fallthrough */
        case GDK_FRAME_CLOCK_PHASE_AFTER_PAINT:
          if (priv->freeze_count == 0)
            {
              priv->requested &= ~GDK_FRAME_CLOCK_PHASE_AFTER_PAINT;
              g_signal_emit_by_name (G_OBJECT (clock), "after-paint");
              /* the ::after-paint phase doesn't get repeated on freeze/thaw,
               */
              priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

#ifdef G_ENABLE_DEBUG
              if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
                timings->frame_end_time = g_get_monotonic_time ();
#endif /* G_ENABLE_DEBUG */
            }
          /* fallthrough */
        case GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS:
          ;
        }
    }

#ifdef G_ENABLE_DEBUG
  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    {
      if (timings && timings->complete)
        _gdk_frame_clock_debug_print_timings (clock, timings);
    }
#endif /* G_ENABLE_DEBUG */

  if (priv->requested & GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS)
    {
      priv->requested &= ~GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS;
      g_signal_emit_by_name (G_OBJECT (clock), "resume-events");
    }

  if (priv->freeze_count == 0)
    priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

  priv->in_paint_idle = FALSE;

  /* If there is throttling in the backend layer, then we'll do another
   * update as soon as the backend unthrottles (if there is work to do),
   * otherwise we need to figure when the next frame should be.
   */
  if (priv->freeze_count == 0)
    {
      priv->min_next_frame_time = compute_min_next_frame_time (clock_idle,
                                                               priv->frame_time);
      maybe_start_idle (clock_idle);
    }

  if (priv->freeze_count == 0)
    priv->sleep_serial = get_sleep_serial ();

  return FALSE;
}

static void
gdk_frame_clock_idle_request_phase (GdkFrameClock      *clock,
                                    GdkFrameClockPhase  phase)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->requested |= phase;
  maybe_start_idle (clock_idle);
}

static void
gdk_frame_clock_idle_begin_updating (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

#ifdef G_OS_WIN32
  /* We need a higher resolution timer while doing animations */
  if (priv->updating_count == 0 && !priv->begin_period)
    {
      timeBeginPeriod(1);
      priv->begin_period = TRUE;
    }
#endif

  priv->updating_count++;
  maybe_start_idle (clock_idle);
}

static void
gdk_frame_clock_idle_end_updating (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  g_return_if_fail (priv->updating_count > 0);

  priv->updating_count--;
  maybe_stop_idle (clock_idle);

#ifdef G_OS_WIN32
  if (priv->updating_count == 0 && priv->begin_period)
    {
      timeEndPeriod(1);
      priv->begin_period = FALSE;
    }
#endif
}

static void
gdk_frame_clock_idle_freeze (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->freeze_count++;
  maybe_stop_idle (clock_idle);
}

static void
gdk_frame_clock_idle_thaw (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  g_return_if_fail (priv->freeze_count > 0);

  priv->freeze_count--;
  if (priv->freeze_count == 0)
    {
      maybe_start_idle (clock_idle);
      /* If nothing is requested so we didn't start an idle, we need
       * to skip to the end of the state chain, since the idle won't
       * run and do it for us.
       */
      if (priv->paint_idle_id == 0)
        priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

      priv->sleep_serial = get_sleep_serial ();
    }
}

static void
gdk_frame_clock_idle_class_init (GdkFrameClockIdleClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GdkFrameClockClass *frame_clock_class = (GdkFrameClockClass *)klass;

  gobject_class->dispose = gdk_frame_clock_idle_dispose;

  frame_clock_class->get_frame_time = gdk_frame_clock_idle_get_frame_time;
  frame_clock_class->request_phase = gdk_frame_clock_idle_request_phase;
  frame_clock_class->begin_updating = gdk_frame_clock_idle_begin_updating;
  frame_clock_class->end_updating = gdk_frame_clock_idle_end_updating;
  frame_clock_class->freeze = gdk_frame_clock_idle_freeze;
  frame_clock_class->thaw = gdk_frame_clock_idle_thaw;
}

GdkFrameClock *
_gdk_frame_clock_idle_new (void)
{
  GdkFrameClockIdle *clock;

  clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);

  return GDK_FRAME_CLOCK (clock);
}
