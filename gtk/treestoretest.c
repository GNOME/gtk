#include <gtk/gtk.h>
#include <stdlib.h>

GtkObject *model;

static void
selection_changed (GtkTreeSelection *selection,
		   GtkWidget        *button)
{
  if (gtk_tree_selection_get_selected (selection,
				       NULL))
    gtk_widget_set_sensitive (button, TRUE);
  else
    gtk_widget_set_sensitive (button, FALSE);
}

static void
node_set (GtkTreeIter *iter)
{
  static gint i = 0;
  gchar *str;

  str = g_strdup_printf ("FOO: %d", i++);
  gtk_tree_store_iter_set (GTK_TREE_STORE (model), iter, 0, str, -1);
  g_free (str);

}

static void
iter_remove (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter selected;
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_remove (GTK_TREE_STORE (model),
				  &selected);
    }
}

static void
iter_insert (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkWidget *entry;
  GtkTreeIter iter;
  GtkTreeIter selected;

  entry = gtk_object_get_user_data (GTK_OBJECT (button));
  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_insert (GTK_TREE_STORE (model),
				  &iter,
				  &selected,
				  atoi (gtk_entry_get_text (GTK_ENTRY (entry))));
    }
  else
    {
      gtk_tree_store_iter_insert (GTK_TREE_STORE (model),
				  &iter,
				  NULL,
				  atoi (gtk_entry_get_text (GTK_ENTRY (entry))));
    }

  node_set (&iter);
}

static void
iter_insert_before  (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_insert_before (GTK_TREE_STORE (model),
					 &iter,
					 NULL,
					 &selected);
    }
  else
    {
      gtk_tree_store_iter_insert_before (GTK_TREE_STORE (model),
					 &iter,
					 NULL,
					 NULL);
    }

  node_set (&iter);
}

static void
iter_insert_after (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_insert_after (GTK_TREE_STORE (model),
					 &iter,
					 NULL,
					 &selected);
    }
  else
    {
      gtk_tree_store_iter_insert_after (GTK_TREE_STORE (model),
					&iter,
					NULL,
					&selected);
    }

  node_set (&iter);
}

static void
iter_prepend (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_prepend (GTK_TREE_STORE (model),
				   &iter,
				   &selected);
    }
  else
    {
      gtk_tree_store_iter_prepend (GTK_TREE_STORE (model),
				   &iter,
				   NULL);
    }

  node_set (&iter);
}

static void
iter_append (GtkWidget *button, GtkTreeView *tree_view)
{
  GtkTreeIter iter;
  GtkTreeIter selected;

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)),
				       &selected))
    {
      gtk_tree_store_iter_append (GTK_TREE_STORE (model),
				  &iter,
				  &selected);
    }
  else
    {
      gtk_tree_store_iter_append (GTK_TREE_STORE (model),
				  &iter,
				  NULL);
    }

  node_set (&iter);
}

static void
make_window ()
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox, *entry;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkObject *column;
  GtkCellRenderer *cell;
  GtkObject *selection;

  /* Make the Widgets/Objects */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 350);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  selection = GTK_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view)));
  gtk_tree_selection_set_type (GTK_TREE_SELECTION (selection), GTK_TREE_SELECTION_SINGLE);

  /* Put them together */
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);

  /* buttons */
  button = gtk_button_new_with_label ("gtk_tree_store_iter_remove");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_remove, tree_view);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_iter_insert");
  hbox = gtk_hbox_new (FALSE, 8);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  gtk_object_set_user_data (GTK_OBJECT (button), entry);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_insert, tree_view);

  
  button = gtk_button_new_with_label ("gtk_tree_store_iter_insert_before");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_insert_before, tree_view);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_iter_insert_after");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_insert_after, tree_view);
  gtk_signal_connect (GTK_OBJECT (selection),
		      "selection_changed",
		      selection_changed,
		      button);
  gtk_widget_set_sensitive (button, FALSE);

  button = gtk_button_new_with_label ("gtk_tree_store_iter_prepend");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_prepend, tree_view);

  button = gtk_button_new_with_label ("gtk_tree_store_iter_append");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (button), "clicked", iter_append, tree_view);

  /* The selected column */
  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("nodes", cell, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), GTK_TREE_VIEW_COLUMN (column));

  /* A few to start */
  iter_prepend (NULL, GTK_TREE_VIEW (tree_view));
  iter_prepend (NULL, GTK_TREE_VIEW (tree_view));
  iter_prepend (NULL, GTK_TREE_VIEW (tree_view));

  /* Show it all */
  gtk_widget_show_all (window);
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  model = gtk_tree_store_new_with_values (2, G_TYPE_STRING, G_TYPE_STRING);

  make_window ();
  make_window ();

  gtk_main ();

  return 0;
}
