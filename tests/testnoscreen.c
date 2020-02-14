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
  gboolean has_display;

  gdk_set_allowed_backends ("x11");
  g_unsetenv ("DISPLAY");
  has_display = gtk_init_check ();
  g_assert (!has_display);

  window = gtk_window_new ();
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (window), button);

  display = gdk_display_open (NULL);

  gtk_window_set_display (GTK_WINDOW (window), display);

  gtk_widget_show (window);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
