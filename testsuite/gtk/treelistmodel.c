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
prepend (GListStore *store,
         guint       number,
         guint       step)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  if (step / 10)
    object = G_OBJECT (new_store (number - 9 * step / 10, number, step / 10));
  else
    object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_insert (store, 0, object);
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
    prepend (store, i, step);

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

static GListModel *
create_sub_model_cb (gpointer item,
                     gpointer unused)
{
  if (G_IS_LIST_MODEL (item))
    return g_object_ref (item);

  return NULL;
}

static GtkTreeListModel *
new_model (guint    size,
           gboolean expanded)
{
  GtkTreeListModel *tree;
  GString *changes;

  tree = gtk_tree_list_model_new (G_LIST_MODEL (new_store (size, size, size)), TRUE, expanded, create_sub_model_cb, NULL, NULL);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(tree), changes_quark, changes, free_changes);
  g_signal_connect (tree, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (tree, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return tree;
}

static void
test_expand (void)
{
  GtkTreeListModel *tree = new_model (100, FALSE);
  guint i;

  assert_model (tree, "100");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 90 80 70 60 50 40 30 20 10");
  assert_changes (tree, "1+10*");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "11+10*, 10+10*, 9+10*, 8+10*, 7+10*, 6+10*, 5+10*, 4+10*, 3+10*, 2+10*");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "");

  g_object_unref (tree);
}

static void
test_remove_some (void)
{
  GtkTreeListModel *tree = new_model (100, TRUE);
  gpointer item;

  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 1);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 3);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "-5*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 0);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 3);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "33-11*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 88);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 9);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2");
  assert_changes (tree, "-98*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 0);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 8);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11");
  assert_changes (tree, "88-10*");

  g_object_unref (tree);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/treelistmodel/expand", test_expand);
  g_test_add_func ("/treelistmodel/remove_some", test_remove_some);

  return g_test_run ();
}
