#include <locale.h>

#include <gtk/gtk.h>

static GQuark number_quark;

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  g_assert (object != NULL);
  g_object_unref (object);
  return GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
}

static char *
get_string (gpointer object)
{
  return g_strdup_printf ("%u", GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark)));
}

static void
append_digit (GString *s,
              guint    digit)
{
  static char *names[10] = { NULL, "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };

  if (digit == 0)
    return;

  g_assert (digit < 10);

  if (s->len)
    g_string_append_c (s, ' ');
  g_string_append (s, names[digit]);
}

static void
append_below_thousand (GString *s,
                       guint    n)
{
  if (n >= 100)
    {
      append_digit (s, n / 100);
      g_string_append (s, " hundred");
      n %= 100;
    }

  if (n >= 20)
    {
      const char *names[10] = { NULL, NULL, "twenty", "thirty", "fourty", "fifty", "sixty", "seventy", "eighty", "ninety" };
      if (s->len)
        g_string_append_c (s, ' ');
      g_string_append (s, names [n / 10]);
      n %= 10;
    }

  if (n >= 10)
    {
      const char *names[10] = { "ten", "eleven", "twelve", "thirteen", "fourteen",
                                "fifteen", "sixteen", "seventeen", "eighteen", "nineteen" };
      if (s->len)
        g_string_append_c (s, ' ');
      g_string_append (s, names [n - 10]);
    }
  else
    {
      append_digit (s, n);
    }
}

static char *
get_spelled_out (gpointer object)
{
  guint n = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
  GString *s;

  g_assert (n < 1000000);

  if (n == 0)
    return g_strdup ("Zero");

  s = g_string_new (NULL);

  if (n >= 1000)
    {
      append_below_thousand (s, n / 1000);
      g_string_append (s, " thousand");
      n %= 1000;
    }

  append_below_thousand (s, n);

  /* Capitalize first letter so we can do case-sensitive matching */
  s->str[0] = g_ascii_toupper (s->str[0]);

  return g_string_free (s, FALSE);
}

static char *
model_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      if (i > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get (model, i));
    }

  return g_string_free (string, FALSE);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step);

static void
add (GListStore *store,
     guint       number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert (number != 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_append (store, object);
  g_object_unref (object);
}

#define assert_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

static GListStore *
new_empty_store (void)
{
  return g_list_store_new (G_TYPE_OBJECT);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step)
{
  GListStore *store = new_empty_store ();
  guint i;

  for (i = start; i <= end; i += step)
    add (store, i);

  return store;
}

static GtkFilterListModel *
new_model (guint      size,
           GtkFilter *filter)
{
  GtkFilterListModel *result;

  result = gtk_filter_list_model_new (G_LIST_MODEL (new_store (1, size, 1)), filter);

  return result;
}


static int
sort_numbers (gpointer   item1,
              gpointer   item2,
              gpointer   data)
{
  guint n1 = GPOINTER_TO_UINT (g_object_get_qdata (item1, number_quark));
  guint n2 = GPOINTER_TO_UINT (g_object_get_qdata (item2, number_quark));
}

static void
test_simple (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;

  sorter = gtk_custom_sorter_new (sort_numbers, NULL);
  model = new_model (20, filter);
  shuffle (model);
  g_object_unref (filter);
  assert_model (model, "3 6 9 12 15 18");
  g_object_unref (model);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Like a trashcan fire in a prison cell.");

  g_test_add_func ("/sorter/simple", test_simple);

  return g_test_run ();
}

