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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

static void
on_toggle_hexpand (GtkToggleButton *toggle,
                   void            *data)
{
  GtkWidget *parent;

  /* get the event box with color set on it */
  parent = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (toggle)));

  g_object_set (toggle,
                "hexpand", gtk_toggle_button_get_active (toggle),
                NULL);
}

static void
on_toggle_vexpand (GtkToggleButton *toggle,
                   void            *data)
{
  GtkWidget *parent;

  /* get the event box with color set on it */
  parent = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (toggle)));

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
  GtkWidget *alignment;
  GtkWidget *colorbox;
  GdkRGBA red, blue;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Boxes");

  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_box_pack_start (GTK_BOX (box1),
                      gtk_label_new ("VBox 1 Top"),
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box1),
                      box2,
                      FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box1),
                    gtk_label_new ("VBox 1 Bottom"),
                    FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box2),
                      gtk_label_new ("HBox 2 Left"),
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box2),
                      box3,
                      FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box2),
                    gtk_label_new ("HBox 2 Right"),
                    FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box3),
                      gtk_label_new ("VBox 3 Top"),
                      FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (box3),
                    gtk_label_new ("VBox 3 Bottom"),
                    FALSE, FALSE, 0);

  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&blue, "blue");

  colorbox = gtk_event_box_new ();
  gtk_widget_override_background_color (colorbox, 0, &red);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
  gtk_container_add (GTK_CONTAINER (colorbox), alignment);

  toggle = gtk_toggle_button_new_with_label ("H Expand");
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_hexpand), NULL);
  gtk_container_add (GTK_CONTAINER (alignment), toggle);

  gtk_box_pack_start (GTK_BOX (box3),
                      colorbox,
                      FALSE, TRUE, 0);

  colorbox = gtk_event_box_new ();
  gtk_widget_override_background_color (colorbox, 0, &blue);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
  gtk_container_add (GTK_CONTAINER (colorbox), alignment);

  toggle = gtk_toggle_button_new_with_label ("V Expand");
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_vexpand), NULL);
  gtk_container_add (GTK_CONTAINER (alignment), toggle);
  gtk_box_pack_start (GTK_BOX (box3),
                      colorbox,
                      FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (window), box1);
  gtk_widget_show_all (window);
}

static void
create_table_window (void)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *toggle;
  GtkWidget *alignment;
  GtkWidget *colorbox;
  GdkRGBA red, blue;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Table");

  table = gtk_table_new (4, 3, FALSE);

  gtk_table_attach (GTK_TABLE (table),
                    gtk_label_new ("Top"),
                    1, 2, 0, 1,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table),
                    gtk_label_new ("Bottom"),
                    1, 2, 3, 4,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table),
                    gtk_label_new ("Left"),
                    0, 1, 1, 3,
                    GTK_FILL, GTK_FILL, 0, 0);
  gtk_table_attach (GTK_TABLE (table),
                    gtk_label_new ("Right"),
                    2, 3, 1, 3,
                    GTK_FILL, GTK_FILL, 0, 0);

  gdk_rgba_parse (&red, "red");
  gdk_rgba_parse (&blue, "blue");

  colorbox = gtk_event_box_new ();
  gtk_widget_override_background_color (colorbox, GTK_STATE_NORMAL, &red);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
  gtk_container_add (GTK_CONTAINER (colorbox), alignment);

  toggle = gtk_toggle_button_new_with_label ("H Expand");
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_hexpand), NULL);
  gtk_container_add (GTK_CONTAINER (alignment), toggle);

  gtk_table_attach (GTK_TABLE (table),
                    colorbox,
                    1, 2, 1, 2,
                    GTK_FILL, GTK_FILL, 0, 0);

  colorbox = gtk_event_box_new ();
  gtk_widget_override_background_color (colorbox, 0, &blue);

  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 5, 5, 5, 5);
  gtk_container_add (GTK_CONTAINER (colorbox), alignment);

  toggle = gtk_toggle_button_new_with_label ("V Expand");
  g_signal_connect (G_OBJECT (toggle), "toggled",
                    G_CALLBACK (on_toggle_vexpand), NULL);
  gtk_container_add (GTK_CONTAINER (alignment), toggle);

  gtk_table_attach (GTK_TABLE (table),
                    colorbox,
                    1, 2, 2, 3,
                    GTK_FILL, GTK_FILL, 0, 0);

  gtk_container_add (GTK_CONTAINER (window), table);
  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  create_box_window ();
  create_table_window ();

  gtk_main ();

  return 0;
}
