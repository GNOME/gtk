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

#include "../../gtk/gtkconcatmodelprivate.h"

/* _gtk_rbtree_test */

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

static void
add (GListStore *store,
     guint       number)
{
  GObject *object;

  /* o cannot be differentiated from NULL, so don't use it */
  g_assert (number != 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_append (store, object);
  g_object_unref (object);
}

static void
remove (GListStore *store,
        guint       position)
{
  g_list_store_remove (store, position);
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
           guint end)
{
  GListStore *store = new_empty_store ();
  guint i;

  for (i = start; i <= end; i++)
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

static GtkConcatModel *
new_model (void)
{
  GtkConcatModel *model = gtk_concat_model_new (G_TYPE_OBJECT);
  GString *changes;

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(model), changes_quark, changes, free_changes);
  g_signal_connect (model, "items-changed", G_CALLBACK (items_changed), changes);

  return model;
}

static void
test_append (void)
{
  GListStore *store = new_store (1, 3);
  GtkConcatModel *concat = new_model ();

  gtk_concat_model_append (concat, G_LIST_MODEL (store));

  assert_model (concat, "1 2 3");
  assert_changes (concat, "0+3");

  g_object_unref (store);
  g_object_unref (concat);
}

static void
test_append_and_add (void)
{
  GListStore *store = new_empty_store ();
  GtkConcatModel *concat = new_model ();

  gtk_concat_model_append (concat, G_LIST_MODEL (store));

  add (store, 1);
  add (store, 2);
  add (store, 3);
  assert_model (concat, "1 2 3");
  assert_changes (concat, "+0, +1, +2");

  g_object_unref (store);
  g_object_unref (concat);
}

static void
test_append_and_remove (void)
{
  GListStore *store = new_store (1, 3);
  GtkConcatModel *concat = new_model ();

  gtk_concat_model_append (concat, G_LIST_MODEL (store));
  gtk_concat_model_remove (concat, G_LIST_MODEL (store));

  assert_model (concat, "");
  assert_changes (concat, "0+3, 0-3");

  /* Check that all signal handlers are gone */
  g_list_store_remove_all (store);

  g_object_unref (store);
  g_object_unref (concat);
}

static void
test_append_and_remove_items (void)
{
  GListStore *store = new_empty_store ();
  GtkConcatModel *concat = new_model ();

  gtk_concat_model_append (concat, G_LIST_MODEL (store));

  add (store, 1);
  add (store, 2);
  add (store, 3);
  remove (store, 0);
  remove (store, 1);
  remove (store, 0);

  assert_model (concat, "");
  assert_changes (concat, "+0, +1, +2, -0, -1, -0");

  g_object_unref (store);
  g_object_unref (concat);
}

static void
test_append_many (void)
{
  GListStore *store[5] = { new_store (1, 3), new_store (4, 4), new_store (5, 10), new_empty_store (), new_store (11, 20) };
  GtkConcatModel *concat = new_model ();
  guint i;

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    gtk_concat_model_append (concat, G_LIST_MODEL (store[i]));

  assert_model (concat, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
  assert_changes (concat, "0+3, +3, 4+6, 10+10");

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    g_object_unref (store[i]);
  g_object_unref (concat);
}

static void
test_append_many_and_add (void)
{
  GListStore *store[3];
  GtkConcatModel *concat = new_model ();
  guint i, j;

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    {
      store[i] = new_empty_store ();
      gtk_concat_model_append (concat, G_LIST_MODEL (store[i]));
    }

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (store); j++)
        {
          add (store[(i + j) % G_N_ELEMENTS (store)], i * 3 + j + 1);
        }
    }

  assert_model (concat, "1 6 8 2 4 9 3 5 7");
  assert_changes (concat, "+0, +1, +2, +2, +4, +1, +6, +2, +5");

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    g_object_unref (store[i]);
  g_object_unref (concat);
}

static void
test_append_many_and_remove (void)
{
  GListStore *store[5];
  GtkConcatModel *concat = new_model ();
  guint i;

  store[0] = new_empty_store ();
  gtk_concat_model_append (concat, G_LIST_MODEL (store[0]));
  for (i = 1; i < G_N_ELEMENTS (store); i++)
    {
      store[i] = new_store (i * (i - 1) / 2 + 1, i * (i + 1) / 2);
      gtk_concat_model_append (concat, G_LIST_MODEL (store[i]));
    }

  assert_model (concat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (concat, "+0, 1+2, 3+3, 6+4");

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    {
      gtk_concat_model_remove (concat, G_LIST_MODEL (store[(3 * i) % G_N_ELEMENTS (store)]));
    }

  assert_model (concat, "");
  assert_changes (concat, "3-3, -0, 2-4, 0-2");

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    {
      g_list_store_remove_all (store[i]);
      g_object_unref (store[i]);
    }

  g_object_unref (concat);
}

static void
test_append_many_and_remove_items (void)
{
  GListStore *store[5];
  GtkConcatModel *concat = new_model ();
  guint i;

  store[0] = new_empty_store ();
  gtk_concat_model_append (concat, G_LIST_MODEL (store[0]));
  for (i = 1; i < G_N_ELEMENTS (store); i++)
    {
      store[i] = new_store (i * (i - 1) / 2 + 1, i * (i + 1) / 2);
      gtk_concat_model_append (concat, G_LIST_MODEL (store[i]));
    }

  assert_model (concat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (concat, "+0, 1+2, 3+3, 6+4");

  for (i = 1; i < G_N_ELEMENTS (store); i++)
    {
      remove (store[i], 3 % i);
    }

  assert_model (concat, "2 5 6 7 8 9");
  assert_changes (concat, "-0, -1, -1, -6");

  for (i = 0; i < G_N_ELEMENTS (store); i++)
    g_object_unref (store[i]);
  g_object_unref (concat);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/compatmodel/append", test_append);
  g_test_add_func ("/compatmodel/append_and_add", test_append_and_add);
  g_test_add_func ("/compatmodel/append_and_remove", test_append_and_remove);
  g_test_add_func ("/compatmodel/append_and_remove_items", test_append_and_remove_items);
  g_test_add_func ("/compatmodel/append_many", test_append_many);
  g_test_add_func ("/compatmodel/append_many_and_add", test_append_many_and_add);
  g_test_add_func ("/compatmodel/append_many_and_remove", test_append_many_and_remove);
  g_test_add_func ("/compatmodel/append_many_and_remove_items", test_append_many_and_remove_items);

  return g_test_run ();
}
