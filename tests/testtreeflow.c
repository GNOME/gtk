#include <gtk/gtk.h>

GtkTreeModel *model = NULL;
GRand *rand = NULL;

enum
{
  TEXT_COLUMN,
  NUM_COLUMNS
};

static char *words[] =
{
  "Boom",
  "Borp",
  "Multiline\ntext",
  "Bingo",
  "Veni\nVedi\nVici",
  NULL
};


#define NUM_WORDS 5
#define NUM_ROWS 100


static void
initialize_model (void)
{
  gint i;
  GtkTreeIter iter;

  model = (GtkTreeModel *) gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING);
  rand = g_rand_new ();
  for (i = 0; i < NUM_ROWS; i++)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  TEXT_COLUMN, words[g_rand_int_range (rand, 0, NUM_WORDS)],
			  -1);
    }
}

static void
futz_row (void)
{
  gint i;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter iter2;

  i = g_rand_int_range (rand, 0,
			gtk_tree_model_iter_n_children (model, NULL));
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  switch (g_rand_int_range (rand, 0, 3))
    {
    case 0:
      /* insert */
            gtk_list_store_insert_after (GTK_LIST_STORE (model),
            				   &iter2, &iter);
            gtk_list_store_set (GTK_LIST_STORE (model), &iter2,
            			  TEXT_COLUMN, words[g_rand_int_range (rand, 0, NUM_WORDS)],
            			  -1);
      break;
    case 1:
      /* delete */
      if (gtk_tree_model_iter_n_children (model, NULL) == 0)
	return;
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
      break;
    case 2:
      /* modify */
      if (gtk_tree_model_iter_n_children (model, NULL) == 0)
	return;
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
      			  TEXT_COLUMN, words[g_rand_int_range (rand, 0, NUM_WORDS)],
      			  -1);
      break;
    }
}

static gboolean
futz (void)
{
  gint i;

  for (i = 0; i < 15; i++)
    futz_row ();
  g_print ("Number of rows: %d\n", gtk_tree_model_iter_n_children (model, NULL));
  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkWidget *hbox;
  GtkWidget *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Reflow test");
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);
  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new ("Incremental Reflow Test"), FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
  
  initialize_model ();
  tree_view = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1,
					       NULL,
					       gtk_cell_renderer_text_new (),
					       "text", TEXT_COLUMN,
					       NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  hbox = gtk_hbox_new (FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  button = gtk_button_new_with_mnemonic ("<b>_Futz!!</b>");
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (GTK_BIN (button)->child), TRUE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (futz), NULL);
  g_signal_connect (G_OBJECT (button), "realize", G_CALLBACK (gtk_widget_grab_focus), NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);
  gtk_widget_show_all (window);
  g_timeout_add (1000, (GSourceFunc) futz, NULL);
  gtk_main ();
  return 0;
}
