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

#define DEFAULT_CONFIG "#ffffff33 end throttle, " \
                       "#fefefe start end, " \
                       "#1a5fb466 start events, " \
                       "#62a0ea66 before-paint update, " \
                       "#9141ac66 update layout, " \
                       "#2ec27e66 layout paint, " \
                       "#d11aff predicted, " \
                       "#1a1aff presentation, " \
                       "#1aff1a frame"

typedef struct _Time Time;

struct _Time {
  GdkRGBA color;
  uint64_t (* get_start_time) (GdkFrameTimings *timings);
  uint64_t (* get_end_time) (GdkFrameTimings *timings);
};

struct _GtkFrameTimeOverlay
{
  GtkInspectorOverlay parent_instance;

  char *config;
  Time *times;
  gsize n_times;
};

struct _GtkFrameTimeOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

enum {
  PROP_CONFIG = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkFrameTimeOverlay, gtk_frame_time_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

#define STAGE_FUNC(name, stage) \
static uint64_t \
frame_timings_get_ ## name ## _end (GdkFrameTimings *timings) \
{ \
  return gdk_frame_timings_get_end_time (timings, stage); \
}\

STAGE_FUNC (none, GDK_FRAME_STAGE_NONE)
STAGE_FUNC (flush_events, GDK_FRAME_STAGE_FLUSH_EVENTS)
STAGE_FUNC (before_paint, GDK_FRAME_STAGE_BEFORE_PAINT)
STAGE_FUNC (update, GDK_FRAME_STAGE_UPDATE)
STAGE_FUNC (layout, GDK_FRAME_STAGE_LAYOUT)
STAGE_FUNC (paint, GDK_FRAME_STAGE_PAINT)
STAGE_FUNC (after_paint, GDK_FRAME_STAGE_AFTER_PAINT)
STAGE_FUNC (resume_events, GDK_FRAME_STAGE_RESUME_EVENTS)

typedef uint64_t (* GetTimeFunc) (GdkFrameTimings *timings);

static const struct {
  const char *name;
  GetTimeFunc get_time;
} definitions[] = {
  { "frame", gdk_frame_timings_get_frame_time_ns },
  { "presentation", gdk_frame_timings_get_presentation_time_ns },
  { "predicted", gdk_frame_timings_get_predicted_presentation_time_ns },
  { "start", frame_timings_get_none_end },
  { "events", frame_timings_get_flush_events_end },
  { "before-paint", frame_timings_get_before_paint_end },
  { "update", frame_timings_get_update_end },
  { "layout", frame_timings_get_layout_end },
  { "paint", frame_timings_get_paint_end },
  { "after-paint", frame_timings_get_after_paint_end },
  { "end", frame_timings_get_resume_events_end },
  { "throttle", gdk_frame_timings_get_throttling_hint },
};

static GetTimeFunc
parse_definition (const char *name)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (definitions); i++)
    {
      if (g_ascii_strcasecmp (definitions[i].name, name) == 0)
        return definitions[i].get_time;
    }

  return NULL;
}

static Time *
parse_config (const char *definition,
              gsize      *out_n_times)
{
  char **split;
  Time *times;
  gsize i, j, n;

  split = g_strsplit_set (definition, ",", -1);
  if (split == NULL)
    goto error;

  times = g_new0 (Time, g_strv_length (split));
  for (n = 0, i = 0; split[i] != NULL; i++)
    {
      char **tokens = g_strsplit_set (split[i], " \t\n\r", -1);
      const char *color = NULL, *def_start = NULL, *def_end = NULL;

      for (j = 0; tokens[j] != NULL; j++)
        {
          if (tokens[j][0] == '\0')
            continue;
          else if (color == NULL)
            color = tokens[j];
          else if (def_start == NULL)
            def_start = tokens[j];
          else if (def_end == NULL)
            def_end = tokens[j];
          else
            {
              g_strfreev (tokens);
              goto error;
            }
        }

      if (color == NULL || def_start == NULL ||
          !gdk_rgba_parse (&times[n].color, color) ||
          !(times[n].get_start_time = parse_definition (def_start)) ||
          (def_end != NULL && !(times[n].get_end_time = parse_definition (def_end)))) 
        {
          g_strfreev (tokens);
          goto error;
        }

      g_strfreev (tokens);
      n++;
    }

  g_clear_pointer (&split, g_strfreev);
  *out_n_times = n;
  return times;

error:
  g_clear_pointer (&split, g_strfreev);
  *out_n_times = 0;
  return NULL;
}


