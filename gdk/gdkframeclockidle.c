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

#include "gdkframeclockidle.h"
#include "gdk.h"

struct _GdkFrameClockIdlePrivate
{
  GTimer *timer;
  /* timer_base is used to avoid ever going backward */
  guint64 timer_base;
  guint64 frame_time;

  guint idle_id;

  unsigned int in_paint : 1;
};

static void gdk_frame_clock_idle_finalize             (GObject                *object);
static void gdk_frame_clock_idle_interface_init       (GdkFrameClockInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkFrameClockIdle, gdk_frame_clock_idle, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GDK_TYPE_FRAME_CLOCK,
						gdk_frame_clock_idle_interface_init))

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

  priv->timer = g_timer_new ();
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

  elapsed = ((guint64) (g_timer_elapsed (priv->timer, NULL) * 1000)) + priv->timer_base;
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
  if (priv->in_paint)
    return priv->frame_time;

  /* Outside a paint, pick something close to "now" */
  computed_frame_time = compute_frame_time (GDK_FRAME_CLOCK_IDLE (clock));

  /* 16ms is 60fps. We only update frame time that often because we'd
   * like to try to keep animations on the same start times.
   * get_frame_time() would normally be used outside of a paint to
   * record an animation start time for example.
   */
  if ((computed_frame_time - priv->frame_time) > 16)
    priv->frame_time = computed_frame_time;

  return priv->frame_time;
}

static gboolean
gdk_frame_clock_paint_idle (void *data)
{
  GdkFrameClock *clock = GDK_FRAME_CLOCK (data);
  GdkFrameClockIdle *clock_idle = GDK_FRAME_CLOCK_IDLE (clock);
  GdkFrameClockIdlePrivate *priv = clock_idle->priv;

  priv->idle_id = 0;

  priv->in_paint = TRUE;
  priv->frame_time = compute_frame_time (clock_idle);

  gdk_frame_clock_paint (clock);

  priv->in_paint = FALSE;

  return FALSE;
}

static void
gdk_frame_clock_idle_request_frame (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;

  if (priv->idle_id == 0)
    {
      priv->idle_id = gdk_threads_add_idle_full (GDK_PRIORITY_REDRAW,
                                                 gdk_frame_clock_paint_idle,
                                                 g_object_ref (clock),
                                                 (GDestroyNotify) g_object_unref);

      gdk_frame_clock_frame_requested (clock);
    }
}

static gboolean
gdk_frame_clock_idle_get_frame_requested (GdkFrameClock *clock)
{
  GdkFrameClockIdlePrivate *priv = GDK_FRAME_CLOCK_IDLE (clock)->priv;

  return priv->idle_id != 0;
}

static void
gdk_frame_clock_idle_interface_init (GdkFrameClockInterface *iface)
{
  iface->get_frame_time = gdk_frame_clock_idle_get_frame_time;
  iface->request_frame = gdk_frame_clock_idle_request_frame;
  iface->get_frame_requested = gdk_frame_clock_idle_get_frame_requested;
}
