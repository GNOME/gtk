#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

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
  g_message (dump);
  g_free (dump);
}

static void
toggle_tearoffs (GtkWidget    *button, 
		 GtkUIManager *merge)
{
  gboolean add_tearoffs;

  add_tearoffs = gtk_ui_manager_get_add_tearoffs (merge);
  
  gtk_ui_manager_set_add_tearoffs (merge, !add_tearoffs);
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

  g_message ("Action %s (type=%s) activated (active=%d)", name, typename,
	     gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}


static void
radio_action_changed (GtkAction *action, GtkRadioAction *current)
{
  g_message ("Action %s (type=%s) activated (active=%d) (value %d)", 
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
  { "Test", NULL, "Test" },

  { "QuitAction",  GTK_STOCK_QUIT,  NULL, "<control>q", NULL, G_CALLBACK (gtk_main_quit) },
  { "NewAction",   GTK_STOCK_NEW,   NULL, "<control>n", NULL, G_CALLBACK (activate_action) },
  { "New2Action",  GTK_STOCK_NEW,   NULL, "<control>m", NULL, G_CALLBACK (activate_action) },
  { "OpenAction",  GTK_STOCK_OPEN,  NULL, "<control>o", NULL, G_CALLBACK (activate_action) },
  { "CutAction",   GTK_STOCK_CUT,   NULL, "<control>x", NULL, G_CALLBACK (activate_action) },
  { "CopyAction",  GTK_STOCK_COPY,  NULL, "<control>c", NULL, G_CALLBACK (activate_action) },
  { "PasteAction", GTK_STOCK_PASTE, NULL, "<control>v", NULL, G_CALLBACK (activate_action) },
  { "AboutAction", NULL,            "_About", NULL,     NULL, G_CALLBACK (activate_action) },
};
static guint n_entries = G_N_ELEMENTS (entries);

enum {
  JUSTIFY_LEFT,
  JUSTIFY_CENTER,
  JUSTIFY_RIGHT,
  JUSTIFY_FILL
};

static GtkRadioActionEntry radio_entries[] = {
  { "justify-left", GTK_STOCK_JUSTIFY_LEFT, NULL, "<control>L", 
    "Left justify the text", JUSTIFY_LEFT },
  { "justify-center", GTK_STOCK_JUSTIFY_CENTER, NULL, "<control>E",
    "Center justify the text", JUSTIFY_CENTER },
  { "justify-right", GTK_STOCK_JUSTIFY_RIGHT, NULL, "<control>R",
    "Right justify the text", JUSTIFY_RIGHT },
  { "justify-fill", GTK_STOCK_JUSTIFY_FILL, NULL, "<control>J",
    "Fill justify the text", JUSTIFY_FILL },
};
static guint n_radio_entries = G_N_ELEMENTS (radio_entries);

static void
add_widget (GtkUIManager *merge, 
	    GtkWidget    *widget, 
	    GtkBox       *box)
{
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

	  g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (gtk_object_destroy), NULL);
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
  g_object_get (G_OBJECT (action), "name", &name, NULL);
  g_object_set (G_OBJECT (cell), "text", name, NULL);
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
  g_object_get (G_OBJECT (action), "sensitive", &sensitive, NULL);
  g_object_set (G_OBJECT (cell), "active", sensitive, NULL);
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
  g_object_get (G_OBJECT (action), "visible", &visible, NULL);
  g_object_set (G_OBJECT (cell), "active", visible, NULL);
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
  g_object_get (G_OBJECT (action), "sensitive", &sensitive, NULL);
  g_object_set (G_OBJECT (action), "sensitive", !sensitive, NULL);
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
  g_object_get (G_OBJECT (action), "visible", &visible, NULL);
  g_object_set (G_OBJECT (action), "visible", !visible, NULL);
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

int
main (int argc, char **argv)
{
  GtkActionGroup *action_group;
  GtkUIManager *merge;
  GtkWidget *window, *table, *frame, *menu_box, *vbox, *view, *area;
  GtkWidget *button;
  gint i;
  
  gtk_init (&argc, &argv);

  action_group = gtk_action_group_new ("TestActions");
  gtk_action_group_add_actions (action_group, entries, n_entries, NULL);
  gtk_action_group_add_radio_actions (action_group, radio_entries, n_radio_entries, 
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
  
  area = gtk_drawing_area_new ();
  gtk_widget_set_events (area, GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_size_request (area, -1, 40);
  gtk_box_pack_end (GTK_BOX (menu_box), area, FALSE, FALSE, 0);
  gtk_widget_show (area);

  merge = gtk_ui_manager_new ();

  g_signal_connect (area, "button_press_event",
		    G_CALLBACK (area_press), merge);

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

  button = gtk_button_new_with_mnemonic ("_Dump Tree");
  g_signal_connect (button, "clicked", G_CALLBACK (dump_tree), merge);
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  view = create_tree_view (merge);
  gtk_table_attach (GTK_TABLE (table), view, 1,2, 0,1,
		    GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

  gtk_widget_show_all (window);
  gtk_main ();


  return 0;
}