static gsize
get_point (uint64_t t,
           uint64_t max_time,
           uint64_t ns_scale)
{
  t = max_time - t;
  return (t + ns_scale - 1) / ns_scale;
}

static guint32
gdk_cairo_composite_over (guint32 dest,
                          guint32 src)
{
  guint8 a, r, g, b;
  guint alpha;

  a = dest >> 24;
  r = dest >> 16;
  g = dest >>  8;
  b = dest >>  0;

  alpha = 255 - (src >> 24);

  a = a * alpha / 255 + (src >> 24);
  r = r * alpha / 255 + (src >> 16);
  g = g * alpha / 255 + (src >>  8);
  b = b * alpha / 255 + (src >>  0);

  return (a << 24) | (r << 16) | (g << 8) | (b << 0);
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

  data[t] = gdk_cairo_composite_over (data[t], color);
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
    data[i] = gdk_cairo_composite_over (data[i], color);

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
free_timings_queue (gpointer data)
{
  g_queue_free_full (data, (GDestroyNotify) gdk_frame_timings_unref);
}

static void
gtk_timings_queue_drop_unused (GQueue *queue,
                               gint    min_used)
{
  GdkFrameTimings *timings;

  while ((timings = g_queue_peek_tail (queue)) &&
         gdk_frame_timings_get_frame_counter (timings) < min_used)
    {
      g_queue_pop_tail (queue);
      gdk_frame_timings_unref (timings);
    }
}

static GQueue *
gtk_frame_time_overlay_get_timings_queue (GtkWidget *widget)
{
  GQueue *result = g_object_get_data (G_OBJECT (widget), "-gtk-inspector-timing-queue");
  GdkFrameClock *clock;
  GdkFrameTimings *newest;
  gint64 i, start;

  if (result == NULL)
    {
      result = g_queue_new ();
      g_object_set_data_full (G_OBJECT (widget),
                              "-gtk-inspector-timing-queue",
                              result,
                              free_timings_queue);
    }

  clock = gtk_widget_get_frame_clock (widget);
  newest = g_queue_peek_head (result);
  if (newest)
    start = gdk_frame_timings_get_frame_counter (newest) + 1;
  else
    start = 0;
  if (gdk_frame_clock_get_history_start (clock) > start)
    start = gdk_frame_clock_get_history_start (clock);

  for (i = start; i <= gdk_frame_clock_get_frame_counter (clock); i++)
    {
      GdkFrameTimings *timings = gdk_frame_clock_get_timings (clock, i);
      g_assert (timings);
      g_queue_push_head (result, gdk_frame_timings_ref (timings));
    }

  return result;
}

static void
gtk_frame_time_overlay_snapshot (GtkInspectorOverlay *overlay,
                                 GtkSnapshot         *snapshot,
                                 GskRenderNode       *node,
                                 GtkWidget           *widget)
{
  GtkFrameTimeOverlay *self = GTK_FRAME_TIME_OVERLAY (overlay);
  guint32 *data;
  gsize width, height, stride, size;
  GList *l;
  gint64 min_frame_counter;
  GdkFrameClock *clock;
  GdkFrameTimings *timings;
  GQueue *queue;
  graphene_rect_t bounds;
  gboolean has_bounds;
  GBytes *bytes;
  GdkTexture *texture;
  uint64_t max_time, nspp; /* nanoseconds per pixel */
  float scale;
  gsize j;

  scale = gdk_surface_get_scale (gtk_native_get_surface (gtk_widget_get_native (widget)));
  clock = gtk_widget_get_frame_clock (widget);
  queue = gtk_frame_time_overlay_get_timings_queue (widget);

  width = ceil (75 * scale);
  gtk_frame_time_overlay_get_timeline_values (clock, width, &max_time, &nspp);

  height = ceil (gtk_widget_get_width (widget) * scale);
  /* limit to 10s for performance reasons */
  height = MIN (height, 10 * G_NSEC_PER_SEC / (nspp * width) + 1);

  size = width * height;
  stride = sizeof (guint32) * width;
  data = g_malloc0_n (stride, height);


  min_frame_counter = G_MAXINT64;
  for (j = 0; j < self->n_times; j++)
    {
      timings = NULL;
      for (l = g_queue_peek_head_link (queue); l; l = l->next)
        {
          uint64_t st, et;

          timings = l->data;

          st = self->times[j].get_start_time (timings);
          st = MIN (st, max_time);
          if (self->times[j].get_end_time)
            {
              /* a range */
              et = self->times[j].get_end_time (timings);
              et = MIN (et, max_time);
              if (et > st)
                if (!draw_line (data, size, rgba_to_color (&self->times[j].color), st, et, max_time, nspp))
                  break;
            }
          else
            {
              /* a point */
              if (!draw_point (data, size, rgba_to_color (&self->times[j].color), st, max_time, nspp))
                break;
            }
        }

      if (timings)
        min_frame_counter = MIN (min_frame_counter, gdk_frame_timings_get_frame_counter (timings));
    }

  gtk_timings_queue_drop_unused (queue, min_frame_counter);

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
gtk_frame_time_overlay_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkFrameTimeOverlay *self = GTK_FRAME_TIME_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      gtk_frame_time_overlay_set_config (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_frame_time_overlay_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkFrameTimeOverlay *self = GTK_FRAME_TIME_OVERLAY (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_string (value, self->config);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_frame_time_overlay_finalize (GObject *object)
{
  GtkFrameTimeOverlay *self = GTK_FRAME_TIME_OVERLAY (object);

  g_free (self->config);
  g_free (self->times);

  G_OBJECT_CLASS (gtk_frame_time_overlay_parent_class)->finalize (object);
}

static void
gtk_frame_time_overlay_class_init (GtkFrameTimeOverlayClass *klass)
{
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  overlay_class->snapshot = gtk_frame_time_overlay_snapshot;

  object_class->set_property = gtk_frame_time_overlay_set_property;
  object_class->get_property = gtk_frame_time_overlay_get_property;
  object_class->finalize = gtk_frame_time_overlay_finalize;

  properties[PROP_CONFIG] =
      g_param_spec_string ("config", NULL, NULL,
                           DEFAULT_CONFIG,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_frame_time_overlay_init (GtkFrameTimeOverlay *self)
{
  self->config = g_strdup ("");
}

GtkInspectorOverlay *
gtk_frame_time_overlay_new (void)
{
  GtkFrameTimeOverlay *self;

  self = g_object_new (GTK_TYPE_FRAME_TIME_OVERLAY, NULL);

  return GTK_INSPECTOR_OVERLAY (self);
}

void
gtk_frame_time_overlay_set_config (GtkFrameTimeOverlay *self,
                                   const char          *config)
{
  Time *times;
  gsize n_times;

  if (config == NULL ||
      g_str_equal (self->config, config))
    return;

  times = parse_config (config, &n_times);
  if (times == NULL)
    return;

  g_free (self->config);
  self->config = g_strdup (config);
  g_free (self->times);
  self->times = times;
  self->n_times = n_times;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONFIG]);
}

const char *
gtk_frame_time_overlay_get_config (GtkFrameTimeOverlay *self)
{
  return self->config;
}

