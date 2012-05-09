#include <gtk/gtk.h>

static GtkTreeModel *
create_treemodel (void)
{
  GtkListStore *store;
  char *all_the_words;
  char **words;
  guint i;

  store = gtk_list_store_new (1, G_TYPE_STRING);

  if (!g_file_get_contents ("/usr/share/dict/words", &all_the_words, NULL, NULL))
    return GTK_TREE_MODEL (store);

  words = g_strsplit (all_the_words, "\n", -1);
  g_free (all_the_words);

  for (i = 0; words[i] && i < 10; i++)
    {
      gtk_list_store_insert_with_values (store, NULL, -1, 0, words[i], -1);
    }

  return GTK_TREE_MODEL (store);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *list;
  GtkTreeModel *model;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  model = create_treemodel ();
  list = gtk_list_view_new_with_model (model);
  g_object_unref (model);

  gtk_container_add (GTK_CONTAINER (window), list);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

