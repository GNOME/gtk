#include <gtk/gtk.h>

typedef GtkListStore MyModel;
typedef GtkListStoreClass MyModelClass;

static void my_model_drag_source_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (MyModel, my_model, GTK_TYPE_LIST_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                my_model_drag_source_init))

static void
my_model_class_init (MyModelClass *class)
{
}

static void
my_model_init (MyModel *object)
{
  GType types[1] = { G_TYPE_STRING };

  gtk_list_store_set_column_types (GTK_LIST_STORE (object), G_N_ELEMENTS (types), types);
}

static gboolean
my_model_drag_data_get (GtkTreeDragSource *source,
                        GtkTreePath       *path,
                        GtkSelectionData  *data)
{
  GtkTreeIter iter;
  gchar *text;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (source), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (source), &iter, 0, &text, -1);
  gtk_selection_data_set_text (data, text, -1);
  g_free (text);

  return TRUE;
}

static void
my_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
  static GtkTreeDragSourceIface *parent;

  parent = g_type_interface_peek_parent (iface);

  iface->row_draggable = parent->row_draggable;
  iface->drag_data_delete = parent->drag_data_delete;
  iface->drag_data_get = my_model_drag_data_get;
}

static GtkTreeModel *
get_model (void)
{
  MyModel *model;

  model = g_object_new (my_model_get_type (), NULL);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 1", -1);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 2", -1);
  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, -1, 0, "Item 3", -1);

  return GTK_TREE_MODEL (model);
}

static GtkTargetEntry entries[] = {
  { "text/plain", 0, 0 }
};

static GtkWidget *
get_dragsource (void)
{
  GtkTreeView *tv;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  tv = (GtkTreeView*) gtk_tree_view_new ();
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Text", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (tv, column);

  gtk_tree_view_set_model (tv, get_model ());
  gtk_tree_view_enable_model_drag_source (tv, GDK_BUTTON1_MASK, entries, G_N_ELEMENTS (entries), GDK_ACTION_COPY);

  return GTK_WIDGET (tv);
}

static void
data_received (GtkWidget *widget,
               GdkDragContext *context,
               gint x, gint y,
               GtkSelectionData *selda,
               guint info, guint time,
               gpointer dada)
{
  gchar *text;

  text = (gchar*) gtk_selection_data_get_text (selda);
  gtk_label_set_label (GTK_LABEL (widget), text);
  g_free (text);
}

static GtkWidget *
get_droptarget (void)
{
  GtkWidget *label;

  label = gtk_label_new ("Drop here");
  gtk_drag_dest_set (label, GTK_DEST_DEFAULT_ALL, entries, G_N_ELEMENTS (entries), GDK_ACTION_COPY);
  g_signal_connect (label, "drag-data-received", G_CALLBACK (data_received), NULL);

  return label;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), get_dragsource ());
  gtk_container_add (GTK_CONTAINER (box), get_droptarget ());

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
