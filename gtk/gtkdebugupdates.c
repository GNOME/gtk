/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkdebugupdatesprivate.h"

#include "gtksnapshot.h"

/* duration before we start fading in us */
#define GDK_DRAW_REGION_MIN_DURATION 50 * 1000
/* duration when fade is finished in us */
#define GDK_DRAW_REGION_MAX_DURATION 200 * 1000

typedef struct {
  gint64 timestamp;
  cairo_region_t *region;
} GtkDebugUpdate;

static gboolean _gtk_debug_updates_enabled = FALSE;

static GQuark _gtk_debug_updates_quark = 0;

static void
gtk_debug_updates_init (void)
{
  static gboolean inited = FALSE;

  if (G_LIKELY (inited))
    return;

  _gtk_debug_updates_quark = g_quark_from_static_string ("-gtk-debug-updates");

  inited = TRUE;
}

gboolean
gtk_debug_updates_get_enabled (void)
{
  return _gtk_debug_updates_enabled;
}

void
gtk_debug_updates_set_enabled (gboolean enabled)
{
  if (enabled)
    gtk_debug_updates_init ();

  _gtk_debug_updates_enabled = enabled;
}

gboolean
gtk_debug_updates_get_enabled_for_display (GdkDisplay *display)
{
  if (gtk_debug_updates_get_enabled ())
    return TRUE;

  if (_gtk_debug_updates_quark == 0)
    return FALSE;

  return g_object_get_qdata (G_OBJECT (display), _gtk_debug_updates_quark) != NULL;
}

void
gtk_debug_updates_set_enabled_for_display (GdkDisplay *display,
                                           gboolean    enabled)
{
  if (enabled)
    {
      gtk_debug_updates_init ();

      g_object_set_qdata (G_OBJECT (display), _gtk_debug_updates_quark, GINT_TO_POINTER (TRUE));
    }
  else
    {
      if (_gtk_debug_updates_quark != 0)
        g_object_steal_qdata (G_OBJECT (display), _gtk_debug_updates_quark);
    }
}

static void
gtk_debug_update_free (gpointer data)
{
  GtkDebugUpdate *region = data;

  cairo_region_destroy (region->region);
  g_slice_free (GtkDebugUpdate, region);
}

static void
gtk_debug_update_free_queue (gpointer data)
{
  GQueue *queue = data;

  g_queue_free_full (queue, gtk_debug_update_free);
}

static void
gtk_debug_updates_print (GQueue               *queue,
                         const cairo_region_t *region,
                         const char           *format,
                         ...) G_GNUC_PRINTF(3,4);
static void
gtk_debug_updates_print (GQueue               *queue,
                         const cairo_region_t *region,
                         const char           *format,
                         ...)
{
#if 0
  GString *string = g_string_new (NULL);
  va_list args;

  g_string_append_printf (string, "%3u: ", g_queue_get_length (queue));

  if (region)
    {
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (region, &extents);
      g_string_append_printf (string, "{%u,%u,%u,%u}(%u) ",
                              extents.x, extents.y,
                              extents.width, extents.height,
                              cairo_region_num_rectangles (region));
    }

  va_start (args, format);
  g_string_append_vprintf (string, format, args);
  va_end (args);

  g_print ("%s\n", string->str);

  g_string_free (string, TRUE);
#endif
}

