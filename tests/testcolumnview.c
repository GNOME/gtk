#include <gtk/gtk.h>

GSList *pending = NULL;
guint active = 0;

static void
loading_cb (GtkDirectoryList *dir,
            GParamSpec       *pspec,
            gpointer          unused)
{
  if (gtk_directory_list_is_loading (dir))
    {
      active++;
      /* HACK: ensure loading finishes and the dir doesn't get destroyed */
      g_object_ref (dir);
    }
  else
    {
      active--;
      g_object_unref (dir);

      while (active < 20 && pending)
        {
          GtkDirectoryList *dir2 = pending->data;
          pending = g_slist_remove (pending, dir2);
          gtk_directory_list_set_file (dir2, g_object_get_data (G_OBJECT (dir2), "file"));
          g_object_unref (dir2);
        }
    }
}

static GtkDirectoryList *
create_directory_list (GFile *file)
{
  GtkDirectoryList *dir;

  dir = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_TYPE
                                "," G_FILE_ATTRIBUTE_STANDARD_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                NULL);
  gtk_directory_list_set_io_priority (dir, G_PRIORITY_DEFAULT_IDLE);
  g_signal_connect (dir, "notify::loading", G_CALLBACK (loading_cb), NULL);
  g_assert (!gtk_directory_list_is_loading (dir));

  if (active > 20)
    {
      g_object_set_data_full (G_OBJECT (dir), "file", g_object_ref (file), g_object_unref);
      pending = g_slist_prepend (pending, g_object_ref (dir));
    }
  else
    {
      gtk_directory_list_set_file (dir, file);
    }

  return dir;
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

  first_file = G_FILE (g_file_info_get_attribute_object (G_FILE_INFO (first), "standard::file"));
  second_file = G_FILE (g_file_info_get_attribute_object (G_FILE_INFO (second), "standard::file"));
  first_path = g_file_get_path (first_file);
  second_path = g_file_get_path (second_file);

  result = strcasecmp (first_path, second_path);

  g_free (first_path);
  g_free (second_path);

  return result;
}

static GListModel *
create_list_model_for_directory (gpointer file)
{
  GtkSortListModel *sort;
  GtkDirectoryList *dir;

  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_DIRECTORY)
    return NULL;

  dir = create_directory_list(file);
  sort = gtk_sort_list_model_new (G_LIST_MODEL (dir),
                                  compare_files,
                                  NULL, NULL);
  g_object_unref (dir);
  return G_LIST_MODEL (sort);
}

#if 0
typedef struct _RowData RowData;
struct _RowData
{
  GtkWidget *expander;
  GtkWidget *icon;
  GtkWidget *name;
  GCancellable *cancellable;

  GtkTreeListRow *current_item;
};

static void row_data_notify_item (GtkListItem *item,
                                  GParamSpec  *pspec,
                                  RowData     *data);
static void
row_data_unbind (RowData *data)
{
  if (data->current_item == NULL)
    return;

  if (data->cancellable)
    {
      g_cancellable_cancel (data->cancellable);
      g_clear_object (&data->cancellable);
    }

  g_clear_object (&data->current_item);
}

static void
row_data_update_info (RowData   *data,
                      GFileInfo *info)
{
  GIcon *icon;
  const char *thumbnail_path;

  thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  if (thumbnail_path)
    {
      /* XXX: not async */
      GFile *thumbnail_file = g_file_new_for_path (thumbnail_path);
      icon = g_file_icon_new (thumbnail_file);
      g_object_unref (thumbnail_file);
    }
  else
    {
      icon = g_file_info_get_icon (info);
    }

  gtk_widget_set_visible (data->icon, icon != NULL);
  gtk_image_set_from_gicon (GTK_IMAGE (data->icon), icon);
}

static void
copy_attribute (GFileInfo   *to,
                GFileInfo   *from,
                const gchar *attribute)
{
  GFileAttributeType type;
  gpointer value;

  if (g_file_info_get_attribute_data (from, attribute, &type, &value, NULL))
    g_file_info_set_attribute (to, attribute, type, value);
}

static void
row_data_got_thumbnail_info_cb (GObject      *source,
                                GAsyncResult *res,
                                gpointer      _data)
{
  RowData *data = _data; /* invalid if operation was cancelled */
  GFile *file = G_FILE (source);
  GFileInfo *queried, *info;

  queried = g_file_query_info_finish (file, res, NULL);
  if (queried == NULL)
    return;

  /* now we know row is valid */

  info = gtk_tree_list_row_get_item (data->current_item);

  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
  copy_attribute (info, queried, G_FILE_ATTRIBUTE_STANDARD_ICON);

  g_object_unref (queried);

  row_data_update_info (data, info);
  
  g_clear_object (&data->cancellable);
}

static void
row_data_bind (RowData        *data,
               GtkTreeListRow *item)
{
  GFileInfo *info;

  row_data_unbind (data);

  if (item == NULL)
    return;

  data->current_item = g_object_ref (item);

  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (data->expander), item);

  info = gtk_tree_list_row_get_item (item);

  if (!g_file_info_has_attribute (info, "filechooser::queried"))
    {
      data->cancellable = g_cancellable_new ();
      g_file_info_set_attribute_boolean (info, "filechooser::queried", TRUE);
      g_file_query_info_async (G_FILE (g_file_info_get_attribute_object (info, "standard::file")),
                               G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
                               G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
                               G_FILE_ATTRIBUTE_STANDARD_ICON,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               data->cancellable,
                               row_data_got_thumbnail_info_cb,
                               data);
    }

  row_data_update_info (data, info);

  gtk_label_set_label (GTK_LABEL (data->name), g_file_info_get_display_name (info));

  g_object_unref (info);
}

