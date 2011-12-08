/* testmerge.c
 * Copyright (C) 2003 James Henstridge
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <gtk/gtk.h>

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1 
#endif

struct { const gchar *filename; guint merge_id; } merge_ids[] = {
  { "merge-1.ui", 0 },
  { "merge-2.ui", 0 },
  { "merge-3.ui", 0 }
};

static void
dump_tree (GtkWidget    *button, 
	   GtkUIManager *merge)
{
  gchar *dump;

  dump = gtk_ui_manager_get_ui (merge);
  g_message ("%s", dump);
  g_free (dump);
}

static void
dump_accels (void)
{
  gtk_accel_map_save_fd (STDOUT_FILENO);
}

static void
print_toplevel (GtkWidget *widget, gpointer user_data)
{
  g_print ("%s\n", G_OBJECT_TYPE_NAME (widget));
}

static void
dump_toplevels (GtkWidget    *button, 
		GtkUIManager *merge)
{
  GSList *toplevels;

  toplevels = gtk_ui_manager_get_toplevels (merge, 
					    GTK_UI_MANAGER_MENUBAR |
					    GTK_UI_MANAGER_TOOLBAR |
					    GTK_UI_MANAGER_POPUP);

  g_slist_foreach (toplevels, (GFunc) print_toplevel, NULL);
  g_slist_free (toplevels);
}

static void
toggle_tearoffs (GtkWidget    *button, 
		 GtkUIManager *merge)
{
  gboolean add_tearoffs;

  add_tearoffs = gtk_ui_manager_get_add_tearoffs (merge);
  
  gtk_ui_manager_set_add_tearoffs (merge, !add_tearoffs);
}

static gint
delayed_toggle_dynamic (GtkUIManager *merge)
{
  GtkAction *dyn;
  static GtkActionGroup *dynamic = NULL;
  static guint merge_id = 0;

  if (!dynamic)
    {
      dynamic = gtk_action_group_new ("dynamic");
      gtk_ui_manager_insert_action_group (merge, dynamic, 0);
      dyn = g_object_new (GTK_TYPE_ACTION,
			  "name", "dyn1",
			  "label", "Dynamic action 1",
			  "stock_id", GTK_STOCK_COPY,
			  NULL);
      gtk_action_group_add_action (dynamic, dyn);
      dyn = g_object_new (GTK_TYPE_ACTION,
			  "name", "dyn2",
			  "label", "Dynamic action 2",
			  "stock_id", GTK_STOCK_EXECUTE,
			  NULL);
      gtk_action_group_add_action (dynamic, dyn);
    }
  
  if (merge_id == 0)
    {
      merge_id = gtk_ui_manager_new_merge_id (merge);
      gtk_ui_manager_add_ui (merge, merge_id, "/toolbar1/ToolbarPlaceholder", 
			     "dyn1", "dyn1", 0, 0);
      gtk_ui_manager_add_ui (merge, merge_id, "/toolbar1/ToolbarPlaceholder", 
			     "dynsep", NULL, GTK_UI_MANAGER_SEPARATOR, 0);
      gtk_ui_manager_add_ui (merge, merge_id, "/toolbar1/ToolbarPlaceholder", 
			     "dyn2", "dyn2", 0, 0);

      gtk_ui_manager_add_ui (merge, merge_id, "/menubar/EditMenu", 
			     "dyn1menu", "dyn1", GTK_UI_MANAGER_MENU, 0);
      gtk_ui_manager_add_ui (merge, merge_id, "/menubar/EditMenu/dyn1menu", 
			     "dyn1", "dyn1", GTK_UI_MANAGER_MENUITEM, 0);
      gtk_ui_manager_add_ui (merge, merge_id, "/menubar/EditMenu/dyn1menu/dyn1", 
			     "dyn2", "dyn2", GTK_UI_MANAGER_AUTO, FALSE);
    }
  else 
    {
      gtk_ui_manager_remove_ui (merge, merge_id);
      merge_id = 0;
    }

  return FALSE;
}

static void
toggle_dynamic (GtkWidget    *button, 
		GtkUIManager *merge)
{
  gdk_threads_add_timeout (2000, (GSourceFunc)delayed_toggle_dynamic, merge);
}

static void
activate_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("Action %s (type=%s) activated", name, typename);
}

static void
toggle_action (GtkAction *action)
{
  const gchar *name = gtk_action_get_name (action);
  const gchar *typename = G_OBJECT_TYPE_NAME (action);

  g_message ("ToggleAction %s (type=%s) toggled (active=%d)", name, typename,
	     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}


static void
radio_action_changed (GtkAction *action, GtkRadioAction *current)
{
  g_message ("RadioAction %s (type=%s) activated (active=%d) (value %d)", 
	     gtk_action_get_name (GTK_ACTION (current)), 
	     G_OBJECT_TYPE_NAME (GTK_ACTION (current)),
	     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (current)),
	     gtk_radio_action_get_current_value (current));
}

static GtkActionEntry entries[] = {
  { "FileMenuAction", NULL, "_File" },
  { "EditMenuAction", NULL, "_Edit" },
  { "HelpMenuAction", NULL, "_Help" },
  { "JustifyMenuAction", NULL, "_Justify" },
  { "EmptyMenu1Action", NULL, "Empty 1" },
  { "EmptyMenu2Action", NULL, "Empty 2" },
  { "Test", NULL, "Test" },

  { "QuitAction",  GTK_STOCK_QUIT,  NULL,     "<control>q", "Quit", G_CALLBACK (gtk_main_quit) },
  { "NewAction",   GTK_STOCK_NEW,   NULL,     "<control>n", "Create something", G_CALLBACK (activate_action) },
  { "New2Action",  GTK_STOCK_NEW,   NULL,     "<control>m", "Create something else", G_CALLBACK (activate_action) },
  { "OpenAction",  GTK_STOCK_OPEN,  NULL,     NULL,         "Open it", G_CALLBACK (activate_action) },
  { "CutAction",   GTK_STOCK_CUT,   NULL,     "<control>x", "Knive", G_CALLBACK (activate_action) },
  { "CopyAction",  GTK_STOCK_COPY,  NULL,     "<control>c", "Copy", G_CALLBACK (activate_action) },
  { "PasteAction", GTK_STOCK_PASTE, NULL,     "<control>v", "Paste", G_CALLBACK (activate_action) },
  { "AboutAction", NULL,            "_About", NULL,         "About", G_CALLBACK (activate_action) },
};
static guint n_entries = G_N_ELEMENTS (entries);

static GtkToggleActionEntry toggle_entries[] = {
  { "BoldAction",  GTK_STOCK_BOLD,  "_Bold",  "<control>b", "Make it bold", G_CALLBACK (toggle_action), 
    TRUE },
};
static guint n_toggle_entries = G_N_ELEMENTS (toggle_entries);

enum {
  JUSTIFY_LEFT,
  JUSTIFY_CENTER,
  JUSTIFY_RIGHT,
  JUSTIFY_FILL
};

static GtkRadioActionEntry radio_entries[] = {
  { "justify-left", GTK_STOCK_JUSTIFY_LEFT, NULL, "<control>L", 
    "Left justify the text", JUSTIFY_LEFT },
  { "justify-center", GTK_STOCK_JUSTIFY_CENTER, NULL, "<super>E",
    "Center justify the text", JUSTIFY_CENTER },
  { "justify-right", GTK_STOCK_JUSTIFY_RIGHT, NULL, "<hyper>R",
    "Right justify the text", JUSTIFY_RIGHT },
  { "justify-fill", GTK_STOCK_JUSTIFY_FILL, NULL, "<super><hyper>J",
    "Fill justify the text", JUSTIFY_FILL },
};
static guint n_radio_entries = G_N_ELEMENTS (radio_entries);

static void
add_widget (GtkUIManager *merge, 
	    GtkWidget    *widget, 
	    GtkBox       *box)
{
  GtkWidget *handle_box;

  if (GTK_IS_TOOLBAR (widget))
    {
      handle_box = gtk_handle_box_new ();
      gtk_widget_show (handle_box);
      gtk_container_add (GTK_CONTAINER (handle_box), widget);
      gtk_box_pack_start (box, handle_box, FALSE, FALSE, 0);
      g_signal_connect_swapped (widget, "destroy", 
				G_CALLBACK (gtk_widget_destroy), handle_box);
    }
  else
    gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
    
  gtk_widget_show (widget);
}

static void
toggle_merge (GtkWidget    *button, 
	      GtkUIManager *merge)
{
  gint mergenum;

  mergenum = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "mergenum"));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      GError *err = NULL;

      g_message ("merging %s", merge_ids[mergenum].filename);
      merge_ids[mergenum].merge_id =
	gtk_ui_manager_add_ui_from_file (merge, merge_ids[mergenum].filename, &err);
      if (err != NULL)
	{
	  GtkWidget *dialog;

	  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (button)),
					   0, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
					   "could not merge %s: %s", merge_ids[mergenum].filename,
					   err->message);

	  g_signal_connect (dialog, "response", G_CALLBACK (gtk_object_destroy), NULL);
	  gtk_widget_show (dialog);

	  g_clear_error (&err);
	}
    }
  else
    {
      g_message ("unmerging %s (merge_id=%u)", merge_ids[mergenum].filename,
		 merge_ids[mergenum].merge_id);
      gtk_ui_manager_remove_ui (merge, merge_ids[mergenum].merge_id);
    }
}

static void  
set_name_func (GtkTreeViewColumn *tree_column,
	       GtkCellRenderer   *cell,
	       GtkTreeModel      *tree_model,
	       GtkTreeIter       *iter,
	       gpointer           data)
{
  GtkAction *action;
  char *name;
  
  gtk_tree_model_get (tree_model, iter, 0, &action, -1);
  g_object_get (action, "name", &name, NULL);
  g_object_set (cell, "text", name, NULL);
  g_free (name);
  g_object_unref (action);
}

static void
set_sensitive_func (GtkTreeViewColumn *tree_column,
		    GtkCellRenderer   *cell,
		    GtkTreeModel      *tree_model,
		    GtkTreeIter       *iter,
		    gpointer           data)
{
  GtkAction *action;
  gboolean sensitive;
  
  gtk_tree_model_get (tree_model, iter, 0, &action, -1);
  g_object_get (action, "sensitive", &sensitive, NULL);
  g_object_set (cell, "active", sensitive, NULL);
  g_object_unref (action);
}


static void
set_visible_func (GtkTreeViewColumn *tree_column,
		  GtkCellRenderer   *cell,
		  GtkTreeModel      *tree_model,
		  GtkTreeIter       *iter,
		  gpointer           data)
{
  GtkAction *action;
  gboolean visible;
  
  gtk_tree_model_get (tree_model, iter, 0, &action, -1);
  g_object_get (action, "visible", &visible, NULL);
  g_object_set (cell, "active", visible, NULL);
  g_object_unref (action);
}

static void
sensitivity_toggled (GtkCellRendererToggle *cell, 
		     const gchar           *path_str,
		     GtkTreeModel          *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkAction *action;
  gboolean sensitive;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &action, -1);
  g_object_get (action, "sensitive", &sensitive, NULL);
  g_object_set (action, "sensitive", !sensitive, NULL);
  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static void
visibility_toggled (GtkCellRendererToggle *cell, 
		    const gchar           *path_str, 
		    GtkTreeModel          *model)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkAction *action;
  gboolean visible;

  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, 0, &action, -1);
  g_object_get (action, "visible", &visible, NULL);
  g_object_set (action, "visible", !visible, NULL);
  gtk_tree_model_row_changed (model, path, &iter);
  gtk_tree_path_free (path);
}

static gint
iter_compare_func (GtkTreeModel *model, 
		   GtkTreeIter  *a, 
		   GtkTreeIter  *b,
		   gpointer      user_data)
{
  GValue a_value = { 0, }, b_value = { 0, };
  GtkAction *a_action, *b_action;
  const gchar *a_name, *b_name;
  gint retval = 0;

  gtk_tree_model_get_value (model, a, 0, &a_value);
  gtk_tree_model_get_value (model, b, 0, &b_value);
  a_action = GTK_ACTION (g_value_get_object (&a_value));
  b_action = GTK_ACTION (g_value_get_object (&b_value));

  a_name = gtk_action_get_name (a_action);
  b_name = gtk_action_get_name (b_action);
  if (a_name == NULL && b_name == NULL) 
    retval = 0;
  else if (a_name == NULL)
    retval = -1;
  else if (b_name == NULL) 
    retval = 1;
  else 
    retval = strcmp (a_name, b_name);

  g_value_unset (&b_value);
  g_value_unset (&a_value);

  return retval;
}

static GtkWidget *
create_tree_view (GtkUIManager *merge)
{
  GtkWidget *tree_view, *sw;
  GtkListStore *store;
  GList *p;
  GtkCellRenderer *cell;
  
  store = gtk_list_store_new (1, GTK_TYPE_ACTION);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), 0,
				   iter_compare_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0,
					GTK_SORT_ASCENDING);
  
  for (p = gtk_ui_manager_get_action_groups (merge); p; p = p->next)
    {
      GList *actions, *l;

      actions = gtk_action_group_list_actions (p->data);

      for (l = actions; l; l = l->next)
	{
	  GtkTreeIter iter;

	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter, 0, l->data, -1);
	}

      g_list_free (actions);
    }
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);

  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Action",
					      gtk_cell_renderer_text_new (),
					      set_name_func, NULL, NULL);

  gtk_tree_view_column_set_sort_column_id (gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0), 0);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (sensitivity_toggled), store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Sensitive",
					      cell,
					      set_sensitive_func, NULL, NULL);

  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell, "toggled", G_CALLBACK (visibility_toggled), store);
  gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (tree_view),
					      -1, "Visible",
					      cell,
					      set_visible_func, NULL, NULL);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);
  
  return sw;
}

static gboolean
area_press (GtkWidget      *drawing_area,
	    GdkEventButton *event,
	    GtkUIManager   *merge)
{
  gtk_widget_grab_focus (drawing_area);

  if (event->button == 3 &&
      event->type == GDK_BUTTON_PRESS)
    {
      GtkWidget *menu = gtk_ui_manager_get_widget (merge, "/FileMenu");
      
      if (GTK_IS_MENU (menu)) 
	{
	  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			  NULL, drawing_area,
			  3, event->time);
	  return TRUE;
	}
    }

  return FALSE;
  
}

static void
activate_path (GtkWidget      *button,
	       GtkUIManager   *merge)
{
  GtkAction *action = gtk_ui_manager_get_action (merge, 
						 "/menubar/HelpMenu/About");
  if (action)
    gtk_action_activate (action);
  else 
    g_message ("no action found");
}

typedef struct _ActionStatus ActionStatus;

struct _ActionStatus {
  GtkAction *action;
  GtkWidget *statusbar;
};

static void
action_status_destroy (gpointer data)
{
  ActionStatus *action_status = data;

  g_object_unref (action_status->action);
  g_object_unref (action_status->statusbar);

  g_free (action_status);
}

static void
set_tip (GtkWidget *widget)
{
  ActionStatus *data;
  gchar *tooltip;
  
  data = g_object_get_data (G_OBJECT (widget), "action-status");
  
  if (data) 
    {
      g_object_get (data->action, "tooltip", &tooltip, NULL);
      
      gtk_statusbar_push (GTK_STATUSBAR (data->statusbar), 0, 
			  tooltip ? tooltip : "");
      
      g_free (tooltip);
    }
}

static void
unset_tip (GtkWidget *widget)
{
  ActionStatus *data;

  data = g_object_get_data (G_OBJECT (widget), "action-status");

  if (data)
    gtk_statusbar_pop (GTK_STATUSBAR (data->statusbar), 0);
}
		    
static void
connect_proxy (GtkUIManager *merge,
	       GtkAction    *action,
	       GtkWidget    *proxy,
	       GtkWidget    *statusbar)
{
  if (GTK_IS_MENU_ITEM (proxy)) 
    {
      ActionStatus *data;

      data = g_object_get_data (G_OBJECT (proxy), "action-status");
      if (data)
	{
	  g_object_unref (data->action);
	  g_object_unref (data->statusbar);

	  data->action = g_object_ref (action);
	  data->statusbar = g_object_ref (statusbar);
	}
      else
	{
	  data = g_new0 (ActionStatus, 1);

	  data->action = g_object_ref (action);
	  data->statusbar = g_object_ref (statusbar);

	  g_object_set_data_full (G_OBJECT (proxy), "action-status", 
				  data, action_status_destroy);
	  
	  g_signal_connect (proxy, "select",  G_CALLBACK (set_tip), NULL);
	  g_signal_connect (proxy, "deselect", G_CALLBACK (unset_tip), NULL);
	}
    }
}

int
main (int argc, char **argv)
{
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkUIManager *merge;
  GtkWidget *window, *table, *frame, *menu_box, *vbox, *view;
  GtkWidget *button, *area, *statusbar;
  gint i;
  
  gtk_init (&argc, &argv);

  action_group = gtk_action_group_new ("TestActions");
  gtk_action_group_add_actions (action_group, 
				entries, n_entries, 
				NULL);
  action = gtk_action_group_get_action (action_group, "EmptyMenu1Action");
  g_object_set (action, "hide_if_empty", FALSE, NULL);
  action = gtk_action_group_get_action (action_group, "EmptyMenu2Action");
  g_object_set (action, "hide_if_empty", TRUE, NULL);
  gtk_action_group_add_toggle_actions (action_group, 
				       toggle_entries, n_toggle_entries, 
				       NULL);
  gtk_action_group_add_radio_actions (action_group, 
				      radio_entries, n_radio_entries, 
				      JUSTIFY_RIGHT,
				      G_CALLBACK (radio_action_changed), NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 400);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  table = gtk_table_new (2, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_col_spacings (GTK_TABLE (table), 2);
  gtk_container_set_border_width (GTK_CONTAINER (table), 2);
  gtk_container_add (GTK_CONTAINER (window), table);

  frame = gtk_frame_new ("Menus and Toolbars");
  gtk_table_attach (GTK_TABLE (table), frame, 0,2, 1,2,
		    GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0);
  
  menu_box = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (menu_box), 2);
  gtk_container_add (GTK_CONTAINER (frame), menu_box);

  statusbar = gtk_statusbar_new ();
  gtk_box_pack_end (GTK_BOX (menu_box), statusbar, FALSE, FALSE, 0);
    
  area = gtk_drawing_area_new ();
  gtk_widget_set_events (area, GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_size_request (area, -1, 40);
  gtk_box_pack_end (GTK_BOX (menu_box), area, FALSE, FALSE, 0);
  gtk_widget_show (area);

  button = gtk_button_new ();
  gtk_box_pack_end (GTK_BOX (menu_box), button, FALSE, FALSE, 0);
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
			    gtk_action_group_get_action (action_group, "AboutAction"));

  gtk_widget_show (button);

  button = gtk_check_button_new ();
  gtk_box_pack_end (GTK_BOX (menu_box), button, FALSE, FALSE, 0);
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
			    gtk_action_group_get_action (action_group, "BoldAction"));
  gtk_widget_show (button);

  merge = gtk_ui_manager_new ();

  g_signal_connect (merge, "connect-proxy", G_CALLBACK (connect_proxy), statusbar);
  g_signal_connect (area, "button_press_event", G_CALLBACK (area_press), merge);

  gtk_ui_manager_insert_action_group (merge, action_group, 0);
  g_signal_connect (merge, "add_widget", G_CALLBACK (add_widget), menu_box);

  gtk_window_add_accel_group (GTK_WINDOW (window), 
			      gtk_ui_manager_get_accel_group (merge));
  
  frame = gtk_frame_new ("UI Files");
  gtk_table_attach (GTK_TABLE (table), frame, 0,1, 0,1,
		    GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

  vbox = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  for (i = 0; i < G_N_ELEMENTS (merge_ids); i++)
    {
      button = gtk_check_button_new_with_label (merge_ids[i].filename);
      g_object_set_data (G_OBJECT (button), "mergenum", GINT_TO_POINTER (i));
      g_signal_connect (button, "toggled", G_CALLBACK (toggle_merge), merge);
      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }

  button = gtk_check_button_new_with_label ("Tearoffs");
  g_signal_connect (button, "clicked", G_CALLBACK (toggle_tearoffs), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_check_button_new_with_label ("Dynamic");
  g_signal_connect (button, "clicked", G_CALLBACK (toggle_dynamic), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Activate path");
  g_signal_connect (button, "clicked", G_CALLBACK (activate_path), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Dump Tree");
  g_signal_connect (button, "clicked", G_CALLBACK (dump_tree), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Dump Toplevels");
  g_signal_connect (button, "clicked", G_CALLBACK (dump_toplevels), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("Dump Accels");
  g_signal_connect (button, "clicked", G_CALLBACK (dump_accels), NULL);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  view = create_tree_view (merge);
  gtk_table_attach (GTK_TABLE (table), view, 1,2, 0,1,
		    GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

  gtk_widget_show_all (window);
  gtk_main ();

#ifdef DEBUG_UI_MANAGER
  {
    GList *action;
    
    g_print ("\n> before unreffing the ui manager <\n");
    for (action = gtk_action_group_list_actions (action_group);
	 action; 
	 action = action->next)
      {
	GtkAction *a = action->data;
	g_print ("  action %s ref count %d\n", 
		 gtk_action_get_name (a), G_OBJECT (a)->ref_count);
      }
  }
#endif

  g_object_unref (merge);

#ifdef DEBUG_UI_MANAGER
  {
    GList *action;

    g_print ("\n> after unreffing the ui manager <\n");
    for (action = gtk_action_group_list_actions (action_group);
	 action; 
	 action = action->next)
      {
	GtkAction *a = action->data;
	g_print ("  action %s ref count %d\n", 
		 gtk_action_get_name (a), G_OBJECT (a)->ref_count);
      }
  }
#endif

  g_object_unref (action_group);

  return 0;
}
