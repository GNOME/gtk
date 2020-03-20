/* testdivershortcuts.c

   Copyright (C) 2017 Red Hat
   Author: Olivier Fourdan <ofourdan@redhat.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

static void
on_button_toggle (GtkWidget *button, gpointer data)
{
  GdkSurface *surface;
  GdkSeat *seat;
  gboolean divert;

  surface = gtk_native_get_surface (gtk_widget_get_native (button));
  seat = gdk_display_get_default_seat (gdk_surface_get_display (surface));
  divert = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gdk_toplevel_divert_system_shortcuts (GDK_TOPLEVEL (surface), seat, divert);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *entry;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (vbox), entry);

  button = gtk_toggle_button_new_with_label ("Divert system keyboard shorcuts");
  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (on_button_toggle), NULL);

  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
