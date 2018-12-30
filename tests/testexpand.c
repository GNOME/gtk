/* testexpand.c
 * Copyright (C) 2010 Havoc Pennington
 *
 * Author:
 *      Havoc Pennington <hp@pobox.com>
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

static void
on_toggle_hexpand (GtkToggleButton *toggle,
                   void            *data)
{
  g_object_set (toggle,
                "hexpand", gtk_toggle_button_get_active (toggle),
                NULL);
}

static void
on_toggle_vexpand (GtkToggleButton *toggle,
                   void            *data)
{
  g_object_set (toggle,
                "vexpand", gtk_toggle_button_get_active (toggle),
                NULL);
}

static void
create_box_window (void)
{
  GtkWidget *window;
  GtkWidget *box1, *box2, *box3;
  GtkWidget *toggle;
  GtkWidget *colorbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Boxes");

  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (box1),
                      gtk_label_new ("VBox 1 Top"));
  gtk_container_add (GTK_CONTAINER (box1),
                      box2);
  gtk_container_add (GTK_CONTAINER(box1),
                     gtk_label_new ("VBox 1 Bottom"));

  gtk_container_add (GTK_CONTAINER (box2),
                      gtk_label_new ("HBox 2 Left"));
  gtk_container_add (GTK_CONTAINER (box2),
                      box3);
  gtk_container_add (GTK_CONTAINER(box2),
                     gtk_label_new ("HBox 2 Right"));

  gtk_container_add (GTK_CONTAINER (box3),
                      gtk_label_new ("VBox 3 Top"));

  colorbox = gtk_frame_new (NULL);

  toggle = gtk_toggle_button_new_with_label ("H Expand");
  gtk_widget_set_halign (toggle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  g_object_set (toggle, "margin", 5, NULL);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_hexpand), NULL);
  gtk_container_add (GTK_CONTAINER (colorbox), toggle);

  gtk_container_add (GTK_CONTAINER (box3), colorbox);

  colorbox = gtk_frame_new (NULL);

  toggle = gtk_toggle_button_new_with_label ("V Expand");
  gtk_widget_set_halign (toggle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  g_object_set (toggle, "margin", 5, NULL);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_vexpand), NULL);
  gtk_container_add (GTK_CONTAINER (colorbox), toggle);
  gtk_container_add (GTK_CONTAINER (box3), colorbox);
  gtk_container_add (GTK_CONTAINER (box3),
                     gtk_label_new ("VBox 3 Bottom"));

  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show (window);
}

static void
create_grid_window (void)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *toggle;
  GtkWidget *colorbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Grid");

  grid = gtk_grid_new ();

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Top"), 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Bottom"), 1, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Left"), 0, 1, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Right"), 2, 1, 1, 2);

  colorbox = gtk_frame_new (NULL);

  toggle = gtk_toggle_button_new_with_label ("H Expand");
  gtk_widget_set_halign (toggle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  g_object_set (toggle, "margin", 5, NULL);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_hexpand), NULL);
  gtk_container_add (GTK_CONTAINER (colorbox), toggle);

  gtk_grid_attach (GTK_GRID (grid), colorbox, 1, 1, 1, 1);

  colorbox = gtk_frame_new (NULL);

  toggle = gtk_toggle_button_new_with_label ("V Expand");
  gtk_widget_set_halign (toggle, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  g_object_set (toggle, "margin", 5, NULL);
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_vexpand), NULL);
  gtk_container_add (GTK_CONTAINER (colorbox), toggle);

  gtk_grid_attach (GTK_GRID (grid), colorbox, 1, 2, 1, 1); 

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show (window);
}

int
main (int argc, char *argv[])
{
  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  create_box_window ();
  create_grid_window ();

  gtk_main ();

  return 0;
}
