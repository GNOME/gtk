/* testtoolbar.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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
change_orientation (GtkWidget *button, GtkWidget *toolbar)
{
  GtkWidget *grid;
  GtkOrientation orientation;

  grid = gtk_widget_get_parent (toolbar);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    orientation = GTK_ORIENTATION_VERTICAL;
  else
    orientation = GTK_ORIENTATION_HORIZONTAL;

  g_object_ref (toolbar);
  gtk_container_remove (GTK_CONTAINER (grid), toolbar);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), orientation);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_widget_set_hexpand (toolbar, TRUE);
      gtk_widget_set_vexpand (toolbar, FALSE);
      gtk_grid_attach (GTK_GRID (grid), toolbar, 0, 0, 2, 1);
    }
  else
    {
      gtk_widget_set_hexpand (toolbar, FALSE);
      gtk_widget_set_vexpand (toolbar, TRUE);
      gtk_grid_attach (GTK_GRID (grid), toolbar, 0, 0, 1, 5);
    }
  g_object_unref (toolbar);
}

static void
change_show_arrow (GtkWidget *button, GtkWidget *toolbar)
{
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar),
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
set_toolbar_style_toggled (GtkCheckButton *button, GtkToolbar *toolbar)
{
  GtkWidget *option_menu;
  int style;
  
  option_menu = g_object_get_data (G_OBJECT (button), "option-menu");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      style = gtk_combo_box_get_active (GTK_COMBO_BOX (option_menu));

      gtk_toolbar_set_style (toolbar, style);
      gtk_widget_set_sensitive (option_menu, TRUE);
    }
  else
    {
      gtk_toolbar_unset_style (toolbar);
      gtk_widget_set_sensitive (option_menu, FALSE);
    }
}

static void
change_toolbar_style (GtkWidget *option_menu, GtkWidget *toolbar)
{
  GtkToolbarStyle style;

  style = gtk_combo_box_get_active (GTK_COMBO_BOX (option_menu));
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), style);
}

static void
set_visible_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		 GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GtkToolItem *tool_item;
  gboolean visible;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_get (tool_item, "visible", &visible, NULL);
  g_object_set (cell, "active", visible, NULL);
  g_object_unref (tool_item);
}

static void
visibile_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		 GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkToolItem *tool_item;
  gboolean visible;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  g_object_get (tool_item, "visible", &visible, NULL);
  g_object_set (tool_item, "visible", !visible, NULL);
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
set_expand_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GtkToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (cell, "active", gtk_tool_item_get_expand (tool_item), NULL);
  g_object_unref (tool_item);
}

static void
expand_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
	       GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  gtk_tool_item_set_expand (tool_item, !gtk_tool_item_get_expand (tool_item));
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
set_homogeneous_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GtkToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (cell, "active", gtk_tool_item_get_homogeneous (tool_item), NULL);
  g_object_unref (tool_item);
}

static void
homogeneous_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		    GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  gtk_tool_item_set_homogeneous (tool_item, !gtk_tool_item_get_homogeneous (tool_item));
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}


static void
set_important_func(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		   GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GtkToolItem *tool_item;

  gtk_tree_model_get (model, iter, 0, &tool_item, -1);

  g_object_set (cell, "active", gtk_tool_item_get_is_important (tool_item), NULL);
  g_object_unref (tool_item);
}

static void
important_toggled(GtkCellRendererToggle *cell, const gchar *path_str,
		  GtkTreeModel *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkToolItem *tool_item;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &tool_item, -1);
  gtk_tool_item_set_is_important (tool_item, !gtk_tool_item_get_is_important (tool_item));
  g_object_unref (tool_item);

  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static GtkListStore *
create_items_list (GtkWidget **tree_view_p)
{
  GtkWidget *tree_view;
  GtkListStore *list_store;
  GtkCellRenderer *cell;
  
  list_store = gtk_list_store_new (2, GTK_TYPE_TOOL_ITEM, G_TYPE_STRING);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Tool Item",
					       gtk_cell_renderer_text_new (),
					       "text", 1, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (visibile_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Visible",
					      cell,
					      set_visible_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (expand_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Expand",
					      cell,
					      set_expand_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (homogeneous_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Homogeneous",
					      cell,
					      set_homogeneous_func, NULL,NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (important_toggled),
		    list_store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Important",
					      cell,
					      set_important_func, NULL,NULL);

  g_object_unref (list_store);

  *tree_view_p = tree_view;

  return list_store;
}

static void
add_item_to_list (GtkListStore *store, GtkToolItem *item, const gchar *text)
{
  GtkTreeIter iter;

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, item,
		      1, text,
		      -1);
  
}

static void
bold_toggled (GtkToggleToolButton *button)
{
  g_message ("Bold toggled (active=%d)",
	     gtk_toggle_tool_button_get_active (button));
}

static void
set_icon_size_toggled (GtkCheckButton *button, GtkToolbar *toolbar)
{
  GtkWidget *option_menu;
  int icon_size;
  
  option_menu = g_object_get_data (G_OBJECT (button), "option-menu");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      if (gtk_combo_box_get_active (GTK_COMBO_BOX (option_menu)) == 0)
        icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
      else
        icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;

      gtk_toolbar_set_icon_size (toolbar, icon_size);
      gtk_widget_set_sensitive (option_menu, TRUE);
    }
  else
    {
      gtk_toolbar_unset_icon_size (toolbar);
      gtk_widget_set_sensitive (option_menu, FALSE);
    }
}

static void
icon_size_history_changed (GtkComboBox *menu, GtkToolbar *toolbar)
{
  int icon_size;

  if (gtk_combo_box_get_active (menu) == 0)
    icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
  else
    icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;

  gtk_toolbar_set_icon_size (toolbar, icon_size);
}

static gboolean
toolbar_drag_drop (GtkWidget *widget, GdkDragContext *context,
		   gint x, gint y, guint time, GtkWidget *label)
{
  gchar buf[32];

  g_snprintf(buf, sizeof(buf), "%d",
	     gtk_toolbar_get_drop_index (GTK_TOOLBAR (widget), x, y));
  gtk_label_set_label (GTK_LABEL (label), buf);

  return TRUE;
}

static GtkTargetEntry target_table[] = {
  { "application/x-toolbar-item", 0, 0 }
};

static void
rtl_toggled (GtkCheckButton *check)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
  else
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
}

typedef struct
{
  int x;
  int y;
} MenuPositionData;

static void
position_function (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
  /* Do not do this in your own code */

  MenuPositionData *position_data = user_data;

  if (x)
    *x = position_data->x;

  if (y)
    *y = position_data->y;

  if (push_in)
    *push_in = FALSE;
}

