/* testtreeedit.c
 * Copyright (C) 2001 Red Hat, Inc
 * Author: Jonathan Blandford
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

typedef struct {
  const gchar *string;
  gboolean is_editable;
  gboolean is_sensitive;
  gint progress;
} ListEntry;

enum {
  STRING_COLUMN,
  IS_EDITABLE_COLUMN,
  IS_SENSITIVE_COLUMN,
  PIXBUF_COLUMN,
  LAST_PIXBUF_COLUMN,
  PROGRESS_COLUMN,
  NUM_COLUMNS
};

static ListEntry model_strings[] =
{
  {"A simple string", TRUE, TRUE, 0 },
  {"Another string!", TRUE, TRUE, 10 },
  {"", TRUE, TRUE, 0 },
  {"Guess what, a third string. This one can't be edited", FALSE, TRUE, 47 },
  {"And then a fourth string. Neither can this", FALSE, TRUE, 48 },
  {"Multiline\nFun!", TRUE, FALSE, 75 },
  { NULL }
};

static GtkTreeModel *
create_model (void)
{
  GtkTreeStore *model;
  GtkTreeIter iter;
  gint i;
  GdkPixbuf *foo, *bar;
  GtkWidget *blah;

  blah = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  foo = gtk_widget_render_icon_pixbuf (blah, GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
  bar = gtk_widget_render_icon_pixbuf (blah, GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
  gtk_widget_destroy (blah);
  
  model = gtk_tree_store_new (NUM_COLUMNS,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN,
			      G_TYPE_BOOLEAN,
			      GDK_TYPE_PIXBUF,
			      GDK_TYPE_PIXBUF,
			      G_TYPE_INT);

  for (i = 0; model_strings[i].string != NULL; i++)
    {
      gtk_tree_store_append (model, &iter, NULL);

      gtk_tree_store_set (model, &iter,
			  STRING_COLUMN, model_strings[i].string,
			  IS_EDITABLE_COLUMN, model_strings[i].is_editable,
			  IS_SENSITIVE_COLUMN, model_strings[i].is_sensitive,
			  PIXBUF_COLUMN, foo,
			  LAST_PIXBUF_COLUMN, bar,
			  PROGRESS_COLUMN, model_strings[i].progress,
			  -1);
    }
  
  return GTK_TREE_MODEL (model);
}

static void
editable_toggled (GtkCellRendererToggle *cell,
		  gchar                 *path_string,
		  gpointer               data)
{
  GtkTreeModel *model = GTK_TREE_MODEL (data);
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  gboolean value;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, IS_EDITABLE_COLUMN, &value, -1);

  value = !value;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, IS_EDITABLE_COLUMN, value, -1);

  gtk_tree_path_free (path);
}

static void
sensitive_toggled (GtkCellRendererToggle *cell,
		   gchar                 *path_string,
		   gpointer               data)
{
  GtkTreeModel *model = GTK_TREE_MODEL (data);
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  gboolean value;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, IS_SENSITIVE_COLUMN, &value, -1);

  value = !value;
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, IS_SENSITIVE_COLUMN, value, -1);

  gtk_tree_path_free (path);
}

static void
edited (GtkCellRendererText *cell,
	gchar               *path_string,
	gchar               *new_text,
	gpointer             data)
{
  GtkTreeModel *model = GTK_TREE_MODEL (data);
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter, STRING_COLUMN, new_text, -1);

  gtk_tree_path_free (path);
}

static gboolean
button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	/* Deselect if people click outside any row. */
	if (event->window == gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))
	    && !gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y, NULL, NULL, NULL, NULL)) {
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)));
	}

	/* Let the default code run in any case; it won't reselect anything. */
	return FALSE;
}

typedef struct {
  GtkCellArea     *area;
  GtkCellRenderer *renderer;
} CallbackData;

static void
align_cell_toggled (GtkToggleButton  *toggle,
		    CallbackData     *data)
{
  gboolean active = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (data->area, data->renderer, "align", active, NULL);
}

static void
expand_cell_toggled (GtkToggleButton  *toggle,
		     CallbackData     *data)
{
  gboolean active = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (data->area, data->renderer, "expand", active, NULL);
}

static void
fixed_cell_toggled (GtkToggleButton  *toggle,
		    CallbackData     *data)
{
  gboolean active = gtk_toggle_button_get_active (toggle);

  gtk_cell_area_cell_set (data->area, data->renderer, "fixed-size", active, NULL);
}

enum {
  CNTL_EXPAND,
  CNTL_ALIGN,
  CNTL_FIXED
};

