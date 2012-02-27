/* visuals: UI runner for visual GtkBuilder files
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License  along  with  this library;  if not,  write to  the Free
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <gtk/gtk.h>

static void
dark_button_toggled_cb (GtkToggleButton *button,
                        gpointer user_data)
{
  gboolean active = gtk_toggle_button_get_active (button);
  GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (settings,
                "gtk-application-prefer-dark-theme", active,
                NULL);
}

static void
create_dark_popup (GtkWidget *parent)
{
  GtkWidget *popup = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *button = gtk_toggle_button_new_with_label ("Dark");

  gtk_window_set_decorated (GTK_WINDOW (popup), FALSE);
  gtk_widget_set_size_request (popup, 100, 100);
  gtk_window_set_resizable (GTK_WINDOW (popup), FALSE);

  g_signal_connect (popup, "delete-event",
                    G_CALLBACK (gtk_true), NULL);

  gtk_container_add (GTK_CONTAINER (popup), button);
  g_signal_connect (button, "toggled",
                    G_CALLBACK (dark_button_toggled_cb), NULL);

  gtk_window_set_transient_for (GTK_WINDOW (popup), GTK_WINDOW (parent));

  gtk_widget_show_all (popup);
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget  *window;
  const gchar *filename;

  gtk_init (&argc, &argv);

  if (argc < 2)
    return 1;
  filename = argv[1];

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, filename, NULL);

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
  g_object_unref (G_OBJECT (builder));
  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);

  create_dark_popup (window);
  gtk_main ();

  return 0;
}