static gboolean
popup_context_menu (GtkToolbar *toolbar, gint x, gint y, gint button_number)
{
  MenuPositionData position_data;
  
  GtkMenu *menu = GTK_MENU (gtk_menu_new ());
  int i;

  for (i = 0; i < 5; i++)
    {
      GtkWidget *item;
      gchar *label = g_strdup_printf ("Item _%d", i);
      item = gtk_menu_item_new_with_mnemonic (label);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }
  gtk_widget_show_all (GTK_WIDGET (menu));

  if (button_number != -1)
    {
      position_data.x = x;
      position_data.y = y;
      
      gtk_menu_popup (menu, NULL, NULL, position_function,
		      &position_data, button_number, gtk_get_current_event_time());
    }
  else
    gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());

  return TRUE;
}

static GtkToolItem *drag_item = NULL;

static gboolean
toolbar_drag_motion (GtkToolbar     *toolbar,
		     GdkDragContext *context,
		     gint            x,
		     gint            y,
		     guint           time,
		     gpointer        null)
{
  gint index;
  
  if (!drag_item)
    {
      drag_item = gtk_tool_button_new (NULL, "A quite long button");
      g_object_ref_sink (g_object_ref (drag_item));
    }
  
  gdk_drag_status (context, GDK_ACTION_MOVE, time);

  index = gtk_toolbar_get_drop_index (toolbar, x, y);
  
  gtk_toolbar_set_drop_highlight_item (toolbar, drag_item, index);
  
  return TRUE;
}

static void
toolbar_drag_leave (GtkToolbar     *toolbar,
		    GdkDragContext *context,
		    guint           time,
		    gpointer	    null)
{
  if (drag_item)
    {
      g_object_unref (drag_item);
      drag_item = NULL;
    }
  
  gtk_toolbar_set_drop_highlight_item (toolbar, NULL, 0);
}

