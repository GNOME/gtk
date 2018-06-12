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

      if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
        continue;

      child = g_file_get_child (file, g_file_info_get_name (info));
      g_list_store_append (store, child);
      g_object_unref (child);
    }

  g_object_unref (enumerate);

  return G_LIST_MODEL (store);
}

static GtkTreeList *
get_tree_list (GtkWidget *row)
{
  return GTK_TREE_LIST (g_object_get_data (G_OBJECT (gtk_widget_get_parent (row)), "model"));
}

static void
expand_clicked (GtkWidget *button,
                GtkWidget *row)
{
  gtk_tree_list_set_expanded (get_tree_list (row),
                              gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)),
                              TRUE);
}

static void
collapse_clicked (GtkWidget *button,
                GtkWidget *row)
{
  gtk_tree_list_set_expanded (get_tree_list (row),
                              gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)),
                              FALSE);
}

static GtkWidget *
create_widget_for_model (gpointer file,
                         gpointer root)
{
  GtkWidget *row, *box, *child;
  GFile *iter;
  guint depth;

  row = gtk_list_box_row_new ();

  depth = 0;
  for (iter = g_object_ref (g_file_get_parent (file));
       !g_file_equal (root, iter);
       g_set_object (&iter, g_file_get_parent (iter)))
    {
      g_object_unref (iter);
      depth++;
    }
  g_object_unref (iter);
  g_object_unref (iter);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);

  if (depth > 0)
    {
      child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request (child, 16 * depth, 0);
      gtk_container_add (GTK_CONTAINER (box), child);
    }

  child = gtk_button_new_from_icon_name ("list-remove-symbolic");
  gtk_button_set_relief (GTK_BUTTON (child), GTK_RELIEF_NONE);
  g_signal_connect (child, "clicked", G_CALLBACK (collapse_clicked), row);
  gtk_container_add (GTK_CONTAINER (box), child);

  child = gtk_button_new_from_icon_name ("list-add-symbolic");
  gtk_button_set_relief (GTK_BUTTON (child), GTK_RELIEF_NONE);
  g_signal_connect (child, "clicked", G_CALLBACK (expand_clicked), row);
  gtk_container_add (GTK_CONTAINER (box), child);

  child = gtk_label_new (g_file_get_basename (file));
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
  model = gtk_tree_list_new (dirmodel,
                             create_list_model_for_directory,
                             NULL, NULL);
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
