/* Tree View/Filter Model
 *
 * This example demonstrates how GtkTreeModelFilter can be used
 * to compute columns that are not actually present in the underlying
 * model.
 */

#include <gtk/gtk.h>

enum {
  WIDTH_COLUMN,
  HEIGHT_COLUMN,
  AREA_COLUMN,
  SQUARE_COLUMN
};

static void
format_number (GtkTreeViewColumn *col,
               GtkCellRenderer   *cell,
               GtkTreeModel      *model,
               GtkTreeIter       *iter,
               gpointer           data)
{
  gint num;
  gchar *text;

  gtk_tree_model_get (model, iter, GPOINTER_TO_INT (data), &num, -1);
  text = g_strdup_printf ("%d", num);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
filter_modify_func (GtkTreeModel *model,
                    GtkTreeIter  *iter,
                    GValue       *value,
                    gint          column,
                    gpointer      data)
{
  GtkTreeModelFilter *filter_model = GTK_TREE_MODEL_FILTER (model);
  gint width, height;
  GtkTreeModel *child_model;
  GtkTreeIter child_iter;

  child_model = gtk_tree_model_filter_get_model (filter_model);
  gtk_tree_model_filter_convert_iter_to_child_iter (filter_model, &child_iter, iter);

  gtk_tree_model_get (child_model, &child_iter,
                      WIDTH_COLUMN, &width,
                      HEIGHT_COLUMN, &height,
                      -1);

  switch (column)
    {
    case WIDTH_COLUMN:
      g_value_set_int (value, width);
      break;
    case HEIGHT_COLUMN:
      g_value_set_int (value, height);
      break;
    case AREA_COLUMN:
      g_value_set_int (value, width * height);
      break;
    case SQUARE_COLUMN:
      g_value_set_boolean (value, width == height);
      break;
    default:
      g_assert_not_reached ();
    }
}

GtkWidget *
do_filtermodel (GtkWidget *do_widget)
{
  static GtkWidget *window;
  GtkWidget *tree;
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  GType types[4];

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Filter Model");
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      tree = gtk_tree_view_new ();
      gtk_container_add (GTK_CONTAINER (window), tree);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, "Width");
      cell = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_start (column, cell, FALSE);
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (WIDTH_COLUMN), NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, "Height");
      cell = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_start (column, cell, FALSE);
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (HEIGHT_COLUMN), NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, "Area");
      cell = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_start (column, cell, FALSE);
      gtk_tree_view_column_set_cell_data_func (column, cell,
                                               format_number, GINT_TO_POINTER (AREA_COLUMN), NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_title (column, "Square");
      cell = gtk_cell_renderer_pixbuf_new ();
      g_object_set (cell, "icon-name", "object-select-symbolic", NULL);
      gtk_tree_view_column_pack_start (column, cell, FALSE);
      gtk_tree_view_column_add_attribute (column, cell, "visible", SQUARE_COLUMN);
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);

      store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_INT);
      store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_INT);
      gtk_list_store_insert_with_values (store, NULL, -1,
                                         WIDTH_COLUMN, 10,
                                         HEIGHT_COLUMN, 20,
                                         -1);
      gtk_list_store_insert_with_values (store, NULL, -1,
                                         WIDTH_COLUMN, 5,
                                         HEIGHT_COLUMN, 25,
                                         -1);
      gtk_list_store_insert_with_values (store, NULL, -1,
                                         WIDTH_COLUMN, 15,
                                         HEIGHT_COLUMN, 15,
                                         -1);

      types[WIDTH_COLUMN] = G_TYPE_INT;
      types[HEIGHT_COLUMN] = G_TYPE_INT;
      types[AREA_COLUMN] = G_TYPE_INT;
      types[SQUARE_COLUMN] = G_TYPE_BOOLEAN;
      model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
      gtk_tree_model_filter_set_modify_func (GTK_TREE_MODEL_FILTER (model),
                                             G_N_ELEMENTS (types), types,
                                             filter_modify_func, NULL, NULL);

      gtk_tree_view_set_model (GTK_TREE_VIEW (tree), model);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
