/* GtkSorter tests.
 *
 * Copyright (C) 2019, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>

#include <gtk/gtk.h>

static GQuark number_quark;

static guint
get_number (GObject *object)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
}

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  guint number;
  g_assert_nonnull (object);
  number = get_number (object);
  g_object_unref (object);
  return number;
}

static char *
get_string (gpointer object)
{
  return g_strdup_printf ("%u", GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark)));
}

static guint
get_number_mod_5 (GObject *object)
{
  return get_number (object) % 5;
}

static void
append_digit (GString *s,
              guint    digit)
{
  static const char *names[10] = { NULL, "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };

  if (digit == 0)
    return;

  g_assert_cmpint (digit, <, 10);

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
      const char *names[10] = { NULL, NULL, "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };
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

  g_assert_cmpint (n, <, 1000000);

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

  /* Capitalize first letter so we can do case-sensitive sorting */
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
  g_assert_cmpint (number, !=, 0);

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

#define assert_not_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " != " #expected, s, "!=", expected); \
  g_free (s); \
}G_STMT_END

/* This could be faster by foreach()ing through the models and comparing
 * the item pointers */
#define assert_model_equal(model1, model2) G_STMT_START{\
  char *s1 = model_to_string (G_LIST_MODEL (model1)); \
  char *s2 = model_to_string (G_LIST_MODEL (model2)); \
  if (!g_str_equal (s1, s2)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model1 " != " #model2, s1, "==", s2); \
  g_free (s2); \
  g_free (s1); \
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

static GListStore *
fisher_yates_shuffle (GListStore *store)
{
  int i, n;
  gboolean shuffled = FALSE;

  while (!shuffled)
    {
      n = g_list_model_get_n_items (G_LIST_MODEL (store));
      for (i = 0; i < n; i++)
        {
          int pos = g_random_int_range (0, n - i);
          GObject *item;

          item = g_list_model_get_item (G_LIST_MODEL (store), pos);
          g_list_store_remove (store, pos);
          g_list_store_append (store, item);
          g_object_unref (item); 
          shuffled |= pos != 0;
        }
    }

  return store;
}

static GtkSortListModel *
new_model (guint      size,
           GtkSorter *sorter)
{
  GtkSortListModel *result;

  if (sorter)
    g_object_ref (sorter);
  result = gtk_sort_list_model_new (G_LIST_MODEL (fisher_yates_shuffle (new_store (1, size, 1))), sorter);

  return result;
}

static int
compare_numbers (gconstpointer item1,
                 gconstpointer item2,
                 gpointer data)
{
  guint n1 = get_number (G_OBJECT (item1));
  guint n2 = get_number (G_OBJECT (item2));

  return n1 - n2;
}

static void
test_simple (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;

  model = new_model (20, NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  sorter = GTK_SORTER (gtk_custom_sorter_new (compare_numbers, NULL, NULL));
  gtk_sort_list_model_set_sorter (model, sorter);
  g_object_unref (sorter);

  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  gtk_sort_list_model_set_sorter (model, NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  g_object_unref (model);
}

static void
test_string (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;
  GtkExpression *expression;

  model = new_model (20, NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_string, NULL, NULL)));

  gtk_sort_list_model_set_sorter (model, sorter);
  g_object_unref (sorter);

  assert_model (model, "1 10 11 12 13 14 15 16 17 18 19 2 20 3 4 5 6 7 8 9");

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_spelled_out, NULL, NULL);
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_expression_unref (expression);

  assert_model (model, "8 18 11 15 5 4 14 9 19 1 7 17 6 16 10 13 3 12 20 2");

  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  g_object_unref (model);
}

static void
inc_counter (GtkSorter *sorter, int change, gpointer data)
{
  int *counter = data;

  (*counter)++;
}

static void
test_change (void)
{
  GtkSorter *sorter;
  GtkExpression *expression;
  int counter = 0;

  sorter = GTK_SORTER (gtk_string_sorter_new (NULL));
  g_signal_connect (sorter, "changed", G_CALLBACK (inc_counter), &counter);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_string, NULL, NULL);
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  g_assert_cmpint (counter, ==, 1);

  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  g_assert_cmpint (counter, ==, 1);
  
  gtk_expression_unref (expression);

  gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), FALSE);
  g_assert_cmpint (counter, ==, 2);

  gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), FALSE);
  g_assert_cmpint (counter, ==, 2);

  g_object_unref (sorter);
}

static void
test_numeric (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;

  model = new_model (20, NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL)));
  gtk_sort_list_model_set_sorter (model, sorter);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
  assert_model (model, "20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1");

  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_ASCENDING);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  gtk_numeric_sorter_set_expression (GTK_NUMERIC_SORTER (sorter), NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  g_object_unref (sorter);
  g_object_unref (model);
}

/* sort even numbers before odd, don't care about anything else */
static int
compare_even (gconstpointer item1,
              gconstpointer item2,
              gpointer data)
{
  guint n1 = get_number (G_OBJECT (item1));
  guint n2 = get_number (G_OBJECT (item2));
  int r1 = n1 % 2;
  int r2 = n2 % 2;

  if (r1 == r2)
    return 0;

  if (r1 == 1)
    return 1;

  return -1;
}

