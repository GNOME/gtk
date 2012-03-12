/* testiconview.c
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
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
#include <sys/types.h>
#include <string.h>
#include "prop-editor.h"

#define NUMBER_OF_ITEMS   10
#define SOME_ITEMS       100
#define MANY_ITEMS     10000

static void
fill_model (GtkTreeModel *model)
{
  GdkPixbuf *pixbuf;
  int i;
  char *str, *str2;
  GtkTreeIter iter;
  GtkListStore *store = GTK_LIST_STORE (model);
  gint32 size;
  
  pixbuf = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);

  i = 0;
  
  gtk_list_store_prepend (store, &iter);

  gtk_list_store_set (store, &iter,
		      0, pixbuf,
		      1, "Really really\nreally really loooooooooong item name",
		      2, 0,
		      3, "This is a <b>Test</b> of <i>markup</i>",
		      4, TRUE,
		      -1);

  while (i < NUMBER_OF_ITEMS - 1)
    {
      GdkPixbuf *pb;
      size = g_random_int_range (20, 70);
      pb = gdk_pixbuf_scale_simple (pixbuf, size, size, GDK_INTERP_NEAREST);

      str = g_strdup_printf ("Icon %d", i);
      str2 = g_strdup_printf ("Icon <b>%d</b>", i);	
      gtk_list_store_prepend (store, &iter);
      gtk_list_store_set (store, &iter,
			  0, pb,
			  1, str,
			  2, i,
			  3, str2,
			  4, TRUE,
			  -1);
      g_free (str);
      g_free (str2);
      i++;
    }
  
  //  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 2, GTK_SORT_ASCENDING);
}

static GtkTreeModel *
create_model (void)
{
  GtkListStore *store;
  
  store = gtk_list_store_new (5, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN);

  return GTK_TREE_MODEL (store);
}


static void
foreach_selected_remove (GtkWidget *button, GtkIconView *icon_list)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  GList *list, *selected;

  selected = gtk_icon_view_get_selected_items (icon_list);
  model = gtk_icon_view_get_model (icon_list);
  
  for (list = selected; list; list = list->next)
    {
      GtkTreePath *path = list->data;

      gtk_tree_model_get_iter (model, &iter, path);
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      
      gtk_tree_path_free (path);
    } 
  
  g_list_free (selected);
}


static void
swap_rows (GtkWidget *button, GtkIconView *icon_list)
{
  GtkTreeIter iter, iter2;
  GtkTreeModel *model;

  model = gtk_icon_view_get_model (icon_list);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), -2, GTK_SORT_ASCENDING);

  gtk_tree_model_get_iter_first (model, &iter);
  iter2 = iter;
  gtk_tree_model_iter_next (model, &iter2);
  gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter2);
}

static void
add_n_items (GtkIconView *icon_list, gint n)
{
  static gint count = NUMBER_OF_ITEMS;

  GtkTreeIter iter;
  GtkListStore *store;
  GdkPixbuf *pixbuf;
  gchar *str, *str2;
  gint i;

  store = GTK_LIST_STORE (gtk_icon_view_get_model (icon_list));
  pixbuf = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);


  for (i = 0; i < n; i++)
    {
      str = g_strdup_printf ("Icon %d", count);
      str2 = g_strdup_printf ("Icon <b>%d</b>", count);	
      gtk_list_store_prepend (store, &iter);
      gtk_list_store_set (store, &iter,
			  0, pixbuf,
			  1, str,
			  2, i,
			  3, str2,
			  -1);
      g_free (str);
      g_free (str2);
      count++;
    }
}

static void
add_some (GtkWidget *button, GtkIconView *icon_list)
{
  add_n_items (icon_list, SOME_ITEMS);
}

static void
add_many (GtkWidget *button, GtkIconView *icon_list)
{
  add_n_items (icon_list, MANY_ITEMS);
}

static void
add_large (GtkWidget *button, GtkIconView *icon_list)
{
  GtkListStore *store;
  GtkTreeIter iter;

  GdkPixbuf *pixbuf, *pb;
  gchar *str;

  store = GTK_LIST_STORE (gtk_icon_view_get_model (icon_list));
  pixbuf = gdk_pixbuf_new_from_file ("gnome-textfile.png", NULL);

  pb = gdk_pixbuf_scale_simple (pixbuf, 
				2 * gdk_pixbuf_get_width (pixbuf),
				2 * gdk_pixbuf_get_height (pixbuf),
				GDK_INTERP_BILINEAR);

  str = g_strdup_printf ("Some really long text");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pb,
		      1, str,
		      2, 0,
		      3, str,
		      -1);
  g_object_unref (pb);
  g_free (str);
  
  pb = gdk_pixbuf_scale_simple (pixbuf, 
				3 * gdk_pixbuf_get_width (pixbuf),
				3 * gdk_pixbuf_get_height (pixbuf),
				GDK_INTERP_BILINEAR);

  str = g_strdup ("see how long text behaves when placed underneath "
		  "an oversized icon which would allow for long lines");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pb,
		      1, str,
		      2, 1,
		      3, str,
		      -1);
  g_object_unref (pb);
  g_free (str);

  pb = gdk_pixbuf_scale_simple (pixbuf, 
				3 * gdk_pixbuf_get_width (pixbuf),
				3 * gdk_pixbuf_get_height (pixbuf),
				GDK_INTERP_BILINEAR);

  str = g_strdup ("short text");
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, pb,
		      1, str,
		      2, 2,
		      3, str,
		      -1);
  g_object_unref (pb);
  g_free (str);

  g_object_unref (pixbuf);
}

static void
select_all (GtkWidget *button, GtkIconView *icon_list)
{
  gtk_icon_view_select_all (icon_list);
}

static void
select_nonexisting (GtkWidget *button, GtkIconView *icon_list)
{  
  GtkTreePath *path = gtk_tree_path_new_from_indices (999999, -1);
  gtk_icon_view_select_path (icon_list, path);
  gtk_tree_path_free (path);
}

static void
unselect_all (GtkWidget *button, GtkIconView *icon_list)
{
  gtk_icon_view_unselect_all (icon_list);
}

static void
selection_changed (GtkIconView *icon_list)
{
  g_print ("Selection changed!\n");
}

typedef struct {
  GtkIconView     *icon_list;
  GtkTreePath     *path;
} ItemData;

static void
free_item_data (ItemData *data)
{
  gtk_tree_path_free (data->path);
  g_free (data);
}

static void
item_activated (GtkIconView *icon_view,
		GtkTreePath *path)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *text;

  model = gtk_icon_view_get_model (icon_view);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 1, &text, -1);
  g_print ("Item activated, text is %s\n", text);
  g_free (text);
  
}

static void
toggled (GtkCellRendererToggle *cell,
	 gchar                 *path_string,
	 gpointer               data)
{
  GtkTreeModel *model = GTK_TREE_MODEL (data);
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  gboolean value;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 4, &value, -1);

  value = !value;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 4, value, -1);

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
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, new_text, -1);

  gtk_tree_path_free (path);
}

static void
item_cb (GtkWidget *menuitem,
	 ItemData  *data)
{
  item_activated (data->icon_list, data->path);
}

static void
do_popup_menu (GtkWidget      *icon_list, 
	       GdkEventButton *event)
{
  GtkIconView *icon_view = GTK_ICON_VIEW (icon_list); 
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkTreePath *path = NULL;
  int button, event_time;
  ItemData *data;
  GList *list;

  if (event)
    path = gtk_icon_view_get_path_at_pos (icon_view, event->x, event->y);
  else
    {
      list = gtk_icon_view_get_selected_items (icon_view);

      if (list)
        {
          path = (GtkTreePath*)list->data;
          g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
        }
    }

  if (!path)
    return;

  menu = gtk_menu_new ();

  data = g_new0 (ItemData, 1);
  data->icon_list = icon_view;
  data->path = path;
  g_object_set_data_full (G_OBJECT (menu), "item-path", data, (GDestroyNotify)free_item_data);

  menuitem = gtk_menu_item_new_with_label ("Activate");
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  g_signal_connect (menuitem, "activate", G_CALLBACK (item_cb), data);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 
                  button, event_time);
}
	

static gboolean
button_press_event_handler (GtkWidget      *widget, 
			    GdkEventButton *event)
{
  /* Ignore double-clicks and triple-clicks */
  if (gdk_event_triggers_context_menu ((GdkEvent *) event) &&
      event->type == GDK_BUTTON_PRESS)
    {
      do_popup_menu (widget, event);
      return TRUE;
    }

  return FALSE;
}