static gboolean
gtk_window_manage_updates (GtkWidget     *widget,
                           GdkFrameClock *frame_clock,
                           gpointer       user_data)
{
  GQueue *updates;
  GtkDebugUpdate *draw;
  cairo_region_t *region;
  gint64 timestamp;
  GList *l;

  updates = g_object_get_qdata (G_OBJECT (widget), _gtk_debug_updates_quark);
  if (updates == NULL)
    return FALSE;
  timestamp = gdk_frame_clock_get_frame_time (frame_clock);

  gtk_debug_updates_print (updates, NULL, "Managing updates");
  /* First queue an update for all regions.
   * While doing so, set the correct timestamp on all untimed regions */
  region = cairo_region_create ();
  for (l = g_queue_peek_head_link (updates); l != NULL; l = l->next)
    {
      draw = l->data;

      if (draw->timestamp == 0)
        {
          draw->timestamp = timestamp;
          gtk_debug_updates_print (updates, draw->region, "Setting timestamp to %lli\n", (long long) timestamp);
        }

      cairo_region_union (region, draw->region);
    }
  gtk_debug_updates_print (updates, region, "Queued update");
  gdk_window_invalidate_region (gtk_widget_get_window (widget), region, TRUE);
  cairo_region_destroy (region);

  /* Then remove all outdated regions */
  do {
    draw = g_queue_peek_tail (updates);

    if (draw == NULL)
      {
        gtk_debug_updates_print (updates, NULL, "Empty, no more updates");
        g_object_set_qdata (G_OBJECT (widget), _gtk_debug_updates_quark, NULL);
        return FALSE;
      }

    if (draw->timestamp + GDK_DRAW_REGION_MAX_DURATION >= timestamp)
      break;

    gtk_debug_updates_print (updates, draw->region, "Popped region");
    draw = g_queue_pop_tail (updates);
    gtk_debug_update_free (draw);
  } while (TRUE);

  return TRUE;
}

void
gtk_debug_updates_add (GtkWidget              *widget,
                       const cairo_region_t   *region)
{
  GQueue *updates;
  GtkDebugUpdate *draw;

  if (!gtk_debug_updates_get_enabled_for_display (gtk_widget_get_display (widget)))
    return;

  updates = g_object_get_qdata (G_OBJECT (widget), _gtk_debug_updates_quark);
  if (updates == NULL)
    {
      updates = g_queue_new ();
      gtk_widget_add_tick_callback (widget,
                                    gtk_window_manage_updates,
                                    NULL,
                                    NULL);
      g_object_set_qdata_full (G_OBJECT (widget),
                               _gtk_debug_updates_quark,
                               updates,
                               gtk_debug_update_free_queue);
      gtk_debug_updates_print (updates, NULL, "Newly created");
    }
  else
    {
      GtkDebugUpdate *first = g_queue_peek_head (updates);

      if (first->timestamp == 0)
        {
          cairo_region_union (first->region, region);
          gtk_debug_updates_print (updates, first->region, "Added to existing region");
          return;
        }
    }

  draw = g_slice_new0 (GtkDebugUpdate);
  draw->region = cairo_region_copy (region);
  g_queue_push_head (updates, draw);
  gtk_debug_updates_print (updates, draw->region, "Added new region");
}

void
gtk_debug_updates_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
  GQueue *updates;
  GtkDebugUpdate *draw;
  GdkRectangle rect;
  gint64 timestamp;
  double progress;
  GList *l;
  guint i;

  if (!gtk_debug_updates_get_enabled_for_display (gtk_widget_get_display (widget)))
    return;

  updates = g_object_get_qdata (G_OBJECT (widget), _gtk_debug_updates_quark);
  if (updates == NULL)
    return;
  timestamp = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
  
  gtk_debug_updates_print (updates, NULL, "Painting at %lli", (long long) timestamp);

  for (l = g_queue_peek_head_link (updates); l != NULL; l = l->next)
    {
      draw = l->data;
      if (timestamp - draw->timestamp < GDK_DRAW_REGION_MIN_DURATION)
        progress = 0.0;
      else if (timestamp - draw->timestamp < GDK_DRAW_REGION_MAX_DURATION)
        progress = (double) (timestamp - draw->timestamp - GDK_DRAW_REGION_MIN_DURATION)
                   / (GDK_DRAW_REGION_MAX_DURATION - GDK_DRAW_REGION_MIN_DURATION);
      else
        continue;

      gtk_debug_updates_print (updates, draw->region, "Painting with progress %g", progress);
      for (i = 0; i < cairo_region_num_rectangles (draw->region); i++)
        {
          cairo_region_get_rectangle (draw->region, i, &rect);
          gtk_snapshot_append_color (snapshot,
                                     &(GdkRGBA) { 1, 0, 0, 0.4 * (1 - progress) },
                                     &GRAPHENE_RECT_INIT(rect.x, rect.y, rect.width, rect.height),
                                     "Debug Updates<%g>", progress);
        }
    }
}

