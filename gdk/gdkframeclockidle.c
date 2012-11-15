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
#include "gdkframeclockidle.h"
#include "gdk.h"

#define FRAME_INTERVAL 16667 // microseconds

struct _GdkFrameClockIdlePrivate
{
  GdkFrameHistory *history;
  GTimer *timer;
  /* timer_base is used to avoid ever going backward */
  guint64 timer_base;
  guint64 frame_time;
  guint64 min_next_frame_time;
  gint64 sleep_serial;

  guint flush_idle_id;
  guint paint_idle_id;
  guint freeze_count;

  GdkFrameClockPhase requested;
  GdkFrameClockPhase phase;

  guint in_paint_idle : 1;
};

static gboolean gdk_frame_clock_flush_idle (void *data);
static gboolean gdk_frame_clock_paint_idle (void *data);

static void gdk_frame_clock_idle_finalize             (GObject                *object);
static void gdk_frame_clock_idle_interface_init       (GdkFrameClockInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkFrameClockIdle, gdk_frame_clock_idle, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GDK_TYPE_FRAME_CLOCK,
						gdk_frame_clock_idle_interface_init))

static gint64 sleep_serial;
static gint64 sleep_source_prepare_time;
static GSource *sleep_source;

gboolean
sleep_source_prepare (GSource *source,
                      gint    *timeout)
{
  sleep_source_prepare_time = g_source_get_time (source);
  *timeout = -1;
  return FALSE;
}

gboolean
sleep_source_check (GSource *source)
{
  if (g_source_get_time (source) != sleep_source_prepare_time)
    sleep_serial++;

  return FALSE;
}

gboolean
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
gdk_frame_clock_idle_class_init (GdkFrameClockIdleClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;

  gobject_class->finalize     = gdk_frame_clock_idle_finalize;

  g_type_class_add_private (klass, sizeof (GdkFrameClockIdlePrivate));
}

static void
gdk_frame_clock_idle_init (GdkFrameClockIdle *frame_clock_idle)
{
  GdkFrameClockIdlePrivate *priv;

  frame_clock_idle->priv = G_TYPE_INSTANCE_GET_PRIVATE (frame_clock_idle,
                                                        GDK_TYPE_FRAME_CLOCK_IDLE,
                                                        GdkFrameClockIdlePrivate);
  priv = frame_clock_idle->priv;

  priv->history = gdk_frame_history_new ();
  priv->timer = g_timer_new ();
  priv->freeze_count = 0;
}

static void
gdk_frame_clock_idle_finalize (GObject *object)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (object)->priv;

  g_timer_destroy (priv->timer);

  G_OBJECT_CLASS (gdk_frame_clock_idle_parent_class)->finalize (object);
}

