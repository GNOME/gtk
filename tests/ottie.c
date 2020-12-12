/*
 * Copyright © 2020 Benjamin Otte
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

#include <ottie/ottie.h>
#include <gtk/gtk.h>

G_GNUC_UNUSED static gboolean
save_paintable (GdkPaintable *paintable,
                const char  *filename)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  int width, height;
  cairo_t *cr;
  cairo_surface_t *surface;
  gboolean result;

  width = gdk_paintable_get_intrinsic_width (paintable);
  height = gdk_paintable_get_intrinsic_height (paintable);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  node = gtk_snapshot_free_to_node (snapshot);
  if (!gsk_render_node_write_to_file (node, "foo.node", NULL))
    return FALSE;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);
  gsk_render_node_unref (node);

  result = cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS;

  cairo_surface_destroy (surface);

  return result;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *video;
  OttiePlayer *player;

  gtk_init ();

  if (argc > 1)
    player = ottie_player_new_for_filename (argv[1]);
  else
    player = ottie_player_new ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Ottie");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_window_destroy), NULL);

  video = gtk_video_new ();
  gtk_video_set_loop (GTK_VIDEO (video), TRUE);
  gtk_video_set_autoplay (GTK_VIDEO (video), TRUE);
  gtk_video_set_media_stream (GTK_VIDEO (video), GTK_MEDIA_STREAM (player));
  gtk_window_set_child (GTK_WINDOW (window), video);

  gtk_widget_show (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

#if 0
  for (int i = 0; i < 62; i++)
    {
      ottie_paintable_set_timestamp (paintable, i * G_USEC_PER_SEC / 30);
      save_paintable (GDK_PAINTABLE (paintable), g_strdup_printf ("foo%u.png", i));
    }
#else
  //save_paintable (GDK_PAINTABLE (paintable), "foo.png");
#endif

  return 0;
}
