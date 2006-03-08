/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 2006  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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
#include <gtk/gtk.h>

enum {
  PACK_START,
  PACK_END,
  PACK_ALTERNATE
};

enum {
  GROUP_A,
  GROUP_B
};

gchar *tabs1 [] = {
  "a",
  "b",
  "c",
  "d",
  NULL
};

gchar *tabs2 [] = {
  "1",
  "2",
  "3",
  "4",
  "5",
  NULL
};

gchar *tabs3 [] = {
  "foo",
  "bar",
  NULL
};

gchar *tabs4 [] = {
  "beer",
  "water",
  "lemonade",
  "coffee",
  "tea",
  NULL
};

static GtkNotebook*
window_creation_function (GtkNotebook *source_notebook,
			  GtkWidget   *child,
			  gint         x,
			  gint         y,
			  gpointer     data)
{
  GtkWidget *window, *notebook;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  notebook = gtk_notebook_new ();

  gtk_notebook_set_group_id (GTK_NOTEBOOK (notebook),
			     gtk_notebook_get_group_id (source_notebook));

  gtk_container_add (GTK_CONTAINER (window), notebook);

  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_widget_show_all (window);

  return GTK_NOTEBOOK (notebook);
}

static void
on_page_reordered (GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer data)
{
  g_print ("page %d reordered\n", page_num);
}

static GtkWidget*
create_notebook (gchar           **labels,
		 gint              group_id,
		 gint              packing,
		 GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page;
  gint count = 0;

  notebook = gtk_notebook_new ();

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_notebook_set_group_id (GTK_NOTEBOOK (notebook), group_id);

  while (*labels)
    {
      page = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (page), *labels);

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      if (packing == PACK_END ||
	  (packing == PACK_ALTERNATE && count % 2 == 1))
	gtk_container_child_set (GTK_CONTAINER (notebook), page, "tab-pack", GTK_PACK_END, NULL);

      count++;
      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
		    G_CALLBACK (on_page_reordered), NULL);

  return notebook;
}

gint
main (gint argc, gchar *argv[])
{
  GtkWidget *window, *table;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  table = gtk_table_new (2, 2, TRUE);

  gtk_notebook_set_window_creation_hook (window_creation_function, NULL);

  gtk_table_attach_defaults (GTK_TABLE (table),
			     create_notebook (tabs1, GROUP_A, PACK_START, GTK_POS_TOP),
			     0, 1, 0, 1);

  gtk_table_attach_defaults (GTK_TABLE (table),
			     create_notebook (tabs2, GROUP_B, PACK_ALTERNATE, GTK_POS_BOTTOM),
			     0, 1, 1, 2);

  gtk_table_attach_defaults (GTK_TABLE (table),
			     create_notebook (tabs3, GROUP_B, PACK_END, GTK_POS_LEFT),
			     1, 2, 0, 1);

  gtk_table_attach_defaults (GTK_TABLE (table),
			     create_notebook (tabs4, GROUP_A, PACK_ALTERNATE, GTK_POS_RIGHT),
			     1, 2, 1, 2);

  gtk_container_add (GTK_CONTAINER (window), table);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
