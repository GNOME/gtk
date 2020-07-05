#include <gtk/gtk.h>

#include "gtkarraystore.h"
#include "gtkstringlist.h"

typedef struct {
  GObject obj;
  char *str;
} StrObj;

static GtkStringObject *
get_object (const char *string)
{
  GtkStringObject *s;

  s = g_object_new (GTK_TYPE_STRING_OBJECT, NULL);
  ((StrObj*)s)->str = g_strdup (string);

  return s;
}

typedef struct {
  const char *name;
  GListModel * (* create_model) (guint n_items);
  void         (* append)       (GListModel *model, const char *s);
  void         (* insert)       (GListModel *model, guint pos, const char *s);
  guint64      (* size)         (GListModel *model);
} Model;

static GListModel *
make_list_store (guint n_items)
{
  GListStore *store;
  guint i;

  store = g_list_store_new (GTK_TYPE_STRING_OBJECT);

  for (i = 0; i < n_items; i++)
    {
      char *string;
      gpointer obj;

      string = g_strdup_printf ("item %d", i);
      obj = get_object (string);
      g_list_store_append (store, obj);
      g_object_unref (obj);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
append_list_store (GListModel *model, const char *s)
{
  gpointer obj = get_object (s);
  g_list_store_append (G_LIST_STORE (model), obj);
  g_object_unref (obj);
}

static void
insert_list_store (GListModel *model, guint pos, const char *s)
{
  gpointer obj = get_object (s);
  g_list_store_insert (G_LIST_STORE (model), pos, obj);
  g_object_unref (obj);
}

static GListModel *
make_array_store (guint n_items)
{
  GtkArrayStore *store;
  guint i;

  store = gtk_array_store_new (GTK_TYPE_STRING_OBJECT);

  for (i = 0; i < n_items; i++)
    {
      char *string;
      gpointer obj;

      string = g_strdup_printf ("item %d", i);
      obj = get_object (string);
      gtk_array_store_append (store, obj);
      g_object_unref (obj);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
append_array_store (GListModel *model, const char *s)
{
  gpointer obj = get_object (s);
  gtk_array_store_append (GTK_ARRAY_STORE (model), obj);
  g_object_unref (obj);
}

static void
insert_array_store (GListModel *model, guint pos, const char *s)
{
  gpointer obj = get_object (s);
  gtk_array_store_splice (GTK_ARRAY_STORE (model), pos, 0, (gpointer *)&obj, 1);
  g_object_unref (obj);
}

static GListModel *
make_array_store2 (guint n_items)
{
  GtkArrayStore2 *store;
  guint i;

  store = gtk_array_store2_new (GTK_TYPE_STRING_OBJECT);

  for (i = 0; i < n_items; i++)
    {
      char *string;
      gpointer obj;

      string = g_strdup_printf ("item %d", i);
      obj = get_object (string);
      gtk_array_store2_append (store, obj);
      g_object_unref (obj);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
append_array_store2 (GListModel *model, const char *s)
{
  gpointer obj = get_object (s);
  gtk_array_store2_append (GTK_ARRAY_STORE2 (model), obj);
  g_object_unref (obj);
}

static void
insert_array_store2 (GListModel *model, guint pos, const char *s)
{
  gpointer obj = get_object (s);
  gtk_array_store2_splice (GTK_ARRAY_STORE2 (model), pos, 0, (gpointer *)&obj, 1);
  g_object_unref (obj);
}

static GListModel *
make_sequence_string_list (guint n_items)
{
  GtkStringList2 *store;
  guint i;

  store = gtk_string_list2_new (NULL);

  for (i = 0; i < n_items; i++)
    {
      char *string;

      string = g_strdup_printf ("item %d", i);
      gtk_string_list2_append (store, string);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
append_sequence_string_list (GListModel *model, const char *s)
{
  gtk_string_list2_append (GTK_STRING_LIST2 (model), s);
}

static void
insert_sequence_string_list (GListModel *model, guint pos, const char *s)
{
  gtk_string_list2_splice (GTK_STRING_LIST2 (model), pos, 0, (const char *[2]) { s, NULL });
}

static GListModel *
make_string_list (guint n_items)
{
  GtkStringList *store;
  guint i;

  store = gtk_string_list_new (NULL);

  for (i = 0; i < n_items; i++)
    {
      char *string;

      string = g_strdup_printf ("item %d", i);
      gtk_string_list_append (store, string);
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static void
append_string_list (GListModel *model, const char *s)
{
  gtk_string_list_append (GTK_STRING_LIST (model), s);
}

static void
insert_string_list (GListModel *model, guint pos, const char *s)
{
  gtk_string_list_splice (GTK_STRING_LIST (model), pos, 0, (const char *[2]) { s, NULL });
}

static void
do_random_access (const Model *klass,
                  guint        size)
{
  GListModel *model;
  guint i;
  guint position;
  GtkStringObject *obj;
  gint64 start, end;
  guint iterations = 10 * 1000 * 1000;

  model = klass->create_model (size);

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      position = g_random_int_range (0, size);
      obj = g_list_model_get_item (model, position);
      if (g_getenv ("PRINT_ACCESS"))
        g_printerr ("%s", gtk_string_object_get_string (obj));
      g_object_unref (obj);
    }

  end = g_get_monotonic_time ();

  g_printerr ("\"random access\",\"%s\", %u, %g, %lu\n",
              klass->name,
              size,
              ((double)(end - start)) / iterations,
              klass->size (model));

  g_object_unref (model);
}

static void
do_linear_access (const Model *klass,
                  guint        size)
{
  GListModel *model;
  guint i;
  GtkStringObject *obj;
  gint64 start, end;
  guint iterations = 1000 * 1000;

  model = klass->create_model (size);

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      obj = g_list_model_get_item (model, i % size);
      if (g_getenv ("PRINT_ACCESS"))
        g_printerr ("%s", gtk_string_object_get_string (obj));
      g_object_unref (obj);
    }

  end = g_get_monotonic_time ();

  g_printerr ("\"linear access\", \"%s\", %u, %g, %lu\n",
              klass->name,
              size,
              ((double)(end - start)) / iterations,
              klass->size (model));

  g_object_unref (model);
}

static void
do_append (const Model *klass,
           guint        size)
{
  GListModel *model;
  guint i, j;
  gint64 start, end;
  int iterations = 5;
  gint64 total_time;
  guint64 total_size;

  total_time = 0;
  total_size = 0;

  for (i = 0; i < iterations; i++)
    {
      model = klass->create_model (size);

      start = g_get_monotonic_time ();

      for (j = 0; j < size; j++)
        {
          char *string = g_strdup_printf ("item %d", j);

          klass->append (model, string);
        }

      end = g_get_monotonic_time ();
      total_time += end - start;
      total_size += klass->size (model);

      g_object_unref (model);
    }

  g_printerr ("\"append\", \"%s\", %u, %g, %g\n", klass->name, size, ((double)total_time) / iterations, ((double)total_size) / iterations);
}

static void
do_insert (const Model *klass,
           guint        size)
{
  GListModel *model;
  guint i, j;
  gint64 start, end;
  int iterations = 5;
  gint64 total_time;
  guint64 total_size;
  guint position;

  total_time = 0;
  total_size = 0;

  for (i = 0; i < iterations; i++)
    {
      model = klass->create_model (size);

      start = g_get_monotonic_time ();

      for (j = 1; j < size; j++)
        {
          char *string = g_strdup_printf ("item %d", j);
          position = g_random_int_range (0, j);

          klass->insert (model, position, string);

          g_free (string);
        }

      end = g_get_monotonic_time ();
      total_time += end - start;
      total_size += klass->size (model);

      g_object_unref (model);
    }

  g_printerr ("\"insert\", \"%s\", %u, %g, %g\n", klass->name, size, ((double)total_time) / iterations, ((double)total_size) / iterations);
}

static guint64
no_size (GListModel *model)
{
  return 0;
}

const Model all_models[] = {
  {
    "judy-stringlist",
    make_sequence_string_list,
    append_sequence_string_list,
    insert_sequence_string_list,
    gtk_string_list2_get_size
  },
#if 0
  {
    "liststore",
    make_list_store,
    append_list_store,
    insert_list_store,
    no_size
  },
  {
    "arraystore",
    make_array_store,
    append_array_store,
    insert_array_store,
    no_size
  },
  {
    "ptrarraystore",
    make_array_store2,
    append_array_store2,
    insert_array_store2,
    no_size
  },
#endif
  {
    "stringlist",
    make_string_list,
    append_string_list,
    insert_string_list,
    gtk_string_list_get_size
  }
};

typedef struct {
  void (* test_func) (const Model *model, guint size);
  const Model *model;
  guint size;
} TestData;

static void
free_test_data (gpointer data)
{
  g_free (data);
}

static void
run_test (gconstpointer data)
{
  const TestData *test = data;

  test->test_func (test->model, test->size);
}

static void
add_test (const char *name,
          void (* test_func) (const Model *model, guint size))
{
  int max_size = 10 * 1000 * 1000;
  int size;
  int i;

  for (i = 0; i < G_N_ELEMENTS (all_models); i++)
    {
      for (size = 1; size <= max_size; size *= 100)
        {
          TestData *test = g_new0 (TestData, 1);
          char *path;

          test->test_func = test_func;
          test->model = &all_models[i];
          test->size = size;
          path = g_strdup_printf ("/model/%s/%s/size-%d", name, all_models[i].name, size);
          g_test_add_data_func_full (path, test, run_test, free_test_data);
          g_free (path);
        }
    }
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_printerr ("\"test\",\"model\",\"model size\",\"time\"\n");
  add_test ("random-access", do_random_access);
  add_test ("linear-access", do_linear_access);
  add_test ("append", do_append);
  add_test ("insert", do_insert);

  return g_test_run ();
}
