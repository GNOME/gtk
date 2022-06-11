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
static GQuark changes_quark;

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  guint number;
  g_assert_nonnull (object);
  number = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
  g_object_unref (object);
  return number;
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

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define ignore_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  g_string_set_size (changes, 0); \
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

static void
items_changed (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               GString    *changes)
{
  g_assert_true (removed != 0 || added != 0);

  if (changes->len)
    g_string_append (changes, ", ");

  if (removed == 1 && added == 0)
    {
      g_string_append_printf (changes, "-%u", position);
    }
  else if (removed == 0 && added == 1)
    {
      g_string_append_printf (changes, "+%u", position);
    }
  else
    {
      g_string_append_printf (changes, "%u", position);
      if (removed > 0)
        g_string_append_printf (changes, "-%u", removed);
      if (added > 0)
        g_string_append_printf (changes, "+%u", added);
    }
}

static void
notify_n_items (GObject    *object,
                GParamSpec *pspec,
                GString    *changes)
{
  g_string_append_c (changes, '*');
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkFilterListModel *
new_model (guint               size,
           GtkCustomFilterFunc filter_func,
           gpointer            data)
{
  GtkFilterListModel *result;
  GtkFilter *filter;
  GString *changes;

  if (filter_func)
    filter = GTK_FILTER (gtk_custom_filter_new (filter_func, data, NULL));
  else
    filter = NULL;
  result = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (new_store (1, size, 1))), filter);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static gboolean
is_smaller_than (gpointer item,
                 gpointer data)
{
  return g_object_get_qdata (item, number_quark) < data;
}

static gboolean
is_larger_than (gpointer item,
                gpointer data)
{
  return g_object_get_qdata (item, number_quark) > data;
}

static gboolean
is_near (gpointer item,
         gpointer data)
{
  return ABS (GPOINTER_TO_INT (g_object_get_qdata (item, number_quark)) - GPOINTER_TO_INT (data)) <= 2;
}

static gboolean
is_not_near (gpointer item,
             gpointer data)
{
  return ABS (GPOINTER_TO_INT (g_object_get_qdata (item, number_quark)) - GPOINTER_TO_INT (data)) > 2;
}

static void
test_create (void)
{
  GtkFilterListModel *filter;
  
  filter = new_model (10, NULL, NULL);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (20));
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (7));
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, is_smaller_than, GUINT_TO_POINTER (0));
  assert_model (filter, "");
  assert_changes (filter, "");
  g_object_unref (filter);
}

static void
test_empty_set_filter (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (20), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (7), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "6-4*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (0), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "0-10*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (0), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (3), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "4 5 6 7 8 9 10");
  assert_changes (filter, "0-3*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (20), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "0-10*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_near, GUINT_TO_POINTER (5), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "3 4 5 6 7");
  assert_changes (filter, "0-10+5*");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (5), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "2-5*");
  g_object_unref (filter);
}

static void
test_change_filter (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;
  
  filter = new_model (10, is_not_near, GUINT_TO_POINTER (5));
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (6), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 9 10");
  assert_changes (filter, "2-1+1");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (9), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "3-2+3*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_smaller_than, GUINT_TO_POINTER (6), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 2 3 4 5");
  assert_changes (filter, "-5*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_larger_than, GUINT_TO_POINTER (4), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "0-5+6*");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (2), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_not_near, GUINT_TO_POINTER (4), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "1 7 8 9 10");
  assert_changes (filter, "0-2+1*");

  g_object_unref (filter);
}

static void
test_incremental (void)
{
  GtkFilterListModel *filter;
  GtkFilter *custom;

  /* everything is filtered */
  filter = new_model (1000, is_larger_than, GUINT_TO_POINTER (10000));
  gtk_filter_list_model_set_incremental (filter, TRUE);
  assert_model (filter, "");
  assert_changes (filter, "");

  custom = GTK_FILTER (gtk_custom_filter_new (is_near, GUINT_TO_POINTER (512), NULL));
  gtk_filter_list_model_set_filter (filter, custom);
  g_object_unref (custom);
  assert_model (filter, "");
  assert_changes (filter, "");

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, TRUE);
  assert_model (filter, "510 511 512 513 514");
  /* implementation detail */
  ignore_changes (filter);

  g_object_unref (filter);
}

static void
test_empty (void)
{
  GtkFilterListModel *filter;
  GListStore *store;
  GtkFilter *f;

  filter = gtk_filter_list_model_new (NULL, NULL);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  store = g_list_store_new (G_TYPE_OBJECT);
  gtk_filter_list_model_set_model (filter, G_LIST_MODEL (store));
  g_object_unref (store);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  f = GTK_FILTER (gtk_every_filter_new ());
  gtk_filter_list_model_set_filter (filter, f);
  g_object_unref (f);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (filter), 11));

  g_object_unref (filter);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/filterlistmodel/create", test_create);
  g_test_add_func ("/filterlistmodel/empty_set_filter", test_empty_set_filter);
  g_test_add_func ("/filterlistmodel/change_filter", test_change_filter);
  g_test_add_func ("/filterlistmodel/incremental", test_incremental);
  g_test_add_func ("/filterlistmodel/empty", test_empty);

  return g_test_run ();
}