static void
test_multi (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;
  GtkSorter *sorter1;
  GtkSorter *sorter2;
  GtkExpression *expression;
  gpointer item;

  model = new_model (20, NULL);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  sorter2 = GTK_SORTER (gtk_numeric_sorter_new (NULL));
  gtk_sort_list_model_set_sorter (model, sorter2);
  expression = gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL);
  gtk_numeric_sorter_set_expression (GTK_NUMERIC_SORTER (sorter2), expression);
  gtk_expression_unref (expression);

  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  sorter = GTK_SORTER (gtk_multi_sorter_new ());
  gtk_sort_list_model_set_sorter (model, sorter);

  sorter1 = GTK_SORTER (gtk_custom_sorter_new (compare_even, NULL, NULL));
  gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter), sorter1);
  gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter), sorter2);

  g_assert_true (GTK_TYPE_SORTER == g_list_model_get_item_type (G_LIST_MODEL (sorter)));
  g_assert_cmpuint (2, ==, g_list_model_get_n_items (G_LIST_MODEL (sorter)));
  item = g_list_model_get_item (G_LIST_MODEL (sorter), 1);
  g_assert_true (item == sorter2);
  g_object_unref (item);

  assert_model (model, "2 4 6 8 10 12 14 16 18 20 1 3 5 7 9 11 13 15 17 19");

  /* This doesn't do anything */
  gtk_multi_sorter_remove (GTK_MULTI_SORTER (sorter), 12345);
  assert_model (model, "2 4 6 8 10 12 14 16 18 20 1 3 5 7 9 11 13 15 17 19");

  gtk_multi_sorter_remove (GTK_MULTI_SORTER (sorter), 0);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  gtk_multi_sorter_remove (GTK_MULTI_SORTER (sorter), 0);
  assert_not_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  g_object_unref (model);
  g_object_unref (sorter);
}

/* Check that the multi sorter properly disconnects its changed signal */
static void
test_multi_destruct (void)
{
  GtkSorter *multi, *sorter;

  multi = GTK_SORTER (gtk_multi_sorter_new ());
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL)));
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), g_object_ref (sorter));
  g_object_unref (multi);

  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
  g_object_unref (sorter);
}

static void
test_multi_changes (void)
{
  GtkSortListModel *model;
  GtkSorter *multi;
  GtkSorter *sorter1;
  GtkSorter *sorter2;
  GtkSorter *sorter3;
  GtkExpression *expression;
  int counter = 0;

  /* We want a sorted model, so that we can be sure partial sorts do the right thing */
  model = gtk_sort_list_model_new (G_LIST_MODEL (new_store (1, 20, 1)), NULL);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  multi = GTK_SORTER (gtk_multi_sorter_new ());
  g_signal_connect (multi, "changed", G_CALLBACK (inc_counter), &counter);
  gtk_sort_list_model_set_sorter (model, multi);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  g_assert_cmpint (counter, ==, 0);

  sorter1 = GTK_SORTER (gtk_numeric_sorter_new (NULL));
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), sorter1);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  g_assert_cmpint (counter, ==, 1);

  expression = gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number_mod_5, NULL, NULL);
  gtk_numeric_sorter_set_expression (GTK_NUMERIC_SORTER (sorter1), expression);
  gtk_expression_unref (expression);
  assert_model (model, "5 10 15 20 1 6 11 16 2 7 12 17 3 8 13 18 4 9 14 19");
  g_assert_cmpint (counter, ==, 2);

  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter1), GTK_SORT_DESCENDING);
  assert_model (model, "4 9 14 19 3 8 13 18 2 7 12 17 1 6 11 16 5 10 15 20");
  g_assert_cmpint (counter, ==, 3);

  sorter2 = GTK_SORTER (gtk_custom_sorter_new (compare_even, NULL, NULL));
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), sorter2);
  assert_model (model, "4 14 9 19 8 18 3 13 2 12 7 17 6 16 1 11 10 20 5 15");
  g_assert_cmpint (counter, ==, 4);

  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter1), GTK_SORT_ASCENDING);
  assert_model (model, "10 20 5 15 6 16 1 11 2 12 7 17 8 18 3 13 4 14 9 19");
  g_assert_cmpint (counter, ==, 5);

  sorter3 = GTK_SORTER (gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_spelled_out, NULL, NULL)));
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), sorter3);
  assert_model (model, "10 20 15 5 6 16 11 1 12 2 7 17 8 18 13 3 4 14 9 19");
  g_assert_cmpint (counter, ==, 6);

  gtk_multi_sorter_remove (GTK_MULTI_SORTER (multi), 1);
  assert_model (model, "15 5 10 20 11 1 6 16 7 17 12 2 8 18 13 3 4 14 9 19");
  g_assert_cmpint (counter, ==, 7);

  gtk_multi_sorter_remove (GTK_MULTI_SORTER (multi), 1);
  assert_model (model, "5 10 15 20 1 6 11 16 2 7 12 17 3 8 13 18 4 9 14 19");
  g_assert_cmpint (counter, ==, 8);

  gtk_multi_sorter_remove (GTK_MULTI_SORTER (multi), 0);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  g_assert_cmpint (counter, ==, 9);

  g_object_unref (multi);
  g_object_unref (model);
}

