/* GTK - The GIMP Toolkit
 * testapplication.c: Using GtkApplication
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

static const char *builder_data =
"<interface>"
"<object class=\"GtkAboutDialog\" id=\"about_dialog\">"
"  <property name=\"program-name\">Test Application</property>"
"  <property name=\"website\">http://gtk.org</property>"
"</object>"
"<object class=\"GtkActionGroup\" id=\"actions\">"
"  <child>"
"      <object class=\"GtkAction\" id=\"About\">"
"          <property name=\"name\">About</property>"
"          <property name=\"stock_id\">gtk-about</property>"
"      </object>"
"  </child>"
"</object>"
"</interface>";

static GtkWidget *about_dialog;

static void
about_activate (GtkAction *action,
                gpointer   user_data)
{
  gtk_dialog_run (GTK_DIALOG (about_dialog));
  gtk_widget_hide (GTK_WIDGET (about_dialog));
}

static void
launch_myself (void)
{
  GAppInfo *ai;

  g_type_init ();

  ai = (GAppInfo*)g_desktop_app_info_new_from_filename ("./testapplication.desktop");
  g_app_info_launch (ai, NULL, NULL, NULL);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  GtkWindow *window;
  GtkWindow *window2;
  GtkBuilder *builder;
  GtkAction *action;
  GtkActionGroup *actions;

  if (argc > 1 && strcmp (argv[1], "--launch-yourself") == 0)
    {
      launch_myself ();
      exit (0);
    }

  app = gtk_application_new (&argc, &argv, "org.gtk.TestApp");
  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, builder_data, -1, NULL))
    g_error ("failed to parse UI");
  actions = GTK_ACTION_GROUP (gtk_builder_get_object (builder, "actions"));
  gtk_application_set_action_group (app, actions);

  action = gtk_action_group_get_action (actions, "About");
  g_signal_connect (action, "activate", G_CALLBACK (about_activate), app);

  about_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "about_dialog"));

  gtk_builder_connect_signals (builder, app);
  g_object_unref (builder);

  window = gtk_application_get_window (app);
  gtk_container_add (GTK_CONTAINER (window), gtk_label_new ("Hello world"));
  gtk_widget_show_all (GTK_WIDGET (window));

  window2 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window2), gtk_label_new ("Hello again"));
  gtk_widget_show_all (GTK_WIDGET (window2));
  gtk_application_add_window (app, window2);

  gtk_application_run (app);

  return 0;
}
