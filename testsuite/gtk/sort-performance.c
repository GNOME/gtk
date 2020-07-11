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

static void
count_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  guint      *counter)
{
  *counter += MAX (removed, added);
}

static gint64
snapshot_time (gint64  last,
               gint64 *inout_max)
{
  gint64 now = g_get_monotonic_time ();
  *inout_max = MAX (*inout_max, now - last);
  return now;
}

static void
print_result (const char *testname,
              GType       type,
              gboolean    incremental,
              gsize       size,
              guint       total_time,
              guint       max_time,
              guint       n_comparisons,
              guint       n_changed)
{
  g_print ("# \"%s\", \"%s%s\",%8zu,%8uus,%8uus, %8u,%9u\n",
           testname,
           g_type_name (type),
           incremental ? "-inc" : "",
           size,
           total_time,
           max_time,
           n_comparisons,
           n_changed);
}

static void
set_model (const char *testname,
           GType       type,
           gboolean    incremental,
           GListModel *source,
           guint       random)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);
  g_object_unref (sorter);

  while (TRUE)
    {
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      comparisons = 0;
      n_changed = 0;
      max = 0;

      start = end = g_get_monotonic_time ();
      g_object_set (sort, "model", slice, NULL);
      end = snapshot_time (end, &max);
      while (g_main_context_pending (NULL))
        {
          g_main_context_iteration (NULL, TRUE);
          end = snapshot_time (end, &max);
        }

      total = (end - start);

      print_result (testname, type, incremental, size, total, max, comparisons, n_changed);

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
        gboolean    incremental,
        GListModel *source,
        guint       random,
        guint       fraction)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, (fraction - 1) * size / fraction);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, (fraction - 1) * size / fraction);
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      comparisons = 0;
      n_changed = 0;
      max = 0;

      start = end = g_get_monotonic_time ();
      gtk_slice_list_model_set_size (slice, size);
      end = snapshot_time (end, &max);
      while (g_main_context_pending (NULL))
        {
          g_main_context_iteration (NULL, TRUE);
          end = snapshot_time (end, &max);
        }

      total = (end - start);

      print_result (testname, type, incremental, size, total, max, comparisons, n_changed);

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
             gboolean    incremental,
             GListModel *source,
             guint       random)
{
  append (name, type, incremental, source, random, 2);
}

static void
append_10th (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             guint       random)
{
  append (name, type, incremental, source, random, 10);
}

static void
append_100th (const char *name,
              GType       type,
              gboolean    incremental,
              GListModel *source,
              guint       random)
{
  append (name, type, incremental, source, random, 100);
}

static void
remove_test (const char *testname,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             guint       random,
             guint       fraction)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, size);
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      comparisons = 0;
      n_changed = 0;
      max = 0;

      start = end = g_get_monotonic_time ();
      gtk_slice_list_model_set_size (slice, (fraction - 1) * size / fraction);
      end = snapshot_time (end, &max);
      while (g_main_context_pending (NULL))
        {
          g_main_context_iteration (NULL, TRUE);
          end = snapshot_time (end, &max);
        }

      total = (end - start);

      print_result (testname, type, incremental, size, total, max, comparisons, n_changed);

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
             gboolean    incremental,
             GListModel *source,
             guint       random)
{
  remove_test (name, type, incremental, source, random, 2);
}

static void
remove_10th (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             guint       random)
{
  remove_test (name, type, incremental, source, random, 10);
}

static void
remove_100th (const char *name,
              GType       type,
              gboolean    incremental,
              GListModel *source,
              guint       random)
{
  remove_test (name, type, incremental, source, random, 100);
}

static void
append_n (const char *testname,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random,
          guint       n)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint i, n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, size - n * 100);
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      comparisons = 0;
      n_changed = 0;
      max = 0;

      start = end = g_get_monotonic_time ();
      for (i = 0; i < 100; i++)
        {
          gtk_slice_list_model_set_size (slice, size - n * (100 - i));
          end = snapshot_time (end, &max);
          while (g_main_context_pending (NULL))
            {
              g_main_context_iteration (NULL, TRUE);
              end = snapshot_time (end, &max);
            }
        }

      total = (end - start);

      print_result (testname, type, incremental, size, total, max, comparisons, n_changed);

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
append_1 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random)
{
  append_n (name, type, incremental, source, random, 1);
}

static void
append_2 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random)
{
  append_n (name, type, incremental, source, random, 2);
}

static void
append_10 (const char *name,
           GType       type,
           gboolean    incremental,
           GListModel *source,
           guint       random)
{
  append_n (name, type, incremental, source, random, 10);
}

static void
remove_n (const char *testname,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random,
          guint       n)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GtkSorter *sorter;
  GListModel *sort;
  guint i, n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sorter = create_sorter ();
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);
  g_object_unref (sorter);

  while (TRUE)
    {
      gtk_slice_list_model_set_size (slice, size);
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      comparisons = 0;
      n_changed = 0;
      max = 0;

      start = end = g_get_monotonic_time ();
      for (i = 0; i < 100; i++)
        {
          gtk_slice_list_model_set_size (slice, size - n * (i + 1));
          end = snapshot_time (end, &max);
          while (g_main_context_pending (NULL))
            {
              g_main_context_iteration (NULL, TRUE);
              end = snapshot_time (end, &max);
            }
        }

      total = (end - start);

      print_result (testname, type, incremental, size, total, max, comparisons, n_changed);

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
remove_1 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random)
{
  remove_n (name, type, incremental, source, random, 1);
}

static void
remove_2 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          guint       random)
{
  remove_n (name, type, incremental, source, random, 2);
}

static void
remove_10 (const char *name,
           GType       type,
          gboolean    incremental,
           GListModel *source,
           guint       random)
{
  remove_n (name, type, incremental, source, random, 10);
}

static void
run_test (GtkStringList      *source,
          const char * const *tests,
          const char         *test_name,
          void (* test_func) (const char *name, GType type, gboolean incremental, GListModel *source, guint random))
{
  struct {
    GType type;
    gboolean incremental;
  } types[] = {
    { GTK_TYPE_SORT_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR2_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR3_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR3_LIST_MODEL, TRUE },
    { GTK_TYPE_SOR4_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR5_LIST_MODEL, FALSE },
    { GTK_TYPE_TIM1_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM2_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM2_SORT_MODEL, TRUE },
    { GTK_TYPE_TIM3_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM3_SORT_MODEL, TRUE },
    { GTK_TYPE_TIM4_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM4_SORT_MODEL, TRUE },
  };
  guint random = g_random_int ();
  guint i;

  if (tests != NULL && !g_strv_contains (tests, test_name))
    return;

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    {
      test_func (test_name, types[i].type, types[i].incremental, G_LIST_MODEL (source), random);
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

  g_print ("# \"test\",\"model\",\"model size\",\"time\",\"max time\",\"comparisons\",\"changes\"\n");
  run_test (source, tests, "set-model", set_model);
  run_test (source, tests, "append-half", append_half);
  run_test (source, tests, "append-10th", append_10th);
  run_test (source, tests, "append-100th", append_100th);
  run_test (source, tests, "remove-half", remove_half);
  run_test (source, tests, "remove-10th", remove_10th);
  run_test (source, tests, "remove-100th", remove_100th);
  run_test (source, tests, "append-1", append_1);
  run_test (source, tests, "append-2", append_2);
  run_test (source, tests, "append-10", append_10);
  run_test (source, tests, "remove-1", remove_1);
  run_test (source, tests, "remove-2", remove_2);
  run_test (source, tests, "remove-10", remove_10);

  return g_test_run ();
}
