#include <gtk/gtk.h>

GtkWidget *left_tree_view;
GtkWidget *right_tree_view;
GtkTreeModel *left_tree_model;
GtkTreeModel *right_tree_model;

static void
add_clicked (GtkWidget *button, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  static gint i = 0;

  gchar *label = g_strdup_printf ("Column %d", i);
  column = gtk_tree_view_column_new ();
  gtk_list_store_append (GTK_LIST_STORE (left_tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (left_tree_model), &iter, 0, label, 1, column, -1);
  g_free (label);
  i++;
}

static void
add_left_clicked (GtkWidget *button, gpointer data)
{

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

  gtk_list_store_append (GTK_LIST_STORE (right_tree_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (right_tree_model), &iter, 0, label, 1, column, -1);
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

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *hbox, *vbox, *bbox;
  GtkWidget *button;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GtkWidget *swindow;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 300);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (cell), "foreground", "black", NULL);
  left_tree_model = (GtkTreeModel *) gtk_list_store_new_with_types (2, G_TYPE_STRING, GTK_TYPE_POINTER);
  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  left_tree_view = gtk_tree_view_new_with_model (left_tree_model);
  gtk_container_add (GTK_CONTAINER (swindow), left_tree_view);
  column = gtk_tree_view_column_new_with_attributes ("Unattached Columns", cell, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (left_tree_view), column);
  gtk_box_pack_start (GTK_BOX (hbox), swindow, TRUE, TRUE, 0);

  bbox = gtk_vbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_SPREAD);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (bbox), 0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("<<");
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_left_clicked), NULL);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label (">>");
  gtk_widget_set_sensitive (button, FALSE);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (add_right_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (left_tree_view))),
		      "selection-changed", GTK_SIGNAL_FUNC (selection_changed), button);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

  swindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  right_tree_model = (GtkTreeModel *) gtk_list_store_new_with_types (2, G_TYPE_STRING, GTK_TYPE_POINTER);
  right_tree_view = gtk_tree_view_new_with_model (right_tree_model);
  column = gtk_tree_view_column_new_with_attributes ("Unattached Columns", cell, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (right_tree_view), column);
  gtk_container_add (GTK_CONTAINER (swindow), right_tree_view);
  gtk_box_pack_start (GTK_BOX (hbox), swindow, TRUE, TRUE, 0);

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
