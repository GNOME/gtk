#include <gtk/gtk.h>

#define ROWS 30

static GListModel *
create_list_model_for_directory (gpointer file,
                                 gpointer unused)
{
  GFileEnumerator *enumerate;
  GListStore *store;
  GFile *child;
  GFileInfo *info;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  enumerate = g_file_enumerate_children (file,
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE
                                         "," G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         0,
                                         NULL,
                                         NULL);
  if (enumerate == NULL)
    return NULL;

  store = g_list_store_new (G_TYPE_FILE);

  while (g_file_enumerator_iterate (enumerate, &info, NULL, NULL, NULL))
    {
      if (info == NULL)
        break;

      child = g_file_get_child (file, g_file_info_get_name (info));
      g_list_store_append (store, child);
      g_object_unref (child);
    }

  g_object_unref (enumerate);

  return G_LIST_MODEL (store);
}

static GtkWidget *
create_widget_for_model (gpointer item,
                         gpointer root)
{
  GtkWidget *row, *box, *child;
  GFile *file;
  guint depth;

  row = gtk_list_box_row_new ();
  gtk_widget_set_vexpand (row, TRUE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);

  depth = gtk_tree_list_row_get_depth (item);
  if (depth > 0)
    {
      child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request (child, 16 * depth, 0);
      gtk_container_add (GTK_CONTAINER (box), child);
    }

  if (gtk_tree_list_row_is_expandable (item))
    {
      GtkWidget *title, *arrow;

      child = g_object_new (GTK_TYPE_BOX, "css-name", "expander", NULL);
      
      title = g_object_new (GTK_TYPE_TOGGLE_BUTTON, "css-name", "title", NULL);
      gtk_button_set_relief (GTK_BUTTON (title), GTK_RELIEF_NONE);
      g_object_bind_property (item, "expanded", title, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      g_object_set_data_full (G_OBJECT (title), "make-sure-its-not-unreffed", g_object_ref (item), g_object_unref);
      gtk_container_add (GTK_CONTAINER (child), title);

      arrow = g_object_new (GTK_TYPE_SPINNER, "css-name", "arrow", NULL);
      gtk_container_add (GTK_CONTAINER (title), arrow);
    }
  else
    {
      child = gtk_image_new (); /* empty whatever */
    }
  gtk_container_add (GTK_CONTAINER (box), child);

  file = gtk_tree_list_row_get_item (item);
  child = gtk_label_new (g_file_get_basename (file));
  g_object_unref (file);

  gtk_container_add (GTK_CONTAINER (box), child);

  return row;
}

static void
update_adjustment (GListModel    *model,
                   guint          position,
                   guint          removed,
                   guint          added,
                   GtkAdjustment *adjustment)
{
  gtk_adjustment_set_upper (adjustment, MAX (ROWS, g_list_model_get_n_items (model)));
}

int
main (int argc, char *argv[])
{
  GtkAdjustment *adjustment;
  GtkWidget *win, *box, *scrollbar, *listbox;
  GListModel *dirmodel;
  GtkTreeListModel *tree;
  GtkSliceListModel *slice;
  GFile *root;

  gtk_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), win);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (win), box);

  listbox = gtk_list_box_new ();
  gtk_widget_set_hexpand (listbox, TRUE);
  gtk_container_add (GTK_CONTAINER (box), listbox);

  if (argc > 1)
    root = g_file_new_for_commandline_arg (argv[1]);
  else
    root = g_file_new_for_path (g_get_current_dir ());
  dirmodel = create_list_model_for_directory (root, NULL);
  tree = gtk_tree_list_model_new (FALSE,
                                  dirmodel,
                                  TRUE,
                                  create_list_model_for_directory,
                                  NULL, NULL);
  g_object_unref (dirmodel);

  slice = gtk_slice_list_model_new (G_LIST_MODEL (tree), 0, ROWS);

  gtk_list_box_bind_model (GTK_LIST_BOX (listbox),
                           G_LIST_MODEL (slice),
                           create_widget_for_model,
                           root, g_object_unref);

  adjustment = gtk_adjustment_new (0,
                                   0, MAX (g_list_model_get_n_items (G_LIST_MODEL (tree)), ROWS), 
                                   1, ROWS - 1,
                                   ROWS);
  g_object_bind_property (adjustment, "value", slice, "offset", 0);
  g_signal_connect (tree, "items-changed", G_CALLBACK (update_adjustment), adjustment);

  scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_container_add (GTK_CONTAINER (box), scrollbar);

  g_object_unref (tree);
  g_object_unref (slice);

  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
