/*
 * Copyright © 2026 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "frametimeoverlay.h"

#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtknative.h"
#include "gtkmain.h"

#include "gdk/gdkframetimingsprivate.h"

#define WIDTH 300
#define HEIGHT 50

struct _GtkFrameTimeOverlay
{
  GtkInspectorOverlay parent_instance;
};

struct _GtkFrameTimeOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkFrameTimeOverlay, gtk_frame_time_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static uint64_t
frame_timings_get_frame_start (GdkFrameTimings *timings)
{
  return gdk_frame_timings_get_start_time (timings, GDK_FRAME_STAGE_FLUSH_EVENTS);
}

static uint64_t
frame_timings_get_frame_end (GdkFrameTimings *timings)
{
  return gdk_frame_timings_get_end_time (timings, GDK_FRAME_STAGE_RESUME_EVENTS);
}

struct {
  GdkRGBA color;
  uint64_t (* get_time) (GdkFrameTimings *timings);
} const points[] = {
  { { 0.0, 1.0, 0.0, 1.0 }, gdk_frame_timings_get_frame_time_ns },
  { { 0.0, 0.0, 1.0, 1.0 }, gdk_frame_timings_get_presentation_time_ns },
  { { 0.4, 0.0, 1.0, 1.0 }, gdk_frame_timings_get_predicted_presentation_time_ns },
};

struct {
  GdkRGBA color;
  uint64_t (* get_start_time) (GdkFrameTimings *timings);
  uint64_t (* get_end_time) (GdkFrameTimings *timings);
} const ranges[] = {
  { { 0.7, 0.7, 0.7, 1.0 }, frame_timings_get_frame_start, frame_timings_get_frame_end, },
  { { 0.5, 0.0, 0.0, 0.25 }, frame_timings_get_frame_end, gdk_frame_timings_get_throttling_hint },
};

static gsize
get_point (uint64_t t,
           uint64_t max_time,
           uint64_t ns_scale)
{
  t = max_time - t;
  return (t + ns_scale - 1) / ns_scale;
}

static gboolean
draw_point (guint32 *data,
            gsize    size,
            guint32  color,
            uint64_t t,
            uint64_t max_time,
            uint64_t ns_scale)
{
  if (t > max_time)
    return TRUE;

  t = get_point (t, max_time, ns_scale);
  if (t >= size)
    return FALSE;

  data[t] = color;
  return TRUE;
}
            
static gboolean
draw_line (guint32 *data,
           gsize    size,
           guint32  color,
           uint64_t start,
           uint64_t end,
           uint64_t max_time,
           uint64_t ns_scale)
{
  gsize s, e, i;

  if (end > max_time)
    return TRUE;
  e = get_point (end, max_time, ns_scale);
  if (e >= size)
    return FALSE;
  if (start > max_time)
    s = 0;
  else
    s = get_point (start, max_time, ns_scale);
  s = MIN (s + 1, size);

  for (i = e; i < s; i++)
    data[i] = color;

  return TRUE;
}
            
static void
gtk_frame_time_overlay_get_timeline_values (GdkFrameClock *clock,
                                            gsize          size,
                                            uint64_t      *end_time,
                                            uint64_t      *ns_per_pixel)
{
  GdkFrameTimings *timings;
  gint64 presentation, predicted, refresh;

  timings = gdk_frame_clock_get_current_timings (clock);
  predicted = gdk_frame_timings_get_predicted_presentation_time_ns (timings);
  gdk_frame_clock_get_refresh_info (clock, 
                                    gdk_frame_clock_get_frame_time (clock),
                                    &refresh,
                                    &presentation);
  presentation *= 1000;
  refresh *= 1000;
  if (presentation == 0)
    presentation = predicted;

  /* We assume that the predicted presentation time of this frame is the
   * newest time we might get */
  *end_time = ((predicted - presentation + refresh - 1) / refresh
              * refresh + presentation);
  *ns_per_pixel = refresh / size;
}

static guint32
rgba_to_color (const GdkRGBA *rgba)
{
  return ((guint32) (0xFF * 1.0f        * rgba->alpha)) << 24 |
         ((guint32) (0xFF * rgba->red   * rgba->alpha)) << 16 |
         ((guint32) (0xFF * rgba->green * rgba->alpha)) <<  8 |
         ((guint32) (0xFF * rgba->blue  * rgba->alpha)) <<  0;
}

