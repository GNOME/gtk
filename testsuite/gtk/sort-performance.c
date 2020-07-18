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
count_comparisons (gconstpointer a,
                   gconstpointer b,
                   gpointer      unused)
{
  comparisons++;

  return GTK_ORDERING_EQUAL;
}

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
           GtkSorter  *sorter,
           guint       random)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sort = g_object_new (type,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);

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

      if (total > MAX_TIME ||
          size >= g_list_model_get_n_items (source))
        break;

      size *= 2;

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
        GtkSorter  *sorter,
        guint       random,
        guint       fraction)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, (fraction - 1) * size / fraction);
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);

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

      if (total > MAX_TIME ||
          size >= g_list_model_get_n_items (source))
        break;

      size *= 2;
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
append_half (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             GtkSorter  *sorter,
             guint       random)
{
  append (name, type, incremental, source, sorter, random, 2);
}

static void
append_10th (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             GtkSorter  *sorter,
             guint       random)
{
  append (name, type, incremental, source, sorter, random, 10);
}

static void
append_100th (const char *name,
              GType       type,
              gboolean    incremental,
              GListModel *source,
              GtkSorter  *sorter,
              guint       random)
{
  append (name, type, incremental, source, sorter, random, 100);
}

static void
remove_test (const char *testname,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             GtkSorter  *sorter,
             guint       random,
             guint       fraction)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GListModel *sort;
  guint n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);

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

      if (total > MAX_TIME ||
          size >= g_list_model_get_n_items (source))
        break;

      size *= 2;
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
remove_half (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             GtkSorter  *sorter,
             guint       random)
{
  remove_test (name, type, incremental, source, sorter, random, 2);
}

static void
remove_10th (const char *name,
             GType       type,
             gboolean    incremental,
             GListModel *source,
             GtkSorter  *sorter,
             guint       random)
{
  remove_test (name, type, incremental, source, sorter, random, 10);
}

static void
remove_100th (const char *name,
              GType       type,
              gboolean    incremental,
              GListModel *source,
              GtkSorter  *sorter,
              guint       random)
{
  remove_test (name, type, incremental, source, sorter, random, 100);
}

static void
append_n (const char *testname,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random,
          guint       n)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GListModel *sort;
  guint i, n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);

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

      if (total > MAX_TIME ||
          size >= g_list_model_get_n_items (source))
        break;

      size *= 2;
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
append_1 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random)
{
  append_n (name, type, incremental, source, sorter, random, 1);
}

static void
append_2 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random)
{
  append_n (name, type, incremental, source, sorter, random, 2);
}

static void
append_10 (const char *name,
           GType       type,
           gboolean    incremental,
           GListModel *source,
           GtkSorter  *sorter,
           guint       random)
{
  append_n (name, type, incremental, source, sorter, random, 10);
}

static void
remove_n (const char *testname,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random,
          guint       n)
{
  gint64 start, end, max, total;
  GtkSliceListModel *slice;
  GListModel *sort;
  guint i, n_changed, size = 1000;

  slice = gtk_slice_list_model_new (source, 0, size);
  sort = g_object_new (type,
                       "model", slice,
                       "sorter", sorter,
                       incremental ? "incremental" : NULL, TRUE,
                       NULL);
  g_signal_connect (sort, "items-changed", G_CALLBACK (count_changed_cb), &n_changed);

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

      if (total > MAX_TIME ||
          size >= g_list_model_get_n_items (source))
        break;

      size *= 2;
    }

  g_object_unref (sort);
  g_object_unref (slice);
}

static void
remove_1 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random)
{
  remove_n (name, type, incremental, source, sorter, random, 1);
}

static void
remove_2 (const char *name,
          GType       type,
          gboolean    incremental,
          GListModel *source,
          GtkSorter  *sorter,
          guint       random)
{
  remove_n (name, type, incremental, source, sorter, random, 2);
}

static void
remove_10 (const char *name,
           GType       type,
           gboolean    incremental,
           GListModel *source,
           GtkSorter  *sorter,
           guint       random)
{
  remove_n (name, type, incremental, source, sorter, random, 10);
}

