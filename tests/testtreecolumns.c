#include <gtk/gtk.h>


GtkWidget *left_tree_view;
GtkWidget *top_right_tree_view;
GtkWidget *bottom_right_tree_view;
GtkTreeModel *left_tree_model;
GtkTreeModel *top_right_tree_model;
GtkTreeModel *bottom_right_tree_model;
GtkWidget *sample_tree_view_top;
GtkWidget *sample_tree_view_bottom;

static void
add_clicked (GtkWidget *button, gpointer data)
{
  static gint i = 0;

  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  gchar *label = g_strdup_printf ("Column %d", i);

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (label, cell, "text", 0, NULL);
  gtk_tree_view_column_set_reorderable (column, TRUE);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_RESIZEABLE);
  gtk_tree_view_column_set_clickable (column, FALSE);
  gtk_list_store_append (GTK_LIST_STORE (left_tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (left_tree_model), &iter, 0, label, 1, column, -1);
  g_free (label);
  i++;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));
  gtk_tree_selection_select_iter (selection, &iter);
}

static void
get_visible (GtkTreeViewColumn *tree_column,
	     GtkCellRenderer   *cell,
	     GtkTreeModel      *tree_model,
	     GtkTreeIter       *iter,
	     gpointer           data)
{
  GtkTreeViewColumn *column;

  gtk_tree_model_get (tree_model, iter, 1, &column, -1);
  if (column)
    {
      gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
					   column->visible);
    }
}

static void
set_visible (GtkCellRendererToggle *cell,
	     gchar                 *path_str,
	     gpointer               data)
{
  GtkTreeView *tree_view = (GtkTreeView *) data;
  GtkTreeViewColumn *column;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);

  model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 1, &column, -1);

  if (column)
    {
      gtk_tree_view_column_set_visible (column, ! gtk_tree_view_column_get_visible (column));
      gtk_tree_model_range_changed (model, path, &iter, path, &iter);
    }
  gtk_tree_path_free (path);
}

static void
add_left_clicked (GtkWidget *button, gpointer data)
{
  GtkTreeIter iter;
  gchar *label;
  GtkTreeViewColumn *column;

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data));

  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (gtk_tree_view_get_model (GTK_TREE_VIEW (data)),
		      &iter, 0, &label, 1, &column, -1);

  if (GTK_WIDGET (data) == top_right_tree_view)
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_top), column);
  else
    gtk_tree_view_remove_column (GTK_TREE_VIEW (sample_tree_view_bottom), column);

  gtk_list_store_remove (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (data))), &iter);

  /* Put it back on the left */
  gtk_list_store_append (GTK_LIST_STORE (left_tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (left_tree_model), &iter, 0, label, 1, column, -1);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));
  gtk_tree_selection_select_iter (selection, &iter);
  g_free (label);
}



static void
add_right_clicked (GtkWidget *button, gpointer data)
{
  GtkTreeIter iter;
  gchar *label;
  GtkTreeViewColumn *column;

  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view));

  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (GTK_TREE_MODEL (left_tree_model),
		      &iter, 0, &label, 1, &column, -1);
  gtk_list_store_remove (GTK_LIST_STORE (left_tree_model), &iter);

  gtk_list_store_append (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (data))), &iter);
  gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (data))), &iter, 0, label, 1, column, -1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data));
  gtk_tree_selection_select_iter (selection, &iter);

  if (GTK_WIDGET (data) == top_right_tree_view)
    gtk_tree_view_append_column (GTK_TREE_VIEW (sample_tree_view_top), column);
  else
    gtk_tree_view_append_column (GTK_TREE_VIEW (sample_tree_view_bottom), column);
  g_free (label);
}

static void
selection_changed (GtkTreeSelection *selection, GtkWidget *button)
{
  if (gtk_tree_selection_get_selected (selection, NULL, NULL))
    gtk_widget_set_sensitive (button, TRUE);
  else
    gtk_widget_set_sensitive (button, FALSE);
}