static void
create_control (GtkWidget *box, gint number, gint cntl, CallbackData *data)
{
  GtkWidget *checkbutton;
  GCallback  callback = NULL;
  gchar *name = NULL;

  switch (cntl)
    {
    case CNTL_EXPAND: 
      name = g_strdup_printf ("Expand Cell #%d", number); 
      callback = G_CALLBACK (expand_cell_toggled);
      break;
    case CNTL_ALIGN: 
      name = g_strdup_printf ("Align Cell #%d", number); 
      callback = G_CALLBACK (align_cell_toggled);
      break;
    case CNTL_FIXED: 
      name = g_strdup_printf ("Fix size Cell #%d", number); 
      callback = G_CALLBACK (fixed_cell_toggled);
      break;
    }

  checkbutton = gtk_check_button_new_with_label (name);
  gtk_widget_show (checkbutton);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), cntl == CNTL_FIXED);
  gtk_box_pack_start (GTK_BOX (box), checkbutton, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (checkbutton), "toggled", callback, data);
  g_free (name);
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *window;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkWidget *vbox, *hbox, *cntl_vbox;
  GtkTreeModel *tree_model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkCellArea *area;
  CallbackData callback[4];
  
  gtk_init (&argc, &argv);

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkTreeView editing sample");
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), 
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  tree_model = create_model ();
  tree_view = gtk_tree_view_new_with_model (tree_model);
  g_signal_connect (tree_view, "button_press_event", G_CALLBACK (button_press_event), NULL);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), TRUE);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "String");
  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", PIXBUF_COLUMN, 
				       "sensitive", IS_SENSITIVE_COLUMN,
				       NULL);
  callback[0].area = area;
  callback[0].renderer = renderer;

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "text", STRING_COLUMN,
				       "editable", IS_EDITABLE_COLUMN,
				       "sensitive", IS_SENSITIVE_COLUMN,
				       NULL);
  callback[1].area = area;
  callback[1].renderer = renderer;
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (edited), tree_model);
  g_object_set (renderer,
                "placeholder-text", "Type here",
                NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
		  		       "text", STRING_COLUMN,
				       "editable", IS_EDITABLE_COLUMN,
				       "sensitive", IS_SENSITIVE_COLUMN,
				       NULL);
  callback[2].area = area;
  callback[2].renderer = renderer;
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (edited), tree_model);
  g_object_set (renderer,
                "placeholder-text", "Type here too",
                NULL);

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
		"xalign", 0.0,
		NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", LAST_PIXBUF_COLUMN, 
				       "sensitive", IS_SENSITIVE_COLUMN,
				       NULL);
  callback[3].area = area;
  callback[3].renderer = renderer;

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
		    G_CALLBACK (editable_toggled), tree_model);
  
  g_object_set (renderer,
		"xalign", 0.0,
		NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Editable",
					       renderer,
					       "active", IS_EDITABLE_COLUMN,
					       NULL);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
		    G_CALLBACK (sensitive_toggled), tree_model);
  
  g_object_set (renderer,
		"xalign", 0.0,
		NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Sensitive",
					       renderer,
					       "active", IS_SENSITIVE_COLUMN,
					       NULL);

  renderer = gtk_cell_renderer_progress_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Progress",
					       renderer,
					       "value", PROGRESS_COLUMN,
					       NULL);

  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  
  gtk_window_set_default_size (GTK_WINDOW (window),
			       800, 250);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* Alignment controls */
  cntl_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_show (cntl_vbox);
  gtk_box_pack_start (GTK_BOX (hbox), cntl_vbox, FALSE, FALSE, 0);

  create_control (cntl_vbox, 1, CNTL_ALIGN, &callback[0]);
  create_control (cntl_vbox, 2, CNTL_ALIGN, &callback[1]);
  create_control (cntl_vbox, 3, CNTL_ALIGN, &callback[2]);
  create_control (cntl_vbox, 4, CNTL_ALIGN, &callback[3]);

  /* Expand controls */
  cntl_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_show (cntl_vbox);
  gtk_box_pack_start (GTK_BOX (hbox), cntl_vbox, FALSE, FALSE, 0);

  create_control (cntl_vbox, 1, CNTL_EXPAND, &callback[0]);
  create_control (cntl_vbox, 2, CNTL_EXPAND, &callback[1]);
  create_control (cntl_vbox, 3, CNTL_EXPAND, &callback[2]);
  create_control (cntl_vbox, 4, CNTL_EXPAND, &callback[3]);

  /* Fixed controls */
  cntl_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_show (cntl_vbox);
  gtk_box_pack_start (GTK_BOX (hbox), cntl_vbox, FALSE, FALSE, 0);

  create_control (cntl_vbox, 1, CNTL_FIXED, &callback[0]);
  create_control (cntl_vbox, 2, CNTL_FIXED, &callback[1]);
  create_control (cntl_vbox, 3, CNTL_FIXED, &callback[2]);
  create_control (cntl_vbox, 4, CNTL_FIXED, &callback[3]);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