static gboolean
popup_menu_handler (GtkWidget *widget)
{
  do_popup_menu (widget, NULL);
  return TRUE;
}

static const GtkTargetEntry item_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, 0 }
};
	
gint
main (gint argc, gchar **argv)
{
  GtkWidget *paned, *tv;
  GtkWidget *window, *icon_list, *scrolled_window;
  GtkWidget *vbox, *bbox;
  GtkWidget *button;
  GtkWidget *prop_editor;
  GtkTreeModel *model;
  GtkCellRenderer *cell;
  GtkTreeViewColumn *tvc;
  
  gtk_init (&argc, &argv);

  /* to test rtl layout, set RTL=1 in the environment */
  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 700, 400);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);

  icon_list = gtk_icon_view_new ();
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (icon_list), GTK_SELECTION_MULTIPLE);

  tv = gtk_tree_view_new ();
  tvc = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), tvc);

  g_signal_connect_after (icon_list, "button_press_event",
			  G_CALLBACK (button_press_event_handler), NULL);
  g_signal_connect (icon_list, "selection_changed",
		    G_CALLBACK (selection_changed), NULL);
  g_signal_connect (icon_list, "popup_menu",
		    G_CALLBACK (popup_menu_handler), NULL);

  g_signal_connect (icon_list, "item_activated",
		    G_CALLBACK (item_activated), NULL);
  
  model = create_model ();
  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_list), model);
  gtk_tree_view_set_model (GTK_TREE_VIEW (tv), model);
  fill_model (model);

