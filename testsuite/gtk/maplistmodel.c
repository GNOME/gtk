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

#if 0
static void
splice (GListStore *store,
        guint       pos,
        guint       removed,
        guint      *numbers,
        guint       added)
{
  GObject *objects[added];
  guint i;

  for (i = 0; i < added; i++)
    {
      /* 0 cannot be differentiated from NULL, so don't use it */
      g_assert_true (numbers[i] != 0);
      objects[i] = g_object_new (G_TYPE_OBJECT, NULL);
      g_object_set_qdata (objects[i], number_quark, GUINT_TO_POINTER (numbers[i]));
    }

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}
#endif

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
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static gpointer
map_multiply (gpointer item,
              gpointer factor)
{
  GObject *object;
  guint num;

  object = g_object_new (G_TYPE_OBJECT, NULL);
  num = GPOINTER_TO_UINT (g_object_get_qdata (item, number_quark));
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (GPOINTER_TO_UINT (factor) * num));
  g_object_unref (item);

  return object;
}

static GtkMapListModel *
new_model (GListStore *store)
{
  GtkMapListModel *result;
  GString *changes;

  if (store)
    g_object_ref (store);
  result = gtk_map_list_model_new (G_LIST_MODEL (store), map_multiply, GUINT_TO_POINTER (2), NULL);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

  return result;
}

static void
test_create_empty (void)
{
  GtkMapListModel *map;

  map = new_model (NULL);
  assert_model (map, "");
  assert_changes (map, "");

  g_object_unref (map);
}

static void
test_create (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  map = new_model (store);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  g_object_unref (store);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  g_object_unref (map);
}

static void
test_set_model (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  map = new_model (NULL);
  assert_model (map, "");
  assert_changes (map, "");

  store = new_store (1, 5, 1);
  gtk_map_list_model_set_model (map, G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "0+5");

  gtk_map_list_model_set_model (map, NULL);
  assert_model (map, "");
  assert_changes (map, "0-5");

  g_object_unref (store);
  g_object_unref (map);
}

static void
test_set_map_func (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  map = new_model (store);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  gtk_map_list_model_set_map_func (map, map_multiply, GUINT_TO_POINTER (3), NULL);
  assert_model (map, "3 6 9 12 15");
  assert_changes (map, "0-5+5");

  gtk_map_list_model_set_map_func (map, NULL, NULL, NULL);
  assert_model (map, "1 2 3 4 5");
  assert_changes (map, "0-5+5");

  gtk_map_list_model_set_map_func (map, map_multiply, GUINT_TO_POINTER (2), NULL);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "0-5+5");

  g_object_unref (store);
  g_object_unref (map);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/maplistmodel/create_empty", test_create_empty);
  g_test_add_func ("/maplistmodel/create", test_create);
  g_test_add_func ("/maplistmodel/set-model", test_set_model);
  g_test_add_func ("/maplistmodel/set-map-func", test_set_map_func);

  return g_test_run ();
}