static void
done_loading_directory (GtkDirectoryList *dir,
                        GParamSpec       *pspec,
                        gpointer          data)
{
  /* When we get G_IO_ERROR_TOO_MANY_OPEN_FILES we enqueue directories here for reloading
   * as more file descriptors get available
   */
  static GSList *too_many = NULL;

  const GError *error;
  guint *counters = data;

  /* happens when restarting the load below */
  if (gtk_directory_list_is_loading (dir))
    return;

  error = gtk_directory_list_get_error (dir);
  if (error)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TOO_MANY_OPEN_FILES))
        {
          too_many = g_slist_prepend (too_many, g_object_ref (dir));
          return;
        }
    }
  counters[1]++;
  if (too_many)
    {
      GtkDirectoryList *reload = too_many->data;
      GFile *file;

      too_many = g_slist_remove (too_many, reload);
      file = g_object_ref (gtk_directory_list_get_file (reload));
      gtk_directory_list_set_file (reload, NULL);
      gtk_directory_list_set_file (reload, file);
      g_object_unref (file);
    }
}

static gboolean
file_info_is_directory (GFileInfo *file_info)
{
  if (g_file_info_get_is_symlink (file_info))
    return FALSE;

  return g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY;
}

static GListModel *
create_directory_list (gpointer item,
                       gpointer data)
{
  GFileInfo *file_info = G_FILE_INFO (item);
  guint *counters = data;
  GtkDirectoryList *dir;
  GFile *file;

  if (!file_info_is_directory (file_info))
    return NULL;
  file = G_FILE (g_file_info_get_attribute_object (file_info, "standard::file"));
  if (file == NULL)
    return NULL;

  dir = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_TYPE
                                "," G_FILE_ATTRIBUTE_STANDARD_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
                                NULL);
  gtk_directory_list_set_io_priority (dir, G_PRIORITY_DEFAULT + g_random_int_range (-5, 5));
  gtk_directory_list_set_monitored (dir, FALSE);
  gtk_directory_list_set_file (dir, file);
  counters[0]++;
  g_signal_connect (dir, "notify::loading", G_CALLBACK (done_loading_directory), counters);
  g_assert (gtk_directory_list_is_loading (dir));

  return G_LIST_MODEL (dir);
}

static GListModel *
get_file_infos (void)
{
  static GtkTreeListModel *tree = NULL;
  gint64 start, end, max;
  GtkDirectoryList *dir;
  GFile *root;
  guint counters[2] = { 1, 0 };

  if (tree)
    return G_LIST_MODEL (g_object_ref (tree));

  if (g_getenv ("G_TEST_SRCDIR"))
    root = g_file_new_for_path (g_getenv ("G_TEST_SRCDIR"));
  else
    root = g_file_new_for_path (g_get_home_dir ());

  start = end = g_get_monotonic_time ();
  max = 0;
  dir = gtk_directory_list_new (G_FILE_ATTRIBUTE_STANDARD_TYPE
                                "," G_FILE_ATTRIBUTE_STANDARD_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME
                                "," G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
                                NULL);
  gtk_directory_list_set_monitored (dir, FALSE);
  gtk_directory_list_set_file (dir, root);
  tree = gtk_tree_list_model_new (FALSE,
                                  G_LIST_MODEL (dir),
                                  TRUE,
                                  create_directory_list,
                                  counters, NULL);
  g_signal_connect (dir, "notify::loading", G_CALLBACK (done_loading_directory), counters);
  end = snapshot_time (end, &max);
  while (counters[0] != counters[1])
    {
      g_main_context_iteration (NULL, TRUE);
      end = snapshot_time (end, &max);
    }
  //g_print ("%u/%u\n", counters[0], counters[1]);

  end = snapshot_time (end, &max);

  print_result ("load-directory", GTK_TYPE_DIRECTORY_LIST, FALSE, g_list_model_get_n_items (G_LIST_MODEL (tree)), end - start, max, 0, counters[0]);

  g_object_unref (dir);
  g_object_unref (root);

  return G_LIST_MODEL (g_object_ref (tree));
}

static void
run_test (GListModel         *source,
          GtkSorter          *sorter,
          const char * const *tests,
          const char         *test_name,
          void (* test_func) (const char *name, GType type, gboolean incremental, GListModel *source, GtkSorter *sorter, guint random))
{
  struct {
    GType type;
    gboolean incremental;
  } types[] = { 
    { GTK_TYPE_SORT_LIST_MODEL, FALSE },
    { GTK_TYPE_GSEQ_SORT_MODEL, FALSE },
    { GTK_TYPE_SOR2_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR3_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR4_LIST_MODEL, FALSE },
    { GTK_TYPE_SOR5_LIST_MODEL, FALSE },
    { GTK_TYPE_TIM1_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM2_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM3_SORT_MODEL, FALSE },
    { GTK_TYPE_TIM4_SORT_MODEL, FALSE },
    { GTK_TYPE_SORT_LIST_MODEL, TRUE },
    { GTK_TYPE_SOR3_LIST_MODEL, TRUE },
    { GTK_TYPE_TIM2_SORT_MODEL, TRUE },
    { GTK_TYPE_TIM3_SORT_MODEL, TRUE },
    { GTK_TYPE_TIM4_SORT_MODEL, TRUE },
  };
  guint random = g_random_int ();
  guint i;

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    {
      test_func (test_name, types[i].type, types[i].incremental, source, sorter, random);
    }
}

