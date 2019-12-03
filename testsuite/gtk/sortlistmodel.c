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
  g_assert (object != NULL);
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

static void
splice (GListStore *store,
        guint       pos,
        guint       removed,
        guint      *numbers,
        guint       added)
{
  GObject **objects = g_newa (GObject *, added);
  guint i;

  for (i = 0; i < added; i++)
    {
      /* 0 cannot be differentiated from NULL, so don't use it */
      g_assert (numbers[i] != 0);
      objects[i] = g_object_new (G_TYPE_OBJECT, NULL);
      g_object_set_qdata (objects[i], number_quark, GUINT_TO_POINTER (numbers[i]));
    }

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}

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
new_store (guint *numbers)
{
  GListStore *store = new_empty_store ();
  guint i;

  for (i = 0; numbers[i] != 0; i++)
    add (store, numbers[i]);

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

static int
compare_modulo (gconstpointer first,
                gconstpointer second,
                gpointer      modulo)
{
  guint mod = GPOINTER_TO_UINT (modulo);

  return (GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (first), number_quark)) % mod)
      -  (GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (second), number_quark)) % mod);
}

static int
compare (gconstpointer first,
         gconstpointer second,
         gpointer      unused)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (first), number_quark))
      -  GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (second), number_quark));
}

static GtkSortListModel *
new_model (gpointer model)
{
  GtkSortListModel *result;
  GString *changes;

  g_assert (model == NULL || G_IS_LIST_MODEL (model));

  if (model)
    {
      GtkSorter *sorter;

      sorter = gtk_custom_sorter_new (compare, NULL, NULL);
      result = gtk_sort_list_model_new (model, sorter);
      g_object_unref (sorter);
    }
  else
    result = gtk_sort_list_model_new_for_type (G_TYPE_OBJECT);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

  return result;
}

static void
test_create_empty (void)
{
  GtkSortListModel *sort;

  sort = new_model (NULL);
  assert_model (sort, "");
  assert_changes (sort, "");

  g_object_unref (sort);
}

static void
test_create (void)
{
  GtkSortListModel *sort;
  GListStore *store;
  
  store = new_store ((guint[]) { 4, 8, 2, 6, 10, 0 });
  sort = new_model (store);
  assert_model (sort, "2 4 6 8 10");
  assert_changes (sort, "");

  g_object_unref (store);
  assert_model (sort, "2 4 6 8 10");
  assert_changes (sort, "");

  g_object_unref (sort);
}

static void
test_set_model (void)
{
  GtkSortListModel *sort;
  GListStore *store;
  
  sort = new_model (NULL);
  assert_model (sort, "");
  assert_changes (sort, "");

  store = new_store ((guint[]) { 4, 8, 2, 6, 10, 0 });
  gtk_sort_list_model_set_model (sort, G_LIST_MODEL (store));
  assert_model (sort, "4 8 2 6 10");
  assert_changes (sort, "0+5");

  gtk_sort_list_model_set_model (sort, NULL);
  assert_model (sort, "");
  assert_changes (sort, "0-5");

  g_object_unref (sort);


  sort = new_model (store);
  assert_model (sort, "2 4 6 8 10");
  assert_changes (sort, "");

  gtk_sort_list_model_set_model (sort, NULL);
  assert_model (sort, "");
  assert_changes (sort, "0-5");

  gtk_sort_list_model_set_model (sort, G_LIST_MODEL (store));
  assert_model (sort, "2 4 6 8 10");
  assert_changes (sort, "0+5");

  g_object_unref (store);
  g_object_unref (sort);
}