static void
gtk_frame_time_overlay_snapshot (GtkInspectorOverlay *overlay,
                                 GtkSnapshot         *snapshot,
                                 GskRenderNode       *node,
                                 GtkWidget           *widget)
{
  guint32 *data;
  gsize width, height, stride, size;
  gint64 start, end, i;
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  graphene_rect_t bounds;
  gboolean has_bounds;
  GBytes *bytes;
  GdkTexture *texture;
  uint64_t max_time, t, nspp; /* nanoseconds per pixel */
  float scale;
  gsize j;

  scale = gdk_surface_get_scale (gtk_native_get_surface (gtk_widget_get_native (widget)));
  clock = gtk_widget_get_frame_clock (widget);
  start = gdk_frame_clock_get_frame_counter (clock);
  end = gdk_frame_clock_get_history_start (clock);
  if (end >= start)
    return;

  width = ceil (75 * scale);
  height = 600;
  height = MIN (height, gtk_widget_get_width (widget));
  //height = ceil (height * scale);
  size = width * height;
  stride = sizeof (guint32) * width;
  data = g_malloc0_n (stride, height);

  gtk_frame_time_overlay_get_timeline_values (clock, width, &max_time, &nspp);

  for (j = G_N_ELEMENTS (ranges); j-- > 0; )
    {
      for (i = start; i >= end; i--)
        {
          uint64_t st, et;

          timings = gdk_frame_clock_get_timings (clock, i);
          g_assert (timings);

          st = ranges[j].get_start_time (timings);
          st = MIN (st, max_time);
          et = ranges[j].get_end_time (timings);
          et = MIN (et, max_time);
          if (et > st)
            {
              if (!draw_line (data, size, rgba_to_color (&ranges[j].color), st, et, max_time, nspp))
                break;
            }
        }
    }

  for (j = G_N_ELEMENTS (points); j-- > 0; )
    {
      for (i = start; i >= end; i--)
        {
          timings = gdk_frame_clock_get_timings (clock, i);
          g_assert (timings);

          t = points[j].get_time (timings);
          if (!draw_point (data, size, rgba_to_color (&points[j].color), t, max_time, nspp))
            break;
        }
    }

  if (GTK_IS_WINDOW (widget))
    {
      GtkWidget *child = gtk_window_get_child (GTK_WINDOW (widget));
      if (!child ||
          !gtk_widget_compute_bounds (child, widget, &bounds))
        has_bounds = gtk_widget_compute_bounds (widget, widget, &bounds);
      else
        has_bounds = gtk_widget_compute_bounds (child, widget, &bounds);
    }
   else
    {
      has_bounds = gtk_widget_compute_bounds (widget, widget, &bounds);
    }

  bytes = g_bytes_new_take (data, stride * height);
  texture = gdk_memory_texture_new (width, height,
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    stride);
  g_bytes_unref (bytes);

  gtk_snapshot_save (snapshot);
  if (has_bounds)
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (bounds.origin.x + bounds.size.width, bounds.origin.y));
  gtk_snapshot_rotate (snapshot, 90);

  gtk_snapshot_scale (snapshot, 1 / scale, 1 / scale);
  gtk_snapshot_set_snap (snapshot, GSK_RECT_SNAP_ROUND);
  gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 0, 0, 0, 0.75 }, &GRAPHENE_RECT_INIT(0, 0, width, height));
  gtk_snapshot_append_texture (snapshot, texture, &GRAPHENE_RECT_INIT(0, 0, width, height));

  gtk_snapshot_restore (snapshot);

  g_object_unref (texture);
}

static void
gtk_frame_time_overlay_class_init (GtkFrameTimeOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);

  overlay_class->snapshot = gtk_frame_time_overlay_snapshot;
}

static void
gtk_frame_time_overlay_init (GtkFrameTimeOverlay *self)
{
}

GtkInspectorOverlay *
gtk_frame_time_overlay_new (void)
{
  GtkFrameTimeOverlay *self;

  self = g_object_new (GTK_TYPE_FRAME_TIME_OVERLAY, NULL);

  return GTK_INSPECTOR_OVERLAY (self);
}

