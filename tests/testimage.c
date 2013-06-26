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
	    GdkDragContext *context,
	    gpointer        data)
{
  GtkWidget *image = GTK_WIDGET (data);

  GdkPixbuf *pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (image));

  gtk_drag_set_icon_pixbuf (context, pixbuf, -2, -2);
}

void  
drag_data_get  (GtkWidget        *widget,
		GdkDragContext   *context,
		GtkSelectionData *selection_data,
		guint             info,
		guint             time,
		gpointer          data)
{
  GtkWidget *image = GTK_WIDGET (data);

  GdkPixbuf *pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (image));

  gtk_selection_data_set_pixbuf (selection_data, pixbuf);
}

static void
drag_data_received (GtkWidget        *widget,
		    GdkDragContext   *context,
		    gint              x,
		    gint              y,
		    GtkSelectionData *selection_data,
		    guint             info,
		    guint32           time,
		    gpointer          data)
{
  GtkWidget *image = GTK_WIDGET (data);

  GdkPixbuf *pixbuf;

  if (gtk_selection_data_get_length (selection_data) < 0)
    return;

  pixbuf = gtk_selection_data_get_pixbuf (selection_data);

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
}

static gboolean
idle_func (gpointer data)
{
  g_print ("keep me busy\n");

  return G_SOURCE_CONTINUE;
}

static gboolean
anim_image_draw (GtkWidget *widget,
                 cairo_t   *cr,
                 gpointer   data)
{
  g_print ("start busyness\n");

  g_signal_handlers_disconnect_by_func (widget, anim_image_draw, data);

  /* produce high load */
  g_idle_add_full (G_PRIORITY_DEFAULT,
                   idle_func, NULL, NULL);

  return FALSE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *grid;
  GtkWidget *label, *image, *box;
  GtkIconTheme *theme;
  GdkPixbuf *pixbuf;
  GtkIconSet *iconset;
  GtkIconSource *iconsource;
  gchar *icon_name = "gnome-terminal";
  gchar *anim_filename = NULL;
  GIcon *icon;
  GFile *file;

  gtk_init (&argc, &argv);

  if (argc > 1)
    icon_name = argv[1];

  if (argc > 2)
    anim_filename = argv[2];

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  label = gtk_label_new ("symbolic size");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  label = gtk_label_new ("fixed size");
  gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

  label = gtk_label_new ("GTK_IMAGE_PIXBUF");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  theme = gtk_icon_theme_get_default ();
  pixbuf = gtk_icon_theme_load_icon (theme, icon_name, 48, 0, NULL);
  image = gtk_image_new_from_pixbuf (pixbuf);
  box = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (box), image);
  gtk_grid_attach (GTK_GRID (grid), box, 2, 1, 1, 1);

  gtk_drag_source_set (box, GDK_BUTTON1_MASK, 
		       NULL, 0,
		       GDK_ACTION_COPY);
  gtk_drag_source_add_image_targets (box);
  g_signal_connect (box, "drag_begin", G_CALLBACK (drag_begin), image);
  g_signal_connect (box, "drag_data_get", G_CALLBACK (drag_data_get), image);

  gtk_drag_dest_set (box,
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_DROP,
                     NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_image_targets (box);
  g_signal_connect (box, "drag_data_received", 
		    G_CALLBACK (drag_data_received), image);

  label = gtk_label_new ("GTK_IMAGE_STOCK");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  image = gtk_image_new_from_stock (GTK_STOCK_REDO, GTK_ICON_SIZE_DIALOG);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 2, 1, 1);

  label = gtk_label_new ("GTK_IMAGE_ICON_SET");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

  iconsource = gtk_icon_source_new ();
  gtk_icon_source_set_icon_name (iconsource, icon_name);
  iconset = gtk_icon_set_new ();
  gtk_icon_set_add_source (iconset, iconsource);
  image = gtk_image_new_from_icon_set (iconset, GTK_ICON_SIZE_DIALOG);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 3, 1, 1);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  label = gtk_label_new ("GTK_IMAGE_ICON_NAME");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 4, 1, 1);
  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 30);
  gtk_grid_attach (GTK_GRID (grid), image, 2, 4, 1, 1);

  label = gtk_label_new ("GTK_IMAGE_GICON");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 1, 1);
  icon = g_themed_icon_new_with_default_fallbacks ("folder-remote");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  g_object_unref (icon);
  gtk_grid_attach (GTK_GRID (grid), image, 1, 5, 1, 1);
  file = g_file_new_for_path ("apple-red.png");
  icon = g_file_icon_new (file);
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
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
      g_signal_connect_after (image, "draw",
                              G_CALLBACK (anim_image_draw),
                              NULL);
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
