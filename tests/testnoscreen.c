/* testnoscreen.c
 * Copyright (C) 2011 Red Hat, Inc.
 * Authors: Matthias Clasen
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

/* Very limited test to ensure that creating widgets works
 * before opening a display connection
 */
int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *button;
  GdkDisplay *display;

  gtk_parse_args (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (window), button);

  display = gdk_display_open (NULL);

  gtk_window_set_screen (GTK_WINDOW (window), gdk_display_get_default_screen (display));

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
