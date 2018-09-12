#include <gtk/gtk.h>

#define ROWS 30

GSList *pending;
guint active = 0;

static void
got_files (GObject      *enumerate,
           GAsyncResult *res,
           gpointer      store);

static gboolean
start_enumerate (GListStore *store)
{
  GFileEnumerator *enumerate;
  GFile *file = g_object_get_data (G_OBJECT (store), "file");
  GError *error = NULL;

  enumerate = g_file_enumerate_children (file,
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE
                                         "," G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         0,
                                         NULL,
                                         &error);

  if (enumerate == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TOO_MANY_OPEN_FILES))
        {
          g_clear_error (&error);
          pending = g_slist_prepend (pending, g_object_ref (store));
          return TRUE;
        }

      g_clear_error (&error);
      g_object_unref (store);
      return FALSE;
    }

  if (active > 20)
    {
      g_object_unref (enumerate);
      pending = g_slist_prepend (pending, g_object_ref (store));
      return TRUE;
    }

  active++;
  g_file_enumerator_next_files_async (enumerate,
                                      g_file_is_native (file) ? 5000 : 100,
                                      G_PRIORITY_DEFAULT_IDLE,
                                      NULL,
                                      got_files,
                                      g_object_ref (store));

  g_object_unref (enumerate);
  return TRUE;
}

static void
got_files (GObject      *enumerate,
           GAsyncResult *res,
           gpointer      store)
{
  GList *l, *files;
  GFile *file = g_object_get_data (store, "file");
  GPtrArray *array;

  files = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (enumerate), res, NULL);
  if (files == NULL)
    {
      g_object_unref (store);
      if (pending)
        {
          GListStore *store = pending->data;
          pending = g_slist_remove (pending, store);
          start_enumerate (store);
        }
      active--;
      return;
    }

  array = g_ptr_array_new ();
  g_ptr_array_new_with_free_func (g_object_unref);
  for (l = files; l; l = l->next)
    {
      GFileInfo *info = l->data;
      GFile *child;

      child = g_file_get_child (file, g_file_info_get_name (info));
      g_ptr_array_add (array, child);
    }
  g_list_free_full (files, g_object_unref);

  g_list_store_splice (store, g_list_model_get_n_items (store), 0, array->pdata, array->len);
  g_ptr_array_unref (array);

  g_file_enumerator_next_files_async (G_FILE_ENUMERATOR (enumerate),
                                      g_file_is_native (file) ? 5000 : 100,
                                      G_PRIORITY_DEFAULT_IDLE,
                                      NULL,
                                      got_files,
                                      store);
}

static GListModel *
create_list_model_for_directory (gpointer file,
                                 gpointer unused)
{
  GListStore *store;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  store = g_list_store_new (G_TYPE_FILE);
  g_object_set_data_full (G_OBJECT (store), "file", g_object_ref (file), g_object_unref);

  if (start_enumerate (store))
    return G_LIST_MODEL (store);
  else
    return NULL;
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
