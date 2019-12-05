/* GtkRBTree tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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
  g_assert (object != NULL);
  number = get_number (object);
  g_object_unref (object);
  return number;
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

static GListStore *
shuffle (GListStore *store)
{
  int i;

  for (i = 0; i < 100; i++)
    {
      int pos = g_random_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)));
      GObject *item = g_list_model_get_item (G_LIST_MODEL (store), pos);
     
      g_list_store_remove (store, pos);
      g_list_store_append (store, item);
      g_object_unref (item); 
    }

  return store;
}

static GtkSortListModel *
new_model (guint      size,
           GtkSorter *sorter)
{
  GtkSortListModel *result;

  result = gtk_sort_list_model_new (G_LIST_MODEL (shuffle (new_store (1, size, 1))), sorter);

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
  char *s;

  model = new_model (20, NULL);

  s = model_to_string (G_LIST_MODEL (model));
  g_assert_cmpstr (s, !=, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  g_free (s);

  sorter = gtk_custom_sorter_new (compare_numbers, NULL, NULL);
  gtk_sort_list_model_set_sorter (model, sorter);
  g_object_unref (sorter);

  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  g_object_unref (model);
}

static void
test_string (void)
{
  GtkSortListModel *model;
  GtkSorter *sorter;
  GtkExpression *expression;
  char *s;

  model = new_model (20, NULL);

  s = model_to_string (G_LIST_MODEL (model));
  g_assert_cmpstr (s, !=, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  g_free (s);

  sorter = gtk_string_sorter_new ();
  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_string, NULL, NULL);
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_expression_unref (expression);

  gtk_sort_list_model_set_sorter (model, sorter);
  g_object_unref (sorter);

  assert_model (model, "1 10 11 12 13 14 15 16 17 18 19 2 20 3 4 5 6 7 8 9");

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL, 0, NULL, (GCallback)get_spelled_out, NULL, NULL);
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_expression_unref (expression);

  assert_model (model, "8 18 11 15 5 4 14 9 19 1 7 17 6 16 10 13 3 12 20 2");

  gtk_expression_unref (expression);
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

  sorter = gtk_string_sorter_new ();
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

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Like a trashcan fire in a prison cell");

  g_test_add_func ("/sorter/simple", test_simple);
  g_test_add_func ("/sorter/string", test_string);
  g_test_add_func ("/sorter/change", test_change);

  return g_test_run ();
}
