/* testimage.c
 * Copyright (C) 2004  Red Hat, Inc.
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
#include <gio/gio.h>

static void
drag_begin (GtkWidget      *widget,
	    GdkDrag        *drag,
	    gpointer        data)
{
  GtkWidget *image = GTK_WIDGET (data);
  GdkPaintable *paintable;

  paintable = gtk_image_get_paintable (GTK_IMAGE (image));
  gtk_drag_set_icon_paintable (drag, paintable, -2, -2);
}

void  
drag_data_get  (GtkWidget        *widget,
		GdkDrag          *drag,
		GtkSelectionData *selection_data,
		gpointer          data)
{
  GtkWidget *image = GTK_WIDGET (data);
  GdkPaintable *paintable;

  paintable = gtk_image_get_paintable (GTK_IMAGE (image));
  if (GDK_IS_TEXTURE (paintable))
    gtk_selection_data_set_texture (selection_data, GDK_TEXTURE (paintable));
}

static void
drag_data_received (GtkWidget        *widget,
		    GdkDrag          *drag,
		    GtkSelectionData *selection_data,
		    guint             info,
		    guint32           time,
		    gpointer          data)
{
  GtkWidget *image = GTK_WIDGET (data);
  GdkTexture *texture;

  if (gtk_selection_data_get_length (selection_data) < 0)
    return;

  texture = gtk_selection_data_get_texture (selection_data);
  gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (texture));

  g_object_unref (texture);
}

static gboolean
idle_func (gpointer data)
{
  g_print ("keep me busy\n");

  return G_SOURCE_CONTINUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *grid;
  GtkWidget *label, *image;
  GtkIconTheme *theme;
  GdkTexture *texture;
  gchar *icon_name = "help-browser";
  gchar *anim_filename = NULL;
  GtkIconInfo *icon_info;
  GIcon *icon;
  GFile *file;

  gtk_init ();

  if (argc > 1)
    icon_name = argv[1];

  if (argc > 2)
    anim_filename = argv[2];

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_container_add (GTK_CONTAINER (window), grid);

  label = gtk_label_new ("symbolic size");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  label = gtk_label_new ("fixed size");
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

  label = gtk_label_new ("GTK_IMAGE_PIXBUF");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  theme = gtk_icon_theme_get_default ();
  icon_info = gtk_icon_theme_lookup_icon_for_scale (theme, icon_name, 48, gtk_widget_get_scale_factor (window), GTK_ICON_LOOKUP_GENERIC_FALLBACK);
  texture = gtk_icon_info_load_texture (icon_info, NULL);
  g_object_unref (icon_info);
  image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));
  g_object_unref (texture);
  gtk_grid_attach (GTK_GRID (grid), image, 2, 1, 1, 1);

  gtk_drag_source_set (image, GDK_BUTTON1_MASK, 
		       NULL,
		       GDK_ACTION_COPY);
  gtk_drag_source_add_image_targets (image);
  g_signal_connect (image, "drag_begin", G_CALLBACK (drag_begin), image);
  g_signal_connect (image, "drag_data_get", G_CALLBACK (drag_data_get), image);

  gtk_drag_dest_set (image,
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     NULL, GDK_ACTION_COPY);
  gtk_drag_dest_add_image_targets (image);
  g_signal_connect (image, "drag_data_received",
		    G_CALLBACK (drag_data_received), image);

  label = gtk_label_new ("GTK_IMAGE_ICON_NAME");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 4, 1, 1);
  image = gtk_image_new_from_icon_name (icon_name);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 30);
  gtk_grid_attach (GTK_GRID (grid), image, 2, 4, 1, 1);

  label = gtk_label_new ("GTK_IMAGE_GICON");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 1, 1);
  icon = g_themed_icon_new_with_default_fallbacks ("folder-remote");
  image = gtk_image_new_from_gicon (icon);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  g_object_unref (icon);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 5, 1, 1);
  file = g_file_new_for_path ("apple-red.png");
  icon = g_file_icon_new (file);
  image = gtk_image_new_from_gicon (icon);
  gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
  g_object_unref (icon);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 30);
  gtk_grid_attach (GTK_GRID (grid), image, 2, 5, 1, 1);

  if (anim_filename)
    {
      label = gtk_label_new ("GTK_IMAGE_ANIMATION (from file)");
      gtk_grid_attach (GTK_GRID (grid), label, 0, 6, 1, 1);
      image = gtk_image_new_from_file (anim_filename);
      gtk_image_set_pixel_size (GTK_IMAGE (image), 30);
      gtk_grid_attach (GTK_GRID (grid), image, 2, 6, 1, 1);

      /* produce high load */
      g_idle_add_full (G_PRIORITY_DEFAULT,
                       idle_func, NULL, NULL);
    }

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
