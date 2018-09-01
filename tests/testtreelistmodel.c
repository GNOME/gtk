#include <gtk/gtk.h>

static GListModel *
create_list_model_for_directory (gpointer file,
                                 gpointer unused)
{
  GFileEnumerator *enumerate;
  GListStore *store;
  GFile *child;
  GFileInfo *info;

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
  gtk_style_context_add_class (gtk_widget_get_style_context (child), "expander");
  gtk_container_add (GTK_CONTAINER (box), child);

  file = gtk_tree_list_row_get_item (item);
  child = gtk_label_new (g_file_get_basename (file));
  g_object_unref (file);

  gtk_container_add (GTK_CONTAINER (box), child);

  return row;
}

int
main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *sw;
  GtkWidget *listbox;
  GListModel *model, *dirmodel;
  GFile *root;

  gtk_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), win);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (win), sw);

  listbox = gtk_list_box_new ();
  gtk_container_add (GTK_CONTAINER (sw), listbox);

  root = g_file_new_for_path (g_get_current_dir ());
  dirmodel = create_list_model_for_directory (root, NULL);
  model = G_LIST_MODEL (gtk_tree_list_model_new (FALSE,
                                                 dirmodel,
                                                 FALSE,
                                                 create_list_model_for_directory,
                                                 NULL, NULL));
  g_object_unref (dirmodel);
  gtk_list_box_bind_model (GTK_LIST_BOX (listbox),
                           model,
                           create_widget_for_model,
                           root, g_object_unref);
  g_object_set_data (G_OBJECT (listbox), "model", model);
  g_object_unref (model);

  gtk_widget_show (win);

  gtk_main ();

  return 0;
}
