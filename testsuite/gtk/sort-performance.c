#include <gtk/gtk.h>
#include <math.h>

#define MAX_SIZE 1024000
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
set_model (const char *testname,
           GType       type,
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

      g_print ("\"%s\", \"%s\",%8u,%8uus,%8u\n",
               testname,
               g_type_name (type),
               size,
               (guint) total,
               comparisons);

      if (total > MAX_TIME)
        break;

      size *= 2;
      if (4 * total > 2 * MAX_TIME)
        break;

      g_object_set (sort, "model", NULL, NULL);
      gtk_slice_list_model_set_size (slice, size);
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
append (const char *testname,
        GType       type,
        GListModel *source,
        guint       random,
        guint       fraction)
{
  gint64 start, end, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint size = 1000;

  slice = gtk_slice_list_model_new (source, 0, (fraction - 1) * size / fraction);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       NULL);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, (fraction - 1) * size / fraction);
      comparisons = 0;

      start = g_get_monotonic_time ();
      gtk_slice_list_model_set_size (slice, size);
      end = g_get_monotonic_time ();

      total = (end - start);

      g_print ("\"%s\", \"%s\",%8u,%8uus,%8u\n",
               testname,
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
append_half (const char *name,
             GType       type,
             GListModel *source,
             guint       random)
{
  append (name, type, source, random, 2);
}

static void
append_10th (const char *name,
             GType       type,
             GListModel *source,
             guint       random)
{
  append (name, type, source, random, 10);
}

static void
append_100th (const char *name,
              GType       type,
              GListModel *source,
              guint       random)
{
  append (name, type, source, random, 100);
}

static void
remove_test (const char *testname,
             GType       type,
             GListModel *source,
             guint       random,
             guint       fraction)
{
  gint64 start, end, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       NULL);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, size);
      comparisons = 0;

      start = g_get_monotonic_time ();
      gtk_slice_list_model_set_size (slice, (fraction - 1) * size / fraction);
      end = g_get_monotonic_time ();

      total = (end - start);

      g_print ("\"%s\", \"%s\",%8u,%8uus,%8u\n",
               testname,
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
remove_half (const char *name,
             GType       type,
             GListModel *source,
             guint       random)
{
  remove_test (name, type, source, random, 2);
}

static void
remove_10th (const char *name,
             GType       type,
             GListModel *source,
             guint       random)
{
  remove_test (name, type, source, random, 10);
}

static void
remove_100th (const char *name,
              GType       type,
              GListModel *source,
              guint       random)
{
  remove_test (name, type, source, random, 100);
}

static void
run_test (GtkStringList      *source,
          const char * const *tests,
          const char         *test_name,
          void (* test_func) (const char *name, GType type, GListModel *source, guint random))
{
  GType types[] = { 
    GTK_TYPE_SORT_LIST_MODEL,
    GTK_TYPE_SOR2_LIST_MODEL,
    GTK_TYPE_SOR3_LIST_MODEL,
    GTK_TYPE_SOR4_LIST_MODEL
  };
  guint random = g_random_int ();
  guint i;

  if (tests != NULL && !g_strv_contains (tests, test_name))
    return;

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    {
      test_func (test_name, types[i], G_LIST_MODEL (source), random);
    }
}

int
main (int argc, char *argv[])
{
  GtkStringList *source;
  guint random = g_random_int ();
  guint i;
  const char * const *tests;

  gtk_test_init (&argc, &argv);

  source = gtk_string_list_new (NULL);
  for (i = 0; i < MAX_SIZE; i++)
    {
      gtk_string_list_take (source, g_strdup_printf ("%u", random));
      random = quick_random  (random);
    }

  if (argc < 2)
    tests = NULL;
  else
    tests = (const char **) argv + 1;

  g_print ("\"test\",\"model\",\"model size\",\"time\",\"comparisons\"\n");
  run_test (source, tests, "set-model", set_model);
  run_test (source, tests, "append-half", append_half);
  run_test (source, tests, "append-10th", append_10th);
  run_test (source, tests, "append-100th", append_100th);
  run_test (source, tests, "remove-half", remove_half);
  run_test (source, tests, "remove-10th", remove_10th);
  run_test (source, tests, "remove-100th", remove_100th);

  return g_test_run ();
}
