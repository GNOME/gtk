/* Copyright (C) 2025  Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include "svgpaintable.h"


/* Compare librsvg and SVG renderer rendering of a directory full of SVGs
 */

int
main (int argc, char *argv[])
{
  GtkWidget *window, *sw, *grid;
  GError *error = NULL;
  GFile *file;
  GFileEnumerator *dir;
  int row;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  if (argc < 2)
    g_error ("usage: svg-compare DIRECTORY");

  sw = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), sw);
  grid = gtk_grid_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), grid);

  file = g_file_new_for_commandline_arg (argv[1]);
  dir = g_file_enumerate_children (file,
                                   "standard::name",
                                   G_FILE_QUERY_INFO_NONE,
                                   NULL,
                                   &error);
  if (!dir)
    g_error ("%s", error->message);

  row = 0;
  while (1)
    {
      GFile *child;
      const char *path;
      char *basename;
      GdkPaintable *svg;
      GBytes *bytes;
      GtkWidget *label;
      GtkWidget *img;

      if (!g_file_enumerator_iterate (dir, NULL, &child, NULL, &error))
        g_error ("%s", error->message);

      if (!child)
        break;

      path = g_file_peek_path (child);

      if (!g_str_has_suffix (path, ".svg") ||
          g_str_has_suffix (path, ".ref.svg"))
        continue;

      basename = g_file_get_basename (child);

      label = gtk_label_new (basename);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);

      svg = GDK_PAINTABLE (svg_paintable_new (child));
      if (svg)
        {
          img = gtk_picture_new_for_paintable (svg);
          gtk_picture_set_can_shrink (GTK_PICTURE (img), FALSE);
          gtk_grid_attach (GTK_GRID (grid), img, 1, row, 1, 1);
          g_object_unref (svg);
        }

      bytes = g_file_load_bytes (child, NULL, NULL, NULL);
      svg = GDK_PAINTABLE (gtk_svg_new_from_bytes (bytes));
      if (svg)
        {
          img = gtk_picture_new_for_paintable (svg);
          gtk_picture_set_can_shrink (GTK_PICTURE (img), FALSE);
          gtk_grid_attach (GTK_GRID (grid), img, 2, row, 1, 1);
          g_object_unref (svg);
          g_bytes_unref (bytes);
        }
      else
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
      row++;

      g_free (basename);
    }

  g_object_unref (dir);
  g_object_unref (file);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
