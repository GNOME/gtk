/*
 * Copyright © 2018 Benjamin Otte
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

#include "fpsoverlay.h"

#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtknative.h"

/* duration before we start fading in us */
#define GDK_FPS_OVERLAY_LINGER_DURATION (1000 * 1000)
/* duration when fade is finished in us */
#define GDK_FPS_OVERLAY_FADE_DURATION (500 * 1000)

typedef struct _GtkFpsInfo {
  gint64 last_frame;
  GskRenderNode *last_node;
} GtkFpsInfo;

struct _GtkFpsOverlay
{
  GtkInspectorOverlay parent_instance;

  GHashTable *infos; /* GtkWidget => GtkFpsInfo */
};

struct _GtkFpsOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkFpsOverlay, gtk_fps_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

static void
gtk_fps_info_free (gpointer data)
{
  GtkFpsInfo *info = data;

  gsk_render_node_unref (info->last_node);

  g_slice_free (GtkFpsInfo, info);
}

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
gtk_fps_overlay_get_fps (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;
  GdkFrameTimings *start, *end;
  gint64 start_counter, end_counter;
  gint64 start_timestamp, end_timestamp;
  gint64 interval;

  frame_clock = gtk_widget_get_frame_clock (widget);
  if (frame_clock == NULL)
    return 0.0;

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

static gboolean
gtk_fps_overlay_force_redraw (GtkWidget     *widget,
                              GdkFrameClock *clock,
                              gpointer       unused)
{
  gdk_surface_queue_expose (gtk_native_get_surface (gtk_widget_get_native (widget)));

  return G_SOURCE_REMOVE;
}

static void
gtk_fps_overlay_snapshot (GtkInspectorOverlay *overlay,
                          GtkSnapshot         *snapshot,
                          GskRenderNode       *node,
                          GtkWidget           *widget)
{
  GtkFpsOverlay *self = GTK_FPS_OVERLAY (overlay);
  GtkFpsInfo *info;
  PangoLayout *layout;
  gint64 now;
  double fps;
  char *fps_string;
  graphene_rect_t bounds;
  gboolean has_bounds = FALSE;
  int width, height;
  double overlay_opacity;

  now = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
  info = g_hash_table_lookup (self->infos, widget);
  if (info == NULL)
    {
      info = g_slice_new0 (GtkFpsInfo);
      g_hash_table_insert (self->infos, widget, info);
    }
  if (info->last_node != node)
    {
      g_clear_pointer (&info->last_node, gsk_render_node_unref);
      info->last_node = gsk_render_node_ref (node);
      info->last_frame = now;
      overlay_opacity = 1.0;
    }
  else
    {
      if (now - info->last_frame > GDK_FPS_OVERLAY_LINGER_DURATION + GDK_FPS_OVERLAY_FADE_DURATION)
        {
          g_hash_table_remove (self->infos, widget);
          return;
        }
      else if (now - info->last_frame > GDK_FPS_OVERLAY_LINGER_DURATION)
        {
          overlay_opacity = 1.0 - (double) (now - info->last_frame - GDK_FPS_OVERLAY_LINGER_DURATION)
                                  / GDK_FPS_OVERLAY_FADE_DURATION;
        }
      else
        {
          overlay_opacity = 1.0;
        }
    }

  fps = gtk_fps_overlay_get_fps (widget);
  if (fps == 0.0)
    fps_string = g_strdup ("--- fps");
  else
    fps_string = g_strdup_printf ("%.2f fps", fps);

  if (GTK_IS_WINDOW (widget))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
      if (!child ||
          !gtk_widget_compute_bounds (child, widget, &bounds))
        has_bounds = gtk_widget_compute_bounds (widget, widget, &bounds);
    }
  else
    {
      has_bounds = gtk_widget_compute_bounds (widget, widget, &bounds);
    }

  layout = gtk_widget_create_pango_layout (widget, fps_string);
  pango_layout_get_pixel_size (layout, &width, &height);

  gtk_snapshot_save (snapshot);
  if (has_bounds)
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (bounds.origin.x + bounds.size.width - width, bounds.origin.y));
  if (overlay_opacity < 1.0)
    gtk_snapshot_push_opacity (snapshot, overlay_opacity);
  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA) { 0, 0, 0, 0.5 },
                             &GRAPHENE_RECT_INIT (-1, -1, width + 2, height + 2));
  gtk_snapshot_append_layout (snapshot,
                              layout,
                              &(GdkRGBA) { 1, 1, 1, 1 });
  if (overlay_opacity < 1.0)
    gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);
  g_free (fps_string);

  gtk_widget_add_tick_callback (widget, gtk_fps_overlay_force_redraw, NULL, NULL);
}

static void
gtk_fps_overlay_queue_draw (GtkInspectorOverlay *overlay)
{
  GtkFpsOverlay *self = GTK_FPS_OVERLAY (overlay);
  GHashTableIter iter;
  gpointer widget;

  g_hash_table_iter_init (&iter, self->infos);
  while (g_hash_table_iter_next (&iter, &widget, NULL))
    gdk_surface_queue_expose (gtk_native_get_surface (gtk_widget_get_native (widget)));
}

static void
gtk_fps_overlay_dispose (GObject *object)
{
  GtkFpsOverlay *self = GTK_FPS_OVERLAY (object);

  g_hash_table_unref (self->infos);

  G_OBJECT_CLASS (gtk_fps_overlay_parent_class)->dispose (object);
}

static void
gtk_fps_overlay_class_init (GtkFpsOverlayClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);

  overlay_class->snapshot = gtk_fps_overlay_snapshot;
  overlay_class->queue_draw = gtk_fps_overlay_queue_draw;

  gobject_class->dispose = gtk_fps_overlay_dispose;
}

static void
gtk_fps_overlay_init (GtkFpsOverlay *self)
{
  self->infos = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, gtk_fps_info_free);
}

GtkInspectorOverlay *
gtk_fps_overlay_new (void)
{
  GtkFpsOverlay *self;

  self = g_object_new (GTK_TYPE_FPS_OVERLAY, NULL);

  return GTK_INSPECTOR_OVERLAY (self);
}

