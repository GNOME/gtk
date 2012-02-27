/* testorientable.c
 * Copyright (C) 2004  Red Hat, Inc.
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
orient_toggled (GtkToggleButton *button, gpointer user_data)
{
  GList *orientables = (GList *) user_data, *ptr;
  gboolean state = gtk_toggle_button_get_active (button);
  GtkOrientation orientation;

  if (state)
    {
      orientation = GTK_ORIENTATION_VERTICAL;
      gtk_button_set_label (GTK_BUTTON (button), "Vertical");
    }
  else
    {
      orientation = GTK_ORIENTATION_HORIZONTAL;
      gtk_button_set_label (GTK_BUTTON (button), "Horizontal");
    }

  for (ptr = orientables; ptr; ptr = ptr->next)
    {
      GtkOrientable *orientable = GTK_ORIENTABLE (ptr->data);

      gtk_orientable_set_orientation (orientable, orientation);
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *grid;
  GtkWidget *box, *button;
  GList *orientables = NULL;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  grid= gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  /* GtkBox */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  orientables = g_list_prepend (orientables, box);
  gtk_grid_attach (GTK_GRID (grid), box, 0, 1, 1, 1);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkBox 1"),
                  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkBox 2"),
                  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkBox 3"),
                  TRUE, TRUE, 0);

  /* GtkButtonBox */
  box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  orientables = g_list_prepend (orientables, box);
  gtk_grid_attach (GTK_GRID (grid), box, 1, 1, 1, 1);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkButtonBox 1"),
                  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkButtonBox 2"),
                  TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box),
                  gtk_button_new_with_label ("GtkButtonBox 3"),
                  TRUE, TRUE, 0);

  /* GtkSeparator */
  box = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  orientables = g_list_prepend (orientables, box);
  gtk_grid_attach (GTK_GRID (grid), box, 2, 1, 1, 1);

  button = gtk_toggle_button_new_with_label ("Horizontal");
  gtk_grid_attach (GTK_GRID (grid), button, 0, 0, 1, 1);
  g_signal_connect (button, "toggled",
                  G_CALLBACK (orient_toggled), orientables);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_widget_show_all (window);

  g_signal_connect (window, "destroy",
                  G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