static void
test_set_sorter (void)
{
  GtkSortListModel *sort;
  GtkSorter *sorter;
  GListStore *store;
  
  store = new_store ((guint[]) { 4, 8, 2, 6, 10, 0 });
  sort = new_model (store);
  assert_model (sort, "2 4 6 8 10");
  assert_changes (sort, "");

  sorter = gtk_custom_sorter_new (compare_modulo, GUINT_TO_POINTER (5), NULL);
  gtk_sort_list_model_set_sorter (sort, sorter);
  g_object_unref (sorter);
  assert_model (sort, "10 6 2 8 4");
  assert_changes (sort, "0-5+5");

  gtk_sort_list_model_set_sorter (sort, NULL);
  assert_model (sort, "4 8 2 6 10");
  assert_changes (sort, "0-5+5");

  sorter = gtk_custom_sorter_new (compare, NULL, NULL);
  gtk_sort_list_model_set_sorter (sort, sorter);
  g_object_unref (sorter);
  assert_model (sort, "2 4 6 8 10");
  /* Technically, this is correct, but we shortcut setting the sort func:
   * assert_changes (sort, "0-4+4"); */
  assert_changes (sort, "0-5+5");

  g_object_unref (store);
  g_object_unref (sort);
}

static void
test_add_items (void)
{
  GtkSortListModel *sort;
  GListStore *store;
  
  /* add beginning */
  store = new_store ((guint[]) { 51, 99, 100, 49, 50, 0 });
  sort = new_model (store);
  assert_model (sort, "49 50 51 99 100");
  assert_changes (sort, "");
  splice (store, 4, 0, (guint[]) { 1,  2 }, 2);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "0+2");
  g_object_unref (store);
  g_object_unref (sort);

  /* add middle */
  store = new_store ((guint[]) { 99, 100, 1, 2, 0 });
  sort = new_model (store);
  assert_model (sort, "1 2 99 100");
  assert_changes (sort, "");
  splice (store, 2, 0, (guint[]) { 49, 50, 51 }, 3);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "2+3");
  g_object_unref (store);
  g_object_unref (sort);

  /* add end */
  store = new_store ((guint[]) { 51, 49, 1, 2, 50, 0 });
  sort = new_model (store);
  assert_model (sort, "1 2 49 50 51");
  assert_changes (sort, "");
  splice (store, 1, 0, (guint[]) { 99, 100 }, 2);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "5+2");
  g_object_unref (store);
  g_object_unref (sort);
}

static void
test_remove_items (void)
{
  GtkSortListModel *sort;
  GListStore *store;
  
  /* remove beginning */
  store = new_store ((guint[]) { 51, 99, 100, 49, 1, 2, 50, 0 });
  sort = new_model (store);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "");
  splice (store, 4, 2, NULL, 0);
  assert_model (sort, "49 50 51 99 100");
  assert_changes (sort, "0-2");
  g_object_unref (store);
  g_object_unref (sort);

  /* remove middle */
  store = new_store ((guint[]) { 99, 100, 51, 49, 50, 1, 2, 0 });
  sort = new_model (store);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "");
  splice (store, 2, 3, NULL, 0);
  assert_model (sort, "1 2 99 100");
  assert_changes (sort, "2-3");
  g_object_unref (store);
  g_object_unref (sort);

  /* remove end */
  store = new_store ((guint[]) { 51, 99, 100, 49, 1, 2, 50, 0 });
  sort = new_model (store);
  assert_model (sort, "1 2 49 50 51 99 100");
  assert_changes (sort, "");
  splice (store, 1, 2, NULL, 0);
  assert_model (sort, "1 2 49 50 51");
  assert_changes (sort, "5-2");
  g_object_unref (store);
  g_object_unref (sort);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/sortlistmodel/create_empty", test_create_empty);
  g_test_add_func ("/sortlistmodel/create", test_create);
  g_test_add_func ("/sortlistmodel/set-model", test_set_model);
  g_test_add_func ("/sortlistmodel/set-sorter", test_set_sorter);
#if GLIB_CHECK_VERSION (2, 58, 0) /* g_list_store_splice() is broken before 2.58 */
  g_test_add_func ("/sortlistmodel/add_items", test_add_items);
  g_test_add_func ("/sortlistmodel/remove_items", test_remove_items);
#endif

  return g_test_run ();
}
