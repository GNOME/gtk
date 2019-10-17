#include <gtk/gtk.h>

#define ROWS 30

GSList *pending = NULL;
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
                                         "," G_FILE_ATTRIBUTE_STANDARD_ICON
                                         "," G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         0,
                                         NULL,
                                         &error);

  if (enumerate == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TOO_MANY_OPEN_FILES) && active)
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
      g_object_set_data_full (G_OBJECT (info), "file", child, g_object_unref);
      g_ptr_array_add (array, info);
    }
  g_list_free (files);

  g_list_store_splice (store, g_list_model_get_n_items (store), 0, array->pdata, array->len);
  g_ptr_array_unref (array);

  g_file_enumerator_next_files_async (G_FILE_ENUMERATOR (enumerate),
                                      g_file_is_native (file) ? 5000 : 100,
                                      G_PRIORITY_DEFAULT_IDLE,
                                      NULL,
                                      got_files,
                                      store);
}

static int
compare_files (gconstpointer first,
               gconstpointer second,
               gpointer unused)
{
  GFile *first_file, *second_file;
  char *first_path, *second_path;
  int result;
#if 0
  GFileType first_type, second_type;

  /* This is a bit slow, because each g_file_query_file_type() does a stat() */
  first_type = g_file_query_file_type (G_FILE (first), G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);
  second_type = g_file_query_file_type (G_FILE (second), G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

  if (first_type == G_FILE_TYPE_DIRECTORY && second_type != G_FILE_TYPE_DIRECTORY)
    return -1;
  if (first_type != G_FILE_TYPE_DIRECTORY && second_type == G_FILE_TYPE_DIRECTORY)
    return 1;
#endif

  first_file = g_object_get_data (G_OBJECT (first), "file");
  second_file = g_object_get_data (G_OBJECT (second), "file");
  first_path = g_file_get_path (first_file);
  second_path = g_file_get_path (second_file);

  result = g_ascii_strcasecmp (first_path, second_path);

  g_free (first_path);
  g_free (second_path);

  return result;
}

static GListModel *
create_list_model_for_directory (gpointer file)
{
  GtkSortListModel *sort;
  GListStore *store;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  store = g_list_store_new (G_TYPE_FILE_INFO);
  g_object_set_data_full (G_OBJECT (store), "file", g_object_ref (file), g_object_unref);

  if (!start_enumerate (store))
    return NULL;

  sort = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                  compare_files,
                                  NULL, NULL);
  g_object_unref (store);
  return G_LIST_MODEL (sort);
}

