#include <gtk/gtk.h>

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

static GListModel *
make_array_store (guint n_items)
{
  GtkArrayStore *store;
  guint i;

  store = gtk_array_store_new ();

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

static GListModel *
make_string_list2 (guint n_items)
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
do_random_access (const char *kind,
                  guint       size)
{
  GListModel *model;
  guint i;
  guint position;
  GtkStringObject *obj;
  gint64 start, end;
  guint iterations = 10000000;

  if (strcmp (kind, "liststore") == 0)
    model = make_list_store (size);
  else if (strcmp (kind, "arraystore") == 0)
    model = make_array_store (size);
  else if (strcmp (kind, "stringlist") == 0)
    model = make_string_list2 (size);
  else if (strcmp (kind, "array stringlist") == 0)
    model = make_string_list (size);
  else
    g_error ("unsupported: %s", kind);

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      position = g_random_int_range (0, size);
      obj = g_list_model_get_item (model, position);
      if (g_getenv ("PRINT_ACCESS"))
        g_print ("%s", gtk_string_object_get_string (obj));
      g_object_unref (obj);
    }

  end = g_get_monotonic_time ();

  g_print ("\"random access\",\"%s\", %u, %g\n",
           kind,
           size,
           ((double)(end - start)) / iterations);

  g_object_unref (model);
}

static void
do_linear_access (const char *kind,
                  guint       size)
{
  GListModel *model;
  guint i;
  GtkStringObject *obj;
  gint64 start, end;
  guint iterations = 10000000;

  if (strcmp (kind, "liststore") == 0)
    model = make_list_store (size);
  else if (strcmp (kind, "arraystore") == 0)
    model = make_array_store (size);
  else if (strcmp (kind, "stringlist") == 0)
    model = make_string_list2 (size);
  else if (strcmp (kind, "array stringlist") == 0)
    model = make_string_list (size);
  else
    g_error ("unsupported: %s", kind);

  start = g_get_monotonic_time ();

  for (i = 0; i < iterations; i++)
    {
      obj = g_list_model_get_item (model, i % size);
      if (g_getenv ("PRINT_ACCESS"))
        g_print ("%s", gtk_string_object_get_string (obj));
      g_object_unref (obj);
    }

  end = g_get_monotonic_time ();

  g_print ("\"linear access\", \"%s\", %u, %g\n",
           kind,
           size,
           ((double)(end - start)) / iterations);

  g_object_unref (model);
}

static void
do_append (const char *kind,
           guint       size)
{
  GListModel *model;
  guint i, j;
  gint64 start, end;
  int iterations = 5;
  gint64 total;

  total = 0;

  for (i = 0; i < iterations; i++)
    {
      if (strcmp (kind, "liststore") == 0)
        model = make_list_store (0);
      else if (strcmp (kind, "arraystore") == 0)
        model = make_array_store (0);
      else if (strcmp (kind, "stringlist") == 0)
        model = make_string_list2 (0);
      else if (strcmp (kind, "array stringlist") == 0)
        model = make_string_list (0);
      else
        g_error ("unsupported: %s", kind);

      start = g_get_monotonic_time ();

      for (j = 0; j < size; j++)
        {
          char *string = g_strdup_printf ("item %d", j);

          if (strcmp (kind, "liststore") == 0)
            {
              gpointer obj = get_object (string);
              g_list_store_append (G_LIST_STORE (model), obj);
              g_object_unref (obj);
            }
          else if (strcmp (kind, "arraystore") == 0)
            {
              gpointer obj = get_object (string);
              gtk_array_store_append (GTK_ARRAY_STORE (model), obj);
              g_object_unref (obj);
            }
          else if (strcmp (kind, "stringlist") == 0)
            gtk_string_list2_append (GTK_STRING_LIST2 (model), string);
          else if (strcmp (kind, "array stringlist") == 0)
            gtk_string_list_append (GTK_STRING_LIST (model), string);

          g_free (string);
        }

      end = g_get_monotonic_time ();
      total += end - start;

      g_object_unref (model);
    }

  g_print ("\"append\", \"%s\", %u, %g\n", kind, size, ((double)total) / iterations);
}

