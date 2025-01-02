/* testpaintablecursor.c
 * Copyright (C) 2024 Red Hat, Inc.
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

static GdkTexture *
cursor_callback (GdkCursor *cursor,
                 int        cursor_size,
                 double     scale,
                 int       *width,
                 int       *height,
                 int       *hotspot_x,
                 int       *hotspot_y,
                 gpointer   data)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GError *error = NULL;
  int scaled_size;

  scaled_size = ceil (cursor_size * scale);

  g_print ("cursor size %d scale %g\n", cursor_size, scale);
  g_print ("resulting pixels %d x %d\n", scaled_size, scaled_size);

  pixbuf = gdk_pixbuf_new_from_file_at_size ((const char *)data,
                                             scaled_size, scaled_size,
                                             &error);
  if (!pixbuf)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  texture = gdk_texture_new_for_pixbuf (pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_object_unref (pixbuf);

  *width = cursor_size;
  *height = cursor_size;
  *hotspot_x = 0;
  *hotspot_y = 0;

  return texture;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button;
  gboolean done = FALSE;
  GdkCursor *cursor;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  button = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "hello world");
  gtk_widget_set_margin_top (button, 10);
  gtk_widget_set_margin_bottom (button, 10);
  gtk_widget_set_margin_start (button, 10);
  gtk_widget_set_margin_end (button, 10);

  cursor = gdk_cursor_new_from_callback (cursor_callback,
                                         (gpointer) "docs/reference/gsk/gtk-logo.svg", NULL,
                                         gdk_cursor_new_from_name ("default", NULL));

  gtk_widget_set_cursor (button, cursor);
  g_object_unref (cursor);

  gtk_window_set_child (GTK_WINDOW (window), button);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
