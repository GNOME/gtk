/* testscrolledge.c
 *
 * Copyright (C) 2014 Matthias Clasen <mclasen@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

static guint add_rows_id = 0;

static void
populate_list (GtkListBox *list)
{
  gint i;
  gchar *text;
  GtkWidget *row, *label;
  gint n;
  GList *l;

  l = gtk_container_get_children (GTK_CONTAINER (list));
  n = g_list_length (l);
  g_list_free (l);

  for (i = 1; i <= 50; i++)
    {
      row = gtk_list_box_row_new ();
      text = g_strdup_printf ("List row %d", i + n);
      label = gtk_label_new (text);
      g_free (text);

      g_object_set (label, "margin", 10, NULL);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_container_add (GTK_CONTAINER (row), label);
      gtk_widget_show_all (row);
      gtk_container_add (GTK_CONTAINER (list), row);
    }
}

static GtkWidget *popup;
static GtkWidget *spinner;

static gboolean
add_rows (gpointer data)
{
  GtkListBox *list = data;

  gtk_widget_hide (popup);
  gtk_spinner_stop (GTK_SPINNER (spinner));

  populate_list (list);
  add_rows_id = 0;

  return G_SOURCE_REMOVE;
}

static void
edge_overshot (GtkScrolledWindow *sw,
               GtkPositionType    pos,
               GtkListBox        *list)
{
  if (pos == GTK_POS_BOTTOM)
    {
      gtk_spinner_start (GTK_SPINNER (spinner));
      gtk_widget_show (popup);

      if (add_rows_id == 0)
        add_rows_id = g_timeout_add (2000, add_rows, list);
    }
}

static void
edge_reached (GtkScrolledWindow *sw,
	      GtkPositionType    pos,
	      GtkListBox        *list)
{
  g_print ("Reached the edge at pos %d!\n", pos);
}

int
main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *sw;
  GtkWidget *list;
  GtkWidget *overlay;
  GtkWidget *label;

  gtk_init (NULL, NULL);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 600, 400);

  overlay = gtk_overlay_new ();
  popup = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (popup, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (popup, GTK_ALIGN_END);
  g_object_set (popup, "margin", 40, NULL);
  label = gtk_label_new ("Getting more rows...");
  spinner = gtk_spinner_new ();
  gtk_widget_show (spinner);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (popup), label);
  gtk_container_add (GTK_CONTAINER (popup), spinner);

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), popup);
  gtk_widget_set_no_show_all (popup, TRUE);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);

  gtk_container_add (GTK_CONTAINER (win), overlay);
  gtk_container_add (GTK_CONTAINER (overlay), sw);
  gtk_container_add (GTK_CONTAINER (sw), list);
  populate_list (GTK_LIST_BOX (list));

  g_signal_connect (sw, "edge-overshot", G_CALLBACK (edge_overshot), list);
  g_signal_connect (sw, "edge-reached", G_CALLBACK (edge_reached), list);

  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
