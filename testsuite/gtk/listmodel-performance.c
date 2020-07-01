#include <gtk/gtk.h>

#include "gtkstringlist.h"

static GObject *
get_object (const char *string)
{
  GtkStringList *list;
  GObject *obj;

  list = gtk_string_list_new ((const char *[]){string, NULL});
  obj = g_list_model_get_item (G_LIST_MODEL (list), 0);
  g_object_unref (list);

  return obj;
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
      GObject *obj;

      string = g_strdup_printf ("item %d", i);
      obj = get_object (string);
      g_list_store_append (store, obj);
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
           iterations / (((double)(end - start)) / G_TIME_SPAN_SECOND));

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
           iterations / (((double)(end - start)) / G_TIME_SPAN_SECOND));

  g_object_unref (model);
}

static void
random_access (void)
{
  const char *kind[] = { "liststore", "stringlist", "array stringlist" };
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
  const char *kind[] = { "liststore", "stringlist", "array stringlist" };
  int sizes = 22;
  int size;
  int i, j;

  for (i = 0; i < G_N_ELEMENTS (kind); i++)
    for (j = 0, size = 2; j < sizes; j++, size *= 2)
      do_linear_access (kind[i], size);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_print ("\"test\",\"model\",\"model size\",\"accesses/s\"");
  g_test_add_func ("/liststore/random-access", random_access);
  g_test_add_func ("/liststore/linear-access", linear_access);

  return g_test_run ();
}
