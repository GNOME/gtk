/* testinhibitshortcuts.c

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
on_inhibit_list_change (GdkSurface *surface, GParamSpec *pspec, gpointer data)
{
  GtkWidget *button = GTK_WIDGET (data);
  GList *inhibit_seat_list;
  GdkSeat *gdk_seat;
  gboolean button_active;
  gboolean shortcuts_inhibited;

  g_object_get (GDK_TOPLEVEL (surface), "inhibit-list", &inhibit_seat_list, NULL);
  gdk_seat = gdk_display_get_default_seat (gdk_surface_get_display (surface));
  shortcuts_inhibited = (g_list_find (inhibit_seat_list, gdk_seat) != NULL);

  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), FALSE);
  gtk_widget_set_sensitive (button, TRUE);

  button_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (button_active != shortcuts_inhibited)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), shortcuts_inhibited);
}

static void
on_button_toggle (GtkWidget *button, gpointer data)
{
  GdkSurface *surface = GDK_SURFACE (data);
  GdkSeat *gdk_seat;
  gboolean inhibit;

  gdk_seat = gdk_display_get_default_seat (gdk_surface_get_display (surface));
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      gdk_toplevel_restore_system_shortcuts (GDK_TOPLEVEL (surface), gdk_seat);
      return;
    }

  /* Requested shortcuts to be inhibited, set the button inconsistent until
   * we get confirmation...
   */
  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), TRUE);

  inhibit =
    gdk_toplevel_inhibit_system_shortcuts (GDK_TOPLEVEL (surface), gdk_seat, NULL);

  gtk_widget_set_sensitive (button, inhibit);
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
  GdkSurface *surface;
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *vbox;
  GtkWidget *entry;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  gtk_widget_realize (window);
  surface = gtk_native_get_surface (gtk_widget_get_native (window));

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (vbox), entry);

  button = gtk_check_button_new_with_label ("Inhibit system keyboard shorcuts");

  gtk_container_add (GTK_CONTAINER (vbox), button);
  g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (on_button_toggle), surface);

  g_signal_connect (G_OBJECT (surface), "notify::inhibit-list",
                    G_CALLBACK (on_inhibit_list_change), button);

  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