static GtkSorter *
even_odd_sorter_new (void)
{
  return GTK_SORTER (gtk_custom_sorter_new (compare_even, NULL, NULL));
}

static GtkSorter *
numeric_sorter_new (void)
{
  return GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL)));
}

static void
switch_order (GtkSorter *sorter)
{
  if (gtk_numeric_sorter_get_sort_order (GTK_NUMERIC_SORTER (sorter)) == GTK_SORT_ASCENDING)
    gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
  else
    gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_ASCENDING);
}

static void
set_order_ascending (GtkSorter *sorter)
{
  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_ASCENDING);
}

static void
set_order_descending (GtkSorter *sorter)
{
  gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
}

static void
set_expression_get_number (GtkSorter *sorter)
{
  GtkExpression *expression = gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL);
  gtk_numeric_sorter_set_expression (GTK_NUMERIC_SORTER (sorter), expression);
  gtk_expression_unref (expression);
}

static void
set_expression_get_number_mod_5 (GtkSorter *sorter)
{
  GtkExpression *expression = gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL);
  gtk_numeric_sorter_set_expression (GTK_NUMERIC_SORTER (sorter), expression);
  gtk_expression_unref (expression);
}

static void
modify_sorter (GtkSorter *multi)
{
  struct {
    GType type;
    GtkSorter * (* create_func) (void);
    void (* modify_func) (GtkSorter *);
  } options[] = {
    { GTK_TYPE_CUSTOM_SORTER, even_odd_sorter_new, NULL },
    { GTK_TYPE_NUMERIC_SORTER, numeric_sorter_new, switch_order },
    { GTK_TYPE_NUMERIC_SORTER, numeric_sorter_new, set_order_ascending },
    { GTK_TYPE_NUMERIC_SORTER, numeric_sorter_new, set_order_descending },
    { GTK_TYPE_NUMERIC_SORTER, numeric_sorter_new, set_expression_get_number, },
    { GTK_TYPE_NUMERIC_SORTER, numeric_sorter_new, set_expression_get_number_mod_5, }
  };
  GtkSorter *current;
  guint option;

  current = g_list_model_get_item (G_LIST_MODEL (multi), 0);
  option = g_random_int_range (0, G_N_ELEMENTS (options));

  if (current == NULL || options[option].type != G_OBJECT_TYPE (current) || options[option].modify_func == NULL)
    {
      g_clear_object (&current);
      gtk_multi_sorter_remove (GTK_MULTI_SORTER (multi), 0);
      
      current = options[option].create_func ();
      if (options[option].modify_func)
        options[option].modify_func (current);

      gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), current);
    }
  else
    {
      options[option].modify_func (current);
    }
}

static void
test_stable (void)
{
  GtkSortListModel *model1, *model2, *model2b;
  GtkSorter *multi, *a, *b;
  guint i;

  a = GTK_SORTER (gtk_multi_sorter_new ());
  b = GTK_SORTER (gtk_multi_sorter_new ());
  /* We create 2 setups:
   * 1. sortmodel (multisorter [a, b])
   * 2. sortmodel (b) => sortmodel (a)
   * Given stability of the sort, these 2 setups should always produce the
   * same results, namely the list should be sorter by a before it's sorted
   * by b.
   *
   * All we do is make a and b random sorters and assert that the 2 setups
   * produce the same order every time.
   */
  multi = GTK_SORTER (gtk_multi_sorter_new ());
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), a);
  gtk_multi_sorter_append (GTK_MULTI_SORTER (multi), b);
  model1 = new_model (20, multi);
  g_object_unref (multi);
  model2b = gtk_sort_list_model_new (g_object_ref (gtk_sort_list_model_get_model (model1)), g_object_ref (b));
  model2 = gtk_sort_list_model_new (g_object_ref (G_LIST_MODEL (model2b)), g_object_ref (a));
  assert_model_equal (model1, model2);

  modify_sorter (a);
  assert_model_equal (model1, model2);
  modify_sorter (b);
  assert_model_equal (model1, model2);

  for (i = 0; i < 100; i++)
    {
      modify_sorter (g_random_boolean () ? a : b);
      assert_model_equal (model1, model2);
    }

  g_object_unref (model1);
  g_object_unref (model2);
  g_object_unref (model2b);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Like a trashcan fire in a prison cell");

  g_test_add_func ("/sorter/simple", test_simple);
  g_test_add_func ("/sorter/string", test_string);
  g_test_add_func ("/sorter/change", test_change);
  g_test_add_func ("/sorter/numeric", test_numeric);
  g_test_add_func ("/sorter/multi", test_multi);
  g_test_add_func ("/sorter/multi-destruct", test_multi_destruct);
  g_test_add_func ("/sorter/multi-changes", test_multi_changes);
  g_test_add_func ("/sorter/stable", test_stable);

  return g_test_run ();
}