static guint64
compute_frame_time (GdkFrameClockIdle *idle)
{
  GdkFrameClockIdlePrivate *priv = idle->priv;
  guint64 computed_frame_time;
  guint64 elapsed;

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

static guint64
gdk_frame_clock_idle_get_frame_time (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;
  guint64 computed_frame_time;

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

static void
maybe_start_idle (GdkFrameClockIdle *clock_idle)
{
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  if (priv->freeze_count == 0 && priv->requested != 0)
    {
      guint min_interval = 0;

      if (priv->min_next_frame_time != 0)
        {
          guint64 now = compute_frame_time (clock_idle);
          guint64 min_interval_us = MAX (priv->min_next_frame_time, now) - now;
          min_interval = (min_interval_us + 500) / 1000;
        }

      if (priv->flush_idle_id == 0 &&
          (priv->requested & GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0)
        {
          priv->flush_idle_id = gdk_threads_add_timeout_full (GDK_PRIORITY_EVENTS + 1,
                                                              min_interval,
                                                              gdk_frame_clock_flush_idle,
                                                              g_object_ref (clock_idle),
                                                              (GDestroyNotify) g_object_unref);
        }

      if (priv->paint_idle_id == 0 &&
          !priv->in_paint_idle &&
          (priv->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0)
        {
          priv->paint_idle_id = gdk_threads_add_timeout_full (GDK_PRIORITY_REDRAW,
                                                              min_interval,
                                                              gdk_frame_clock_paint_idle,
                                                              g_object_ref (clock_idle),
                                                              (GDestroyNotify) g_object_unref);

          gdk_frame_clock_frame_requested (GDK_FRAME_CLOCK (clock_idle));
        }
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

  if ((priv->requested & ~GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS) != 0)
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
  gint64 frame_counter = 0;

  priv->paint_idle_id = 0;
  priv->in_paint_idle = TRUE;
  priv->min_next_frame_time = 0;

  skip_to_resume_events =
    (priv->requested & ~(GDK_FRAME_CLOCK_PHASE_FLUSH_EVENTS | GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS)) == 0;

  if (priv->phase > GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT)
    {
      frame_counter = gdk_frame_history_get_frame_counter (priv->history);
      timings = gdk_frame_history_get_timings (priv->history, frame_counter);
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

              gdk_frame_history_begin_frame (priv->history);
              frame_counter = gdk_frame_history_get_frame_counter (priv->history);
              timings = gdk_frame_history_get_timings (priv->history, frame_counter);

              gdk_frame_timings_set_frame_time (timings, priv->frame_time);

              gdk_frame_timings_set_slept_before (timings,
                                                  priv->sleep_serial != get_sleep_serial ());

              priv->phase = GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;

              /* We always emit ::before-paint and ::after-paint if
               * any of the intermediate phases are requested and
               * they don't get repeated if you freeze/thaw while
               * in them. */
              priv->requested &= ~GDK_FRAME_CLOCK_PHASE_BEFORE_PAINT;
              g_signal_emit_by_name (G_OBJECT (clock), "before-paint");
              priv->phase = GDK_FRAME_CLOCK_PHASE_UPDATE;
            }
        case GDK_FRAME_CLOCK_PHASE_UPDATE:
          if (priv->freeze_count == 0)
            {
              if (priv->requested & GDK_FRAME_CLOCK_PHASE_UPDATE)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_UPDATE;
                  g_signal_emit_by_name (G_OBJECT (clock), "update");
                }
            }
        case GDK_FRAME_CLOCK_PHASE_LAYOUT:
          if (priv->freeze_count == 0)
            {
#ifdef G_ENABLE_DEBUG
              if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_LAYOUT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT))
                    _gdk_frame_timings_set_layout_start_time (timings, g_get_monotonic_time ());
                }
#endif /* G_ENABLE_DEBUG */

              priv->phase = GDK_FRAME_CLOCK_PHASE_LAYOUT;
              if (priv->requested & GDK_FRAME_CLOCK_PHASE_LAYOUT)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_LAYOUT;
                  g_signal_emit_by_name (G_OBJECT (clock), "layout");
                }
            }
        case GDK_FRAME_CLOCK_PHASE_PAINT:
          if (priv->freeze_count == 0)
            {
#ifdef G_ENABLE_DEBUG
              if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
                {
                  if (priv->phase != GDK_FRAME_CLOCK_PHASE_PAINT &&
                      (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT))
                    _gdk_frame_timings_set_paint_start_time (timings, g_get_monotonic_time ());
                }
#endif /* G_ENABLE_DEBUG */

              priv->phase = GDK_FRAME_CLOCK_PHASE_PAINT;
              if (priv->requested & GDK_FRAME_CLOCK_PHASE_PAINT)
                {
                  priv->requested &= ~GDK_FRAME_CLOCK_PHASE_PAINT;
                  g_signal_emit_by_name (G_OBJECT (clock), "paint");
                }
            }
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
                _gdk_frame_timings_set_frame_end_time (timings, g_get_monotonic_time ());
#endif /* G_ENABLE_DEBUG */
            }
        case GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS:
          ;
        }
    }

#ifdef G_ENABLE_DEBUG
  if ((_gdk_debug_flags & GDK_DEBUG_FRAMES) != 0)
    {
      if (gdk_frame_timings_get_complete (timings))
        _gdk_frame_history_debug_print (priv->history, timings);
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

static GdkFrameClockPhase
gdk_frame_clock_idle_get_requested (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;

  return priv->requested;
}

static void
gdk_frame_clock_idle_freeze (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;

  priv->freeze_count++;

  if (priv->freeze_count == 1)
    {
      if (priv->flush_idle_id)
	{
	  g_source_remove (priv->flush_idle_id);
	  priv->flush_idle_id = 0;
	}
      if (priv->paint_idle_id)
	{
	  g_source_remove (priv->paint_idle_id);
	  priv->paint_idle_id = 0;
	}
    }
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
       * run and do it for us. */
      if (priv->paint_idle_id == 0)
        priv->phase = GDK_FRAME_CLOCK_PHASE_NONE;

      priv->sleep_serial = get_sleep_serial ();
    }
}

static GdkFrameHistory *
gdk_frame_clock_idle_get_history (GdkFrameClock *clock)
{
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  return priv->history;
}

static void
gdk_frame_clock_idle_interface_init (GdkFrameClockInterface *iface)
{
  iface->get_frame_time = gdk_frame_clock_idle_get_frame_time;
  iface->request_phase = gdk_frame_clock_idle_request_phase;
  iface->get_requested = gdk_frame_clock_idle_get_requested;
  iface->freeze = gdk_frame_clock_idle_freeze;
  iface->thaw = gdk_frame_clock_idle_thaw;
  iface->get_history = gdk_frame_clock_idle_get_history;
}

GdkFrameClock *
_gdk_frame_clock_idle_new (void)
{
  GdkFrameClockIdle *clock;

  clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);

  return GDK_FRAME_CLOCK (clock);
}