static void
row_data_notify_item (GtkListItem *item,
                      GParamSpec  *pspec,
                      RowData     *data)
{
  row_data_bind (data, gtk_list_item_get_item (item));
}

static void
row_data_free (gpointer _data)
{
  RowData *data = _data;

  row_data_unbind (data);

  g_slice_free (RowData, data);
}

static void
setup_widget (GtkListItem *list_item,
              gpointer     unused)
{
  GtkWidget *box, *child;
  RowData *data;

  data = g_slice_new0 (RowData);
  g_signal_connect (list_item, "notify::item", G_CALLBACK (row_data_notify_item), data);
  g_object_set_data_full (G_OBJECT (list_item), "row-data", data, row_data_free);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add (GTK_CONTAINER (list_item), box);

  child = gtk_label_new (NULL);
  gtk_label_set_width_chars (GTK_LABEL (child), 5);
  gtk_label_set_xalign (GTK_LABEL (child), 1.0);
  g_object_bind_property (list_item, "position", child, "label", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), child);

  data->expander = gtk_tree_expander_new ();
  gtk_container_add (GTK_CONTAINER (box), data->expander);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (data->expander), box);

  data->icon = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (box), data->icon);

  data->name = gtk_label_new (NULL);
  gtk_label_set_max_width_chars (GTK_LABEL (data->name), 25);
  gtk_label_set_ellipsize (GTK_LABEL (data->name), PANGO_ELLIPSIZE_END);
  gtk_container_add (GTK_CONTAINER (box), data->name);
}
#endif

static GListModel *
create_list_model_for_file_info (gpointer file_info,
                                 gpointer unused)
{
  GFile *file = G_FILE (g_file_info_get_attribute_object (file_info, "standard::file"));

  if (file == NULL)
    return NULL;

  return create_list_model_for_directory (file);
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
      result = G_SOURCE_CONTINUE;

  gtk_statusbar_push (statusbar, 0, string->str);
  g_free (string->str);

  return result;
}

static gboolean
match_file (gpointer item, gpointer data)
{
  GtkWidget *search_entry = data;
  GFileInfo *info = gtk_tree_list_row_get_item (item);
  GFile *file = G_FILE (g_file_info_get_attribute_object (info, "standard::file"));
  char *path;
  gboolean result;
  
  path = g_file_get_path (file);

  result = strstr (path, gtk_editable_get_text (GTK_EDITABLE (search_entry))) != NULL;

  g_object_unref (info);
  g_free (path);

  return result;
}

const char *ui_file =
"<?xml version='1.0' encoding='UTF-8'?>\n"
"<interface>\n"
"  <object class='GtkColumnView' id='view'>\n"
"    <child>\n"
"      <object class='GtkColumnViewColumn'>\n"
"        <property name='title'>Name</property>\n"
"        <property name='bytes'><![CDATA[\n"
"<?xml version='1.0' encoding='UTF-8'?>\n"
"<interface>\n"
"  <template class='GtkListItem'>\n"
"    <child>\n"
"      <object class='GtkBox'>\n"
"        <child>\n"
"          <object class='GtkImage'>\n"
"            <binding name='gicon'>file-info(GtkListItem.item:GFileInfo, 'standard::icon', object):GIcon</binding>\n"
"          </object>\n"
"        </child>\n"
"        <child>\n"
"          <object class='GtkLabel'>\n"
"            <property name='halign'>start</property>\n"
"            <binding name='label'>file-info(GtkListItem.item:GFileInfo, 'standard::display-name', string)</binding>\n"
"          </object>\n"
"        </child>\n"
"      </object>\n"
"    </child>\n"
"  </template>\n"
"</interface>\n"
"        </property>\n"
"      </object>\n"
"    </child>\n"
"  </object>\n"
"</interface>\n";

int
main (int argc, char *argv[])
{
  GtkWidget *win, *vbox, *sw, *view, *search_entry, *statusbar;
  GListModel *dirmodel;
  GtkTreeListModel *tree;
  GtkFilterListModel *filter;
  GFile *root;
  GtkBuilder *builder;

  gtk_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 800, 600);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), win);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  search_entry = gtk_search_entry_new ();
  gtk_container_add (GTK_CONTAINER (vbox), search_entry);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (search_entry), sw);
  gtk_container_add (GTK_CONTAINER (vbox), sw);

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, ui_file, -1, NULL);
  view = GTK_WIDGET (gtk_builder_get_object (builder, "view"));
  gtk_container_add (GTK_CONTAINER (sw), view);
  g_object_unref (builder);

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
  g_object_unref (root);

  filter = gtk_filter_list_model_new (G_LIST_MODEL (tree),
                                      match_file,
                                      search_entry,
                                      NULL);
  g_signal_connect_swapped (search_entry, "search-changed", G_CALLBACK (gtk_filter_list_model_refilter), filter);

  gtk_column_view_set_model (GTK_COLUMN_VIEW (view), G_LIST_MODEL (filter));

  statusbar = gtk_statusbar_new ();
  gtk_widget_add_tick_callback (statusbar, (GtkTickCallback) update_statusbar, NULL, NULL);
  g_object_set_data (G_OBJECT (statusbar), "model", filter);
  g_signal_connect_swapped (filter, "items-changed", G_CALLBACK (update_statusbar), statusbar);
  update_statusbar (GTK_STATUSBAR (statusbar));
  gtk_container_add (GTK_CONTAINER (vbox), statusbar);

  g_object_unref (tree);
  g_object_unref (filter);

  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
