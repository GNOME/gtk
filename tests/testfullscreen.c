/* testfullscreen.c
 * Copyright (C) 2013 Red Hat
 * Author: Olivier Fourdan <ofourdan@redhat.com>
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

#include "config.h"
#include <gtk/gtk.h>

static void
set_fullscreen_monitor_cb (GtkWidget *widget, gpointer user_data)
{
  GdkFullscreenMode mode = (GdkFullscreenMode) GPOINTER_TO_INT (user_data);
  GdkWindow  *window;

  window = gtk_widget_get_parent_window (widget);
  gdk_window_set_fullscreen_mode (window, mode);
  gdk_window_fullscreen (window);
}

static void
remove_fullscreen_cb (GtkWidget *widget, gpointer user_data)
{
  GdkWindow  *window;

  window = gtk_widget_get_parent_window (widget);
  gdk_window_unfullscreen (window);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *vbox, *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
  gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  button = gtk_button_new_with_label ("Fullscreen on current monitor");
  g_signal_connect (button, "clicked", G_CALLBACK (set_fullscreen_monitor_cb), GINT_TO_POINTER (GDK_FULLSCREEN_ON_CURRENT_MONITOR));
  gtk_container_add (GTK_CONTAINER (vbox), button);

  button = gtk_button_new_with_label ("Fullscreen on all monitors");
  g_signal_connect (button, "clicked", G_CALLBACK (set_fullscreen_monitor_cb), GINT_TO_POINTER (GDK_FULLSCREEN_ON_ALL_MONITORS));
  gtk_container_add (GTK_CONTAINER (vbox), button);

  button = gtk_button_new_with_label ("Un-fullscreen");
  g_signal_connect (button, "clicked", G_CALLBACK (remove_fullscreen_cb), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
