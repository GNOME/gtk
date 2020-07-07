#include <gtk/gtk.h>
#include <math.h>

#define MAX_SIZE 1000000
#define MAX_TIME (G_USEC_PER_SEC / 2)

static inline guint
quick_random (guint prev)
{
  prev ^= prev << 13;
  prev ^= prev >> 17;
  prev ^= prev << 5;
  return prev;
}

static guint comparisons = 0;

static int
compare_string_object (gconstpointer a,
                       gconstpointer b,
                       gpointer      unused)
{
  GtkStringObject *sa = (GtkStringObject *) a;
  GtkStringObject *sb = (GtkStringObject *) b;

  comparisons++;

  return gtk_ordering_from_cmpfunc (strcmp (gtk_string_object_get_string (sa),
                                            gtk_string_object_get_string (sb)));
}

static GtkSorter *
create_sorter (void)
{
  return gtk_custom_sorter_new (compare_string_object, NULL, NULL);
}

G_GNUC_UNUSED static void
set_model (GType       type,
           GListModel *source,
           guint       random)
{
  gint64 start, end, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "sorter", sorter,
                       NULL);
  g_object_unref (sorter);

  while (TRUE)
    {
      comparisons = 0;
      start = g_get_monotonic_time ();
      g_object_set (sort, "model", slice, NULL);
      end = g_get_monotonic_time ();

      total = (end - start);

      g_print ("\"set-model\", \"%s\",%8u,%8uus,%8u\n",
               g_type_name (type),
               size,
               (guint) total,
               comparisons);

      if (total > MAX_TIME)
        break;

      size *= 2;
      if (4 * total > 2 * MAX_TIME)
        break;

      gtk_slice_list_model_set_size (slice, size);
      g_object_set (sort, "model", NULL, NULL);
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
append_items (GType       type,
              GListModel *source,
              guint       random)
{
  gint64 start, end, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size / 2);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       NULL);
  g_object_unref (sorter);

  while (TRUE)
    {
      comparisons = 0;
      start = g_get_monotonic_time ();
      gtk_slice_list_model_set_size (slice, size);
      end = g_get_monotonic_time ();

      total = (end - start);

      g_print ("\"append-items\", \"%s\",%8u,%8uus,%8u\n",
               g_type_name (type),
               size,
               (guint) total,
               comparisons);

      if (total > MAX_TIME)
        break;

      size *= 2;
      if (4 * total > 2 * MAX_TIME ||
          size > MAX_SIZE)
        break;
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
run_test (GtkStringList *source,
          void (* test_func) (GType type, GListModel *source, guint random))
{
  GType types[] = { 
    GTK_TYPE_SORT_LIST_MODEL,
    GTK_TYPE_SOR2_LIST_MODEL
  };
  guint random = g_random_int ();
  guint i;

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    {
      test_func (types[i], G_LIST_MODEL (source), random);
    }
}

int
main (int argc, char *argv[])
{
  GtkStringList *source;
  guint random = g_random_int ();
  guint i;

  gtk_test_init (&argc, &argv);

  source = gtk_string_list_new (NULL);
  for (i = 0; i < MAX_SIZE; i++)
    {
      gtk_string_list_take (source, g_strdup_printf ("%u", random));
      random = quick_random  (random);
    }

  g_print ("\"test\",\"model\",\"model size\",\"time\",\"comparisons\"\n");
  if (argc < 2 || g_strv_contains ((const char **) argv + 1, "set-model"))
    run_test (source, set_model);
  if (argc < 2 || g_strv_contains ((const char **) argv + 1, "append-items"))
    run_test (source, append_items);

  return g_test_run ();
}