static gboolean
timeout_cb (GtkWidget *widget)
{
  static gboolean sensitive = TRUE;
  
  sensitive = !sensitive;
  
  gtk_widget_set_sensitive (widget, sensitive);
  
  return TRUE;
}

static gboolean
timeout_cb1 (GtkWidget *widget)
{
	static gboolean sensitive = TRUE;
	sensitive = !sensitive;
	gtk_widget_set_sensitive (widget, sensitive);
	return TRUE;
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *window, *toolbar, *grid, *treeview, *scrolled_window;
  GtkWidget *hbox, *hbox1, *hbox2, *checkbox, *option_menu, *menu;
  gint i;
  static const gchar *toolbar_styles[] = { "icons", "text", "both (vertical)",
					   "both (horizontal)" };
  GtkToolItem *item;
  GtkListStore *store;
  GtkWidget *image;
  GtkWidget *menuitem;
  GtkWidget *button;
  GtkWidget *label;
  GIcon *gicon;
  GSList *group;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  toolbar = gtk_toolbar_new ();
  gtk_grid_attach (GTK_GRID (grid), toolbar, 0, 0, 2, 1);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 5);
  gtk_grid_attach (GTK_GRID (grid), hbox1, 1, 1, 1, 1);

  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), 5);
  gtk_grid_attach (GTK_GRID (grid), hbox2, 1, 2, 1, 1);

  checkbox = gtk_check_button_new_with_mnemonic("_Vertical");
  gtk_box_pack_start (GTK_BOX (hbox1), checkbox, FALSE, FALSE, 0);
  g_signal_connect (checkbox, "toggled",
		    G_CALLBACK (change_orientation), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Show Arrow");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox1), checkbox, FALSE, FALSE, 0);
  g_signal_connect (checkbox, "toggled",
		    G_CALLBACK (change_show_arrow), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Set Toolbar Style:");
  g_signal_connect (checkbox, "toggled", G_CALLBACK (set_toolbar_style_toggled), toolbar);
  gtk_box_pack_start (GTK_BOX (hbox1), checkbox, FALSE, FALSE, 0);

  option_menu = gtk_combo_box_text_new ();
  gtk_widget_set_sensitive (option_menu, FALSE);  
  g_object_set_data (G_OBJECT (checkbox), "option-menu", option_menu);
  
  for (i = 0; i < G_N_ELEMENTS (toolbar_styles); i++)
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (option_menu), toolbar_styles[i]);
  gtk_combo_box_set_active (GTK_COMBO_BOX (option_menu),
                            gtk_toolbar_get_style (GTK_TOOLBAR (toolbar)));
  gtk_box_pack_start (GTK_BOX (hbox2), option_menu, FALSE, FALSE, 0);
  g_signal_connect (option_menu, "changed",
		    G_CALLBACK (change_toolbar_style), toolbar);

  checkbox = gtk_check_button_new_with_mnemonic("_Set Icon Size:"); 
  g_signal_connect (checkbox, "toggled", G_CALLBACK (set_icon_size_toggled), toolbar);
  gtk_box_pack_start (GTK_BOX (hbox2), checkbox, FALSE, FALSE, 0);

  option_menu = gtk_combo_box_text_new ();
  g_object_set_data (G_OBJECT (checkbox), "option-menu", option_menu);
  gtk_widget_set_sensitive (option_menu, FALSE);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (option_menu), "small toolbar");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (option_menu), "large toolbar");

  gtk_box_pack_start (GTK_BOX (hbox2), option_menu, FALSE, FALSE, 0);
  g_signal_connect (option_menu, "changed",
		    G_CALLBACK (icon_size_history_changed), toolbar);
  
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (scrolled_window, TRUE);
  gtk_widget_set_vexpand (scrolled_window, TRUE);
  gtk_grid_attach (GTK_GRID (grid), scrolled_window, 1, 3, 1, 1);

  store = create_items_list (&treeview);
  gtk_container_add (GTK_CONTAINER (scrolled_window), treeview);
  
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "document-new");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Custom label");
  add_item_to_list (store, item, "New");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gdk_threads_add_timeout (3000, (GSourceFunc) timeout_cb, item);
  gtk_tool_item_set_expand (item, TRUE);

  menu = gtk_menu_new ();
  for (i = 0; i < 20; i++)
    {
      char *text;
      text = g_strdup_printf ("Menuitem %d", i);
      menuitem = gtk_menu_item_new_with_label (text);
      g_free (text);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }

  item = gtk_menu_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "document-open");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Open");
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (item), menu);
  add_item_to_list (store, item, "Open");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gdk_threads_add_timeout (3000, (GSourceFunc) timeout_cb1, item);
 
  menu = gtk_menu_new ();
  for (i = 0; i < 20; i++)
    {
      char *text;
      text = g_strdup_printf ("A%d", i);
      menuitem = gtk_menu_item_new_with_label (text);
      g_free (text);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }

  item = gtk_menu_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-previous");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Back");
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (item), menu);
  add_item_to_list (store, item, "BackWithHistory");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
 
  item = gtk_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");    
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  
  image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
  item = gtk_tool_item_new ();
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (item), image);
  add_item_to_list (store, item, "(Custom Item)");    
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-previous");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Back");
  add_item_to_list (store, item, "Back");    
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");  
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  
  item = gtk_tool_button_new (NULL, NULL);
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "go-next");
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Forward");
  add_item_to_list (store, item, "Forward");  
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_toggle_tool_button_new ();
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Bold");
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "format-text-bold");
  g_signal_connect (item, "toggled", G_CALLBACK (bold_toggled), NULL);
  add_item_to_list (store, item, "Bold");  
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);

  item = gtk_separator_tool_item_new ();
  add_item_to_list (store, item, "-----");  
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_tool_item_set_expand (item, TRUE);
  gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (item), FALSE);
  g_assert (gtk_toolbar_get_nth_item (GTK_TOOLBAR (toolbar), 0) != 0);
  
  item = gtk_radio_tool_button_new (NULL);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Left");
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "format-justify-left");
  group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (item));
  add_item_to_list (store, item, "Left");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  
  
  item = gtk_radio_tool_button_new (group);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Center");
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "format-justify-center");
  group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (item));
  add_item_to_list (store, item, "Center");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_radio_tool_button_new (group);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), "Right");
  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), "format-justify-right");
  add_item_to_list (store, item, "Right");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_tool_button_new (gtk_image_new_from_file ("apple-red.png"), "_Apple");
  add_item_to_list (store, item, "Apple");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON (item), TRUE);

  gicon = g_content_type_get_icon ("video/ogg");
  image = gtk_image_new_from_gicon (gicon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  g_object_unref (gicon);
  item = gtk_tool_button_new (image, "Video");
  add_item_to_list (store, item, "Video");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  image = gtk_image_new_from_icon_name ("utilities-terminal", GTK_ICON_SIZE_LARGE_TOOLBAR);
  item = gtk_tool_button_new (image, "Terminal");
  add_item_to_list (store, item, "Terminal");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  image = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (image));
  item = gtk_tool_button_new (image, "Spinner");
  add_item_to_list (store, item, "Spinner");
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_grid_attach (GTK_GRID (grid), hbox, 1, 4, 1, 1);

  button = gtk_button_new_with_label ("Drag me to the toolbar");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  label = gtk_label_new ("Drop index:");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  checkbox = gtk_check_button_new_with_mnemonic("_Right to left");
  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), FALSE);
  g_signal_connect (checkbox, "toggled", G_CALLBACK (rtl_toggled), NULL);

  gtk_box_pack_end (GTK_BOX (hbox), checkbox, FALSE, FALSE, 0);
  
  gtk_drag_source_set (button, GDK_BUTTON1_MASK,
		       target_table, G_N_ELEMENTS (target_table),
		       GDK_ACTION_MOVE);
  gtk_drag_dest_set (toolbar, GTK_DEST_DEFAULT_DROP,
		     target_table, G_N_ELEMENTS (target_table),
		     GDK_ACTION_MOVE);
  g_signal_connect (toolbar, "drag_motion",
		    G_CALLBACK (toolbar_drag_motion), NULL);
  g_signal_connect (toolbar, "drag_leave",
		    G_CALLBACK (toolbar_drag_leave), NULL);
  g_signal_connect (toolbar, "drag_drop",
		    G_CALLBACK (toolbar_drag_drop), label);

  gtk_widget_show_all (window);

  g_signal_connect (window, "delete_event", G_CALLBACK (gtk_main_quit), NULL);
  
  g_signal_connect (toolbar, "popup_context_menu", G_CALLBACK (popup_context_menu), NULL);
  
  gtk_main ();
  
  return 0;
}