#if 0

  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (icon_list), 0);
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (icon_list), 1);

#else

  cell = gtk_cell_renderer_toggle_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_list), cell, FALSE);
  g_object_set (cell, "activatable", TRUE, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_list),
				  cell, "active", 4, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (toggled), model);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_list), cell, FALSE);
  g_object_set (cell, 
		"follow-state", TRUE, 
		NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_list),
				  cell, "pixbuf", 0, NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_list), cell, FALSE);
  g_object_set (cell, 
		"editable", TRUE, 
		"xalign", 0.5,
		"wrap-mode", PANGO_WRAP_WORD_CHAR,
		"wrap-width", 100,
		NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_list),
				  cell, "text", 1, NULL);
  g_signal_connect (cell, "edited", G_CALLBACK (edited), model);

  /* now the tree view... */
  cell = gtk_cell_renderer_toggle_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tvc), cell, FALSE);
  g_object_set (cell, "activatable", TRUE, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (tvc),
				  cell, "active", 4, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (toggled), model);

  cell = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tvc), cell, FALSE);
  g_object_set (cell, 
		"follow-state", TRUE, 
		NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (tvc),
				  cell, "pixbuf", 0, NULL);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (tvc), cell, FALSE);
  g_object_set (cell, "editable", TRUE, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (tvc),
				  cell, "text", 1, NULL);
  g_signal_connect (cell, "edited", G_CALLBACK (edited), model);
#endif
  /* Allow DND between the icon view and the tree view */
  
  gtk_icon_view_enable_model_drag_source (GTK_ICON_VIEW (icon_list),
					  GDK_BUTTON1_MASK,
					  item_targets,
					  G_N_ELEMENTS (item_targets),
					  GDK_ACTION_MOVE);
  gtk_icon_view_enable_model_drag_dest (GTK_ICON_VIEW (icon_list),
					item_targets,
					G_N_ELEMENTS (item_targets),
					GDK_ACTION_MOVE);

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tv),
					  GDK_BUTTON1_MASK,
					  item_targets,
					  G_N_ELEMENTS (item_targets),
					  GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (tv),
					item_targets,
					G_N_ELEMENTS (item_targets),
					GDK_ACTION_MOVE);

			      
  prop_editor = create_prop_editor (G_OBJECT (icon_list), 0);
  gtk_widget_show_all (prop_editor);
  
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), icon_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
  				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_paned_add1 (GTK_PANED (paned), scrolled_window);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tv);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
  				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_paned_add2 (GTK_PANED (paned), scrolled_window);

  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Add some");
  g_signal_connect (button, "clicked", G_CALLBACK (add_some), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Add many");
  g_signal_connect (button, "clicked", G_CALLBACK (add_many), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Add large");
  g_signal_connect (button, "clicked", G_CALLBACK (add_large), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Remove selected");
  g_signal_connect (button, "clicked", G_CALLBACK (foreach_selected_remove), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Swap");
  g_signal_connect (button, "clicked", G_CALLBACK (swap_rows), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Select all");
  g_signal_connect (button, "clicked", G_CALLBACK (select_all), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Unselect all");
  g_signal_connect (button, "clicked", G_CALLBACK (unselect_all), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Select nonexisting");
  g_signal_connect (button, "clicked", G_CALLBACK (select_nonexisting), icon_list);
  gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);

  icon_list = gtk_icon_view_new ();

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), icon_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_add2 (GTK_PANED (paned), scrolled_window);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
