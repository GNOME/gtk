#include <gtk/gtk.h>

typedef struct {
  const gchar *string;
  gboolean is_editable;
} ListEntry;

enum {
  STRING_COLUMN,
  IS_EDITABLE_COLUMN,
  NUM_COLUMNS
};

static ListEntry model_strings[] =
{
  {"A simple string", TRUE },
  {"Another string!", TRUE },
  {"Guess what, a third string. This one can't be edited", FALSE },
  {"And then a fourth string. Neither can this", FALSE },
  {"Multiline\nFun!", TRUE },
  { NULL }
};

static GtkTreeModel *
create_model (void)
{
  GtkListStore *model;
  GtkTreeIter iter;
  gint i;
  
  model = gtk_list_store_new (NUM_COLUMNS,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN);

  for (i = 0; model_strings[i].string != NULL; i++)
    {
      gtk_list_store_append (model, &iter);

      gtk_list_store_set (model, &iter,
			  STRING_COLUMN, model_strings[i].string,
			  IS_EDITABLE_COLUMN, model_strings[i].is_editable,
			  -1);
    }
  
  return GTK_TREE_MODEL (model);
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
  gtk_tree_model_get (model, &iter, IS_EDITABLE_COLUMN, &value, -1);

  value = !value;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, IS_EDITABLE_COLUMN, value, -1);

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
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, STRING_COLUMN, new_text, -1);

  gtk_tree_path_free (path);
}

gint
main (gint argc, gchar **argv)
{
  GtkWidget *window;
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeModel *tree_model;
  GtkCellRenderer *renderer;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GtkTreeView editing sample");
  gtk_signal_connect (GTK_OBJECT (window), "destroy", gtk_main_quit, NULL);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (window), scrolled_window);

  tree_model = create_model ();
  tree_view = gtk_tree_view_new_with_model (tree_model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "String",
					       renderer,
					       "text", STRING_COLUMN,
					       "editable", IS_EDITABLE_COLUMN,
					       NULL);

  g_signal_connect (G_OBJECT (renderer), "edited",
		    G_CALLBACK (edited), tree_model);
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (G_OBJECT (renderer), "toggled",
		    G_CALLBACK (toggled), tree_model);
  
  g_object_set (G_OBJECT (renderer),
		"xalign", 0.0,
		"mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
					       -1, "Editable",
					       renderer,
					       "active", IS_EDITABLE_COLUMN,
					       NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  
  gtk_window_set_default_size (GTK_WINDOW (window),
			       650, 400);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
