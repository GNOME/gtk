/* Tree View/Editable Cells
 *
 * This demo demonstrates the use of editable cells in a GtkTreeView. If
 * you're new to the GtkTreeView widgets and associates, look into
 * the GtkListStore example first.
 *
 */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

static GtkWidget *window = NULL;

typedef struct
{
  gint     number;
  gchar   *product;
  gboolean editable;
}
Item;

enum
{
  COLUMN_NUMBER,
  COLUMN_PRODUCT,
  COLUMN_EDITABLE,
  NUM_COLUMNS
};

static GArray *articles = NULL;

static void
add_items (void)
{
  Item foo;

  g_return_if_fail (articles != NULL);

  foo.number = 3;
  foo.product = g_strdup ("bottles of coke");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 5;
  foo.product = g_strdup ("packages of noodles");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 2;
  foo.product = g_strdup ("packages of chocolate chip cookies");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 1;
  foo.product = g_strdup ("can vanilla ice cream");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);

  foo.number = 6;
  foo.product = g_strdup ("eggs");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);
}

static GtkTreeModel *
create_model (void)
{
  gint i = 0;
  GtkListStore *model;
  GtkTreeIter iter;

  /* create array */
  articles = g_array_sized_new (FALSE, FALSE, sizeof (Item), 1);

  add_items ();

  /* create list store */
  model = gtk_list_store_new (NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING,
			      G_TYPE_BOOLEAN);

  /* add items */
  for (i = 0; i < articles->len; i++)
    {
      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
			  COLUMN_NUMBER,
			  g_array_index (articles, Item, i).number,
			  COLUMN_PRODUCT,
			  g_array_index (articles, Item, i).product,
			  COLUMN_EDITABLE,
			  g_array_index (articles, Item, i).editable,
			  -1);
    }

  return GTK_TREE_MODEL (model);
}

static void
add_item (GtkWidget *button, gpointer data)
{
  Item foo;
  GtkTreeIter iter;
  GtkTreeModel *model = (GtkTreeModel *)data;

  g_return_if_fail (articles != NULL);

  foo.number = 0;
  foo.product = g_strdup ("Description here");
  foo.editable = TRUE;
  g_array_append_vals (articles, &foo, 1);

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      COLUMN_NUMBER, foo.number,
		      COLUMN_PRODUCT, foo.product,
		      COLUMN_EDITABLE, foo.editable,
		      -1);
}

static void
remove_item (GtkWidget *widget, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeView *treeview = (GtkTreeView *)data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      g_array_remove_index (articles, i);

      gtk_tree_path_free (path);
    }
}

static void
cell_edited (GtkCellRendererText *cell,
	     const gchar         *path_string,
	     const gchar         *new_text,
	     gpointer             data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

  gtk_tree_model_get_iter (model, &iter, path);

  switch (column)
    {
    case COLUMN_NUMBER:
      {
	gint i;

	i = gtk_tree_path_get_indices (path)[0];
	g_array_index (articles, Item, i).number = atoi (new_text);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
			    g_array_index (articles, Item, i).number, -1);
      }
      break;

    case COLUMN_PRODUCT:
      {
	gint i;
	gchar *old_text;

        gtk_tree_model_get (model, &iter, column, &old_text, -1);
	g_free (old_text);

	i = gtk_tree_path_get_indices (path)[0];
	g_free (g_array_index (articles, Item, i).product);
	g_array_index (articles, Item, i).product = g_strdup (new_text);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
                            g_array_index (articles, Item, i).product, -1);
      }
      break;
    }

  gtk_tree_path_free (path);
}

static void
add_columns (GtkTreeView *treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);

  /* number column */
  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (cell_edited), model);
  g_object_set_data (G_OBJECT (renderer), "column", (gint *)COLUMN_NUMBER);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
					       -1, "Number", renderer,
					       "text", COLUMN_NUMBER,
					       "editable", COLUMN_EDITABLE,
					       NULL);

  /* product column */
  renderer = gtk_cell_renderer_text_new ();
  g_signal_connect (renderer, "edited",
		    G_CALLBACK (cell_edited), model);
  g_object_set_data (G_OBJECT (renderer), "column", (gint *)COLUMN_PRODUCT);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
					       -1, "Product", renderer,
					       "text", COLUMN_PRODUCT,
					       "editable", COLUMN_EDITABLE,
					       NULL);
}

GtkWidget *
do_editable_cells (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox;
      GtkWidget *hbox;
      GtkWidget *sw;
      GtkWidget *treeview;
      GtkWidget *button;
      GtkTreeModel *model;

      /* create window, etc */
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
			     gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Shopping list");
      gtk_container_set_border_width (GTK_CONTAINER (window), 5);
      g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      gtk_box_pack_start (GTK_BOX (vbox),
			  gtk_label_new ("Shopping list (you can edit the cells!)"),
			  FALSE, FALSE, 0);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					   GTK_SHADOW_ETCHED_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

      /* create model */
      model = create_model ();

      /* create tree view */
      treeview = gtk_tree_view_new_with_model (model);
      g_object_unref (model);
      gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
      gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
				   GTK_SELECTION_SINGLE);

      add_columns (GTK_TREE_VIEW (treeview));

      gtk_container_add (GTK_CONTAINER (sw), treeview);

      /* some buttons */
      hbox = gtk_hbox_new (TRUE, 4);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      button = gtk_button_new_with_label ("Add item");
      g_signal_connect (button, "clicked",
			G_CALLBACK (add_item), model);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      button = gtk_button_new_with_label ("Remove item");
      g_signal_connect (button, "clicked",
			G_CALLBACK (remove_item), treeview);
      gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

      gtk_window_set_default_size (GTK_WINDOW (window), 320, 200);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
