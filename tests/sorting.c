#include <gtk/gtk.h>

static GListModel *
get_model (guint size)
{
  GtkStringList *model;
  guint i;

  model = gtk_string_list_new (NULL);

  for (i = 0; i < size; i++)
    {
      char *string = g_strdup_printf ("%d", g_random_int_range (0, 1000000));
      gtk_string_list_append (model, string);
      g_free (string);
    }

  return G_LIST_MODEL (model);
}

static int
sort_func (gconstpointer a, gconstpointer b, gpointer data)
{
  return strcmp (gtk_string_object_get_string (GTK_STRING_OBJECT ((gpointer)a)),
                 gtk_string_object_get_string (GTK_STRING_OBJECT ((gpointer)b)));
}

static void
sorting_changed (GtkSor3ListModel *model)
{
  gboolean sorting;

  g_object_get (model, "sorting", &sorting, NULL);

  if (!sorting)
    exit (0);
}

static gboolean
start_sort (gpointer data)
{
  GListModel *model = data;
  GListModel *sort;
  GtkSorter *sorter;

  sorter = GTK_SORTER (gtk_custom_sorter_new (sort_func, NULL, NULL));

  if (g_getenv ("TIMSORT"))
    {
      sort = G_LIST_MODEL (gtk_tim2_sort_model_new (model, NULL));
      gtk_tim2_sort_model_set_sorter (GTK_TIM2_SORT_MODEL (sort), sorter);
    }
  else
    {
      sort = G_LIST_MODEL (gtk_sor3_list_model_new (model, NULL));
      gtk_sor3_list_model_set_sorter (GTK_SOR3_LIST_MODEL (sort), sorter);
    }

  g_signal_connect (sort, "notify::sorting", G_CALLBACK (sorting_changed), NULL);

  return G_SOURCE_REMOVE;
}

int main (int argc, char *argv[])
{
  GListModel *model;

  gtk_init ();

  model = get_model (1000000);

  g_idle_add (start_sort, model);

  while (1)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