static GtkTargetEntry row_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, 0}
};

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *hbox, *vbox;
  GtkWidget *vbox2, *bbox;
  GtkWidget *button;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkWidget *swindow;
  GtkTreeModel *sample_model;
  gint i;

  gtk_init (&argc, &argv);

  /* First initialize all the models for signal purposes */
  left_tree_model = (GtkTreeModel *) gtk_list_store_new_with_types (2, G_TYPE_STRING, GTK_TYPE_POINTER);
  top_right_tree_model = (GtkTreeModel *) gtk_list_store_new_with_types (2, G_TYPE_STRING, GTK_TYPE_POINTER);
  bottom_right_tree_model = (GtkTreeModel *) gtk_list_store_new_with_types (2, G_TYPE_STRING, GTK_TYPE_POINTER);
  top_right_tree_view = gtk_tree_view_new_with_model (top_right_tree_model);
  bottom_right_tree_view = gtk_tree_view_new_with_model (bottom_right_tree_model);
  sample_model = (GtkTreeModel *) gtk_list_store_new_with_types (1, G_TYPE_STRING);

  for (i = 0; i < 10; i++)
    {
      GtkTreeIter iter;
      gchar *string = g_strdup_printf ("%d", i);
      gtk_list_store_append (GTK_LIST_STORE (sample_model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (sample_model), &iter, 0, string, -1);
      g_free (string);
    }

  /* Set up the test windows. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Top Window");
  swindow = gtk_scrolled_window_new (NULL, NULL);
  sample_tree_view_top = gtk_tree_view_new_with_model (sample_model);
  gtk_container_add (GTK_CONTAINER (window), swindow);
  gtk_container_add (GTK_CONTAINER (swindow), sample_tree_view_top);
  gtk_widget_show_all (window);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Bottom Window");
  swindow = gtk_scrolled_window_new (NULL, NULL);
  sample_tree_view_bottom = gtk_tree_view_new_with_model (sample_model);
  gtk_container_add (GTK_CONTAINER (window), swindow);
  gtk_container_add (GTK_CONTAINER (swindow), sample_tree_view_bottom);
  gtk_widget_show_all (window);

  /* Set up the main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 300);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  /* Left Pane */
  cell = gtk_cell_renderer_text_new ();

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  left_tree_view = gtk_tree_view_new_with_model (left_tree_model);
  gtk_container_add (GTK_CONTAINER (swindow), left_tree_view);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (left_tree_view), -1,
					       "Unattached Columns", cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (cell), "toggled", (GCallback) set_visible, left_tree_view, NULL, FALSE, FALSE);
  column = gtk_tree_view_column_new_with_attributes ("Visible", cell, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (left_tree_view), column);
  g_object_unref (G_OBJECT (column));
  gtk_tree_view_column_set_cell_data_func (column, get_visible, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), swindow, TRUE, TRUE, 0);

  /* Middle Pane */
  vbox2 = gtk_vbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);
  
  bbox = gtk_vbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), bbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("<<");
  gtk_widget_set_sensitive (button, FALSE);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_left_clicked), top_right_tree_view);
  gtk_signal_connect (GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (top_right_tree_view))),
		      "selection-changed", GTK_SIGNAL_FUNC (selection_changed), button);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label (">>");
  gtk_widget_set_sensitive (button, FALSE);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_right_clicked), top_right_tree_view);
  gtk_signal_connect (GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view))),
		      "selection-changed", GTK_SIGNAL_FUNC (selection_changed), button);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  bbox = gtk_vbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 0, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), bbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("<<");
  gtk_widget_set_sensitive (button, FALSE);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_left_clicked), bottom_right_tree_view);
  gtk_signal_connect (GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (bottom_right_tree_view))),
		      "selection-changed", GTK_SIGNAL_FUNC (selection_changed), button);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label (">>");
  gtk_widget_set_sensitive (button, FALSE);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_right_clicked), bottom_right_tree_view);
  gtk_signal_connect (GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view))),
		      "selection-changed", GTK_SIGNAL_FUNC (selection_changed), button);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  
  /* Right Pane */
  vbox2 = gtk_vbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (top_right_tree_view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (top_right_tree_view), -1,
					       NULL, cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (cell), "toggled", (GCallback) set_visible, top_right_tree_view, NULL, FALSE, FALSE);
  column = gtk_tree_view_column_new_with_attributes (NULL, cell, NULL);
  gtk_tree_view_column_set_cell_data_func (column, get_visible, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (top_right_tree_view), column);

  gtk_container_add (GTK_CONTAINER (swindow), top_right_tree_view);
  gtk_box_pack_start (GTK_BOX (vbox2), swindow, TRUE, TRUE, 0);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (bottom_right_tree_view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (bottom_right_tree_view), -1,
					       NULL, cell, "text", 0, NULL);
  cell = gtk_cell_renderer_toggle_new ();
  g_signal_connect_data (G_OBJECT (cell), "toggled", (GCallback) set_visible, bottom_right_tree_view, NULL, FALSE, FALSE);
  column = gtk_tree_view_column_new_with_attributes (NULL, cell, NULL);
  gtk_tree_view_column_set_cell_data_func (column, get_visible, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (bottom_right_tree_view), column);
  gtk_container_add (GTK_CONTAINER (swindow), bottom_right_tree_view);
  gtk_box_pack_start (GTK_BOX (vbox2), swindow, TRUE, TRUE, 0);

  
  /* Drag and Drop */
  gtk_tree_view_set_rows_drag_source (GTK_TREE_VIEW (left_tree_view),
                                      GDK_BUTTON1_MASK,
                                      row_targets,
                                      G_N_ELEMENTS (row_targets),
                                      GDK_ACTION_MOVE,
                                      NULL, NULL);
  gtk_tree_view_set_rows_drag_dest (GTK_TREE_VIEW (left_tree_view),
                                    row_targets,
                                    G_N_ELEMENTS (row_targets),
                                    GDK_ACTION_MOVE,
                                    NULL, NULL);

  gtk_tree_view_set_rows_drag_source (GTK_TREE_VIEW (top_right_tree_view),
                                      GDK_BUTTON1_MASK,
                                      row_targets,
                                      G_N_ELEMENTS (row_targets),
                                      GDK_ACTION_MOVE,
                                      NULL, NULL);
  gtk_tree_view_set_rows_drag_dest (GTK_TREE_VIEW (top_right_tree_view),
				    row_targets,
				    G_N_ELEMENTS (row_targets),
				    GDK_ACTION_MOVE,
				      NULL, NULL);

  gtk_tree_view_set_rows_drag_source (GTK_TREE_VIEW (bottom_right_tree_view),
                                      GDK_BUTTON1_MASK,
                                      row_targets,
                                      G_N_ELEMENTS (row_targets),
                                      GDK_ACTION_MOVE,
                                      NULL, NULL);
  gtk_tree_view_set_rows_drag_dest (GTK_TREE_VIEW (bottom_right_tree_view),
				    row_targets,
				    G_N_ELEMENTS (row_targets),
				    GDK_ACTION_MOVE,
				    NULL, NULL);


  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  button = gtk_button_new_with_label ("Add new Column");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_clicked), left_tree_model);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