static GListModel *
get_string_list (void)
{
  static GtkStringList *string_list = NULL;

  if (string_list == NULL)
    {
      guint i, random;
      random = g_test_rand_int ();
      string_list = gtk_string_list_new (NULL);
      for (i = 0; i < MAX_SIZE; i++)
        {
          gtk_string_list_take (string_list, g_strdup_printf ("%u", random));
          random = quick_random  (random);
        }
    }

  return G_LIST_MODEL (g_object_ref (string_list));
}

static void
run_tests (const char * const *tests,
           const char         *test_name,
           void (* test_func) (const char *name, GType type, gboolean incremental, GListModel *source, GtkSorter *sorter, guint random))
{
  const char *suffixes[] = { "string", "tree", "filename" };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS(suffixes); i++)
    {
      GListModel *source;
      GtkSorter *sorter;
      char *name;

      name = g_strdup_printf ("%s-%s", test_name, suffixes[i]);
      if (tests != NULL && !g_strv_contains (tests, name))
        {
          g_free (name);
          continue;
        }

      switch (i)
        {
        case 0:
          source = get_string_list ();
          sorter = gtk_custom_sorter_new (compare_string_object, NULL, NULL);
          break;

        case 1:
          source = get_file_infos ();
          sorter = gtk_multi_sorter_new ();
          gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter), gtk_custom_sorter_new (count_comparisons, NULL, NULL));
          gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter),
                                   gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_BOOLEAN,
                                                                                        NULL,
                                                                                        0,
                                                                                        NULL,
                                                                                        (GCallback) file_info_is_directory,
                                                                                        NULL, NULL)));
          gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter),
                                   gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT64,
                                                                                        NULL,
                                                                                        1,
                                                                                        (GtkExpression *[1]) {
                                                                                          gtk_constant_expression_new (G_TYPE_STRING, G_FILE_ATTRIBUTE_STANDARD_SIZE)
                                                                                        },
                                                                                        (GCallback) g_file_info_get_attribute_uint64,
                                                                                        NULL, NULL)));
          sorter = gtk_tree_list_row_sorter_new (sorter);
          break;

        case 2:
          {
            GListModel *infos = get_file_infos ();
            source = G_LIST_MODEL (gtk_map_list_model_new (infos,
                                                           (GtkMapListModelMapFunc) gtk_tree_list_row_get_item,
                                                           NULL, NULL));
            sorter = gtk_string_sorter_new (
                         gtk_cclosure_expression_new (G_TYPE_STRING,
                                                      NULL,
                                                      1,
                                                      (GtkExpression *[1]) {
                                                        gtk_constant_expression_new (G_TYPE_STRING, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME)
                                                      },
                                                      (GCallback) g_file_info_get_attribute_as_string,
                                                      NULL,
                                                      NULL));
            g_object_unref (infos);
          }
          break;

        default:
          g_assert_not_reached ();
          return;
        }

      run_test (source, sorter, tests, name, test_func);

      g_free (name);
      g_object_unref (sorter);
      g_object_unref (source);
    }
}

int
main (int argc, char *argv[])
{
  const char * const *tests;

  gtk_test_init (&argc, &argv);

  if (argc < 2)
    tests = NULL;
  else
    tests = (const char **) argv + 1;

  g_print ("# \"test\",\"model\",\"model size\",\"time\",\"max time\",\"comparisons\",\"changes\"\n");
  run_tests (tests, "set-model", set_model);
  run_tests (tests, "append-half", append_half);
  run_tests (tests, "append-10th", append_10th);
  run_tests (tests, "append-100th", append_100th);
  run_tests (tests, "remove-half", remove_half);
  run_tests (tests, "remove-10th", remove_10th);
  run_tests (tests, "remove-100th", remove_100th);
  run_tests (tests, "append-1", append_1);
  run_tests (tests, "append-2", append_2);
  run_tests (tests, "append-10", append_10);
  run_tests (tests, "remove-1", remove_1);
  run_tests (tests, "remove-2", remove_2);
  run_tests (tests, "remove-10", remove_10);
  run_tests (tests, "remove-10", remove_10);


  return g_test_run ();
}