static GtkWidget *
create_widget_for_model (gpointer item,
                         gpointer root)
{
  GtkWidget *row, *box, *child;
  GFileInfo *info;
  GFile *file;
  guint depth;
  GIcon *icon;

  row = gtk_list_box_row_new ();
  gtk_widget_set_vexpand (row, TRUE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
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

  info = gtk_tree_list_row_get_item (item);

  icon = g_file_info_get_icon (info);
  if (icon)
    {
      child = gtk_image_new_from_gicon (icon);
      gtk_container_add (GTK_CONTAINER (box), child);
    }

  file = g_object_get_data (G_OBJECT (info), "file");
  child = gtk_label_new (g_file_get_basename (file));
  g_object_unref (info);

  gtk_container_add (GTK_CONTAINER (box), child);

  return row;
}

static GListModel *
create_list_model_for_file_info (gpointer file_info,
                                 gpointer unused)
{
  GFile *file = g_object_get_data (file_info, "file");

  if (file == NULL)
    return NULL;

  return create_list_model_for_directory (file);
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

static gboolean
update_statusbar (GtkStatusbar *statusbar)
{
  GListModel *model = g_object_get_data (G_OBJECT (statusbar), "model");
  GString *string = g_string_new (NULL);
  guint n;
  gboolean result = G_SOURCE_REMOVE;

  gtk_statusbar_remove_all (statusbar, 0);

  n = g_list_model_get_n_items (model);
  g_string_append_printf (string, "%u", n);
  if (GTK_IS_FILTER_LIST_MODEL (model))
    {
      guint n_unfiltered = g_list_model_get_n_items (gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (model)));
      if (n != n_unfiltered)
        g_string_append_printf (string, "/%u", n_unfiltered);
    }
  g_string_append (string, " items");

  if (pending || active)
    {
      g_string_append_printf (string, " (%u directories remaining)", active + g_slist_length (pending));
      result = G_SOURCE_CONTINUE;
    }

  gtk_statusbar_push (statusbar, 0, string->str);
  g_free (string->str);

  return result;
}

static gboolean
match_file (gpointer item, gpointer data)
{
  GtkWidget *search_entry = data;
  GFileInfo *info = gtk_tree_list_row_get_item (item);
  GFile *file = g_object_get_data (G_OBJECT (info), "file");
  char *path;
  gboolean result;
  
  path = g_file_get_path (file);

  result = strstr (path, gtk_editable_get_text (GTK_EDITABLE (search_entry))) != NULL;

  g_object_unref (info);
  g_free (path);

  return result;
}

int
main (int argc, char *argv[])
{
  GtkAdjustment *adjustment;
  GtkWidget *win, *hbox, *vbox, *scrollbar, *listbox, *search_entry, *statusbar;
  GListModel *dirmodel;
  GtkTreeListModel *tree;
  GtkFilterListModel *filter;
  GtkSliceListModel *slice;
  GFile *root;

  gtk_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), win);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  search_entry = gtk_search_entry_new ();
  gtk_container_add (GTK_CONTAINER (vbox), search_entry);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (search_entry), hbox);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  listbox = gtk_list_box_new ();
  gtk_widget_set_hexpand (listbox, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), listbox);

  if (argc > 1)
    root = g_file_new_for_commandline_arg (argv[1]);
  else
    root = g_file_new_for_path (g_get_current_dir ());
  dirmodel = create_list_model_for_directory (root);
  tree = gtk_tree_list_model_new (FALSE,
                                  dirmodel,
                                  TRUE,
                                  create_list_model_for_file_info,
                                  NULL, NULL);
  g_object_unref (dirmodel);

  filter = gtk_filter_list_model_new (G_LIST_MODEL (tree),
                                      match_file,
                                      search_entry,
                                      NULL);
  g_signal_connect_swapped (search_entry, "search-changed", G_CALLBACK (gtk_filter_list_model_refilter), filter);
  slice = gtk_slice_list_model_new (G_LIST_MODEL (filter), 0, ROWS);

  gtk_list_box_bind_model (GTK_LIST_BOX (listbox),
                           G_LIST_MODEL (slice),
                           create_widget_for_model,
                           root, g_object_unref);

  adjustment = gtk_adjustment_new (0,
                                   0, MAX (g_list_model_get_n_items (G_LIST_MODEL (tree)), ROWS), 
                                   1, ROWS - 1,
                                   ROWS);
  g_object_bind_property (adjustment, "value", slice, "offset", 0);
  g_signal_connect (filter, "items-changed", G_CALLBACK (update_adjustment), adjustment);

  scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adjustment);
  gtk_container_add (GTK_CONTAINER (hbox), scrollbar);

  statusbar = gtk_statusbar_new ();
  gtk_widget_add_tick_callback (statusbar, (GtkTickCallback) update_statusbar, NULL, NULL);
  g_object_set_data (G_OBJECT (statusbar), "model", filter);
  g_signal_connect_swapped (filter, "items-changed", G_CALLBACK (update_statusbar), statusbar);
  update_statusbar (GTK_STATUSBAR (statusbar));
  gtk_container_add (GTK_CONTAINER (vbox), statusbar);

  g_object_unref (tree);
  g_object_unref (filter);
  g_object_unref (slice);

  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
