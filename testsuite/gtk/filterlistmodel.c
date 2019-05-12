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
  g_assert (object != NULL);
  return GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
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

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
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
  g_assert (removed != 0 || added != 0);

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
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkFilterListModel *
new_model (guint                        size,
           GtkFilterListModelFilterFunc filter_func,
           gpointer                     data)
{
  GtkFilterListModel *result;
  GString *changes;

  result = gtk_filter_list_model_new (G_LIST_MODEL (new_store (1, size, 1)), filter_func, data, NULL);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

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
test_empty_set_filter_func (void)
{
  GtkFilterListModel *filter;
  
  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_smaller_than, GUINT_TO_POINTER (20), NULL);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_smaller_than, GUINT_TO_POINTER (7), NULL);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "6-4");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_smaller_than, GUINT_TO_POINTER (0), NULL);
  assert_model (filter, "");
  assert_changes (filter, "0-10");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_larger_than, GUINT_TO_POINTER (0), NULL);
  assert_model (filter, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (filter, "");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_larger_than, GUINT_TO_POINTER (3), NULL);
  assert_model (filter, "4 5 6 7 8 9 10");
  assert_changes (filter, "0-3");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_larger_than, GUINT_TO_POINTER (20), NULL);
  assert_model (filter, "");
  assert_changes (filter, "0-10");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_near, GUINT_TO_POINTER (5), NULL);
  assert_model (filter, "3 4 5 6 7");
  assert_changes (filter, "0-10+5");
  g_object_unref (filter);

  filter = new_model (10, NULL, NULL);
  gtk_filter_list_model_set_filter_func (filter, is_not_near, GUINT_TO_POINTER (5), NULL);
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "2-5");
  g_object_unref (filter);
}

static void
test_change_filter_func (void)
{
  GtkFilterListModel *filter;
  
  filter = new_model (10, is_not_near, GUINT_TO_POINTER (5));
  assert_model (filter, "1 2 8 9 10");
  assert_changes (filter, "");

  gtk_filter_list_model_set_filter_func (filter, is_not_near, GUINT_TO_POINTER (6), NULL);
  assert_model (filter, "1 2 3 9 10");
  assert_changes (filter, "2-1+1");

  gtk_filter_list_model_set_filter_func (filter, is_not_near, GUINT_TO_POINTER (9), NULL);
  assert_model (filter, "1 2 3 4 5 6");
  assert_changes (filter, "3-2+3");

  gtk_filter_list_model_set_filter_func (filter, is_smaller_than, GUINT_TO_POINTER (6), NULL);
  assert_model (filter, "1 2 3 4 5");
  assert_changes (filter, "-5");

  gtk_filter_list_model_set_filter_func (filter, is_larger_than, GUINT_TO_POINTER (4), NULL);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "0-5+6");

  gtk_filter_list_model_set_filter_func (filter, is_not_near, GUINT_TO_POINTER (2), NULL);
  assert_model (filter, "5 6 7 8 9 10");
  assert_changes (filter, "");

  gtk_filter_list_model_set_filter_func (filter, is_not_near, GUINT_TO_POINTER (4), NULL);
  assert_model (filter, "1 7 8 9 10");
  assert_changes (filter, "0-2+1");

  g_object_unref (filter);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/filterlistmodel/create", test_create);
  g_test_add_func ("/filterlistmodel/empty_set_filter_func", test_empty_set_filter_func);
  g_test_add_func ("/filterlistmodel/change_filter_func", test_change_filter_func);

  return g_test_run ();
}