#define gtk_array_store_insert(store,position,item) \
  gtk_array_store_splice (store, position, 0, (gpointer *)&item, 1)

static void
do_insert (const char *kind,
           guint       size)
{
  GListModel *model;
  guint i, j;
  gint64 start, end;
  int iterations = 5;
  gint64 total;
  guint position;

  total = 0;

  for (i = 0; i < iterations; i++)
    {
      if (strcmp (kind, "liststore") == 0)
        model = make_list_store (1);
      else if (strcmp (kind, "arraystore") == 0)
        model = make_array_store (1);
      else if (strcmp (kind, "stringlist") == 0)
        model = make_string_list2 (1);
      else if (strcmp (kind, "array stringlist") == 0)
        model = make_string_list (1);
      else
        g_error ("unsupported: %s", kind);

      start = g_get_monotonic_time ();

      for (j = 1; j < size; j++)
        {
          char *string = g_strdup_printf ("item %d", j);
          position = g_random_int_range (0, j);

          if (strcmp (kind, "liststore") == 0)
            {
              gpointer obj = get_object (string);
              g_list_store_insert (G_LIST_STORE (model), position, obj);
              g_object_unref (obj);
            }
          else if (strcmp (kind, "arraystore") == 0)
            {
              gpointer obj = get_object (string);
              gtk_array_store_insert (GTK_ARRAY_STORE (model), position, obj);
              g_object_unref (obj);
            }
          else if (strcmp (kind, "stringlist") == 0)
            gtk_string_list2_splice (GTK_STRING_LIST2 (model), position, 0,
                                     (const char * const []){string, NULL});
          else if (strcmp (kind, "array stringlist") == 0)
            gtk_string_list_splice (GTK_STRING_LIST (model), position, 0,
                                     (const char * const []){string, NULL});

          g_free (string);
        }

      end = g_get_monotonic_time ();
      total += end - start;

      g_object_unref (model);
    }

  g_print ("\"insert\", \"%s\", %u, %g\n", kind, size, ((double)total) / iterations);
}

static void
random_access (void)
{
  const char *kind[] = { "liststore", "arraystore", "stringlist", "array stringlist" };
  int sizes = 22;
  int size;
  int i, j;

  for (i = 0; i < G_N_ELEMENTS (kind); i++)
    for (j = 0, size = 2; j < sizes; j++, size *= 2)
      do_random_access (kind[i], size);
}

static void
linear_access (void)
{
  const char *kind[] = { "liststore", "arraystore", "stringlist", "array stringlist" };
  int sizes = 22;
  int size;
  int i, j;

  for (i = 0; i < G_N_ELEMENTS (kind); i++)
    for (j = 0, size = 2; j < sizes; j++, size *= 2)
      do_linear_access (kind[i], size);
}

static void
append (void)
{
  const char *kind[] = { "liststore", "arraystore", "stringlist", "array stringlist" };
  int sizes = 22;
  int size;
  int i, j;

  for (i = 0; i < G_N_ELEMENTS (kind); i++)
    for (j = 0, size = 2; j < sizes; j++, size *= 2)
      do_append (kind[i], size);
}

static void
insert (void)
{
  const char *kind[] = { "liststore", "arraystore", "stringlist", "array stringlist" };
  int sizes = 22;
  int size;
  int i, j;

  for (i = 0; i < G_N_ELEMENTS (kind); i++)
    for (j = 0, size = 2; j < sizes; j++, size *= 2)
      do_insert (kind[i], size);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_print ("\"test\",\"model\",\"model size\",\"time\"");
  g_test_add_func ("/model/random-access", random_access);
  g_test_add_func ("/model/linear-access", linear_access);
  g_test_add_func ("/model/append", append);
  g_test_add_func ("/model/insert", insert);

  return g_test_run ();
}
