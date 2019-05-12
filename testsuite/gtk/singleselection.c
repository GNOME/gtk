/* 
 * Copyright (C) 2019, Red Hat, Inc.
 * Authors: Matthias Clasen <mclasen@redhat.com>
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
static GQuark selection_quark;

static guint
get (GListModel *model,
     guint       position)
{
  guint number;
  GObject *object = g_list_model_get_item (model, position);
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

static char *
selection_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      if (!gtk_selection_model_is_selected (GTK_SELECTION_MODEL (model), i))
        continue;

      if (string->len > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get (model, i));
    }

  return g_string_free (string, FALSE);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step);

static GObject *
make_object (guint number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert (number != 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));

  return object;
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
    objects[i] = make_object (numbers[i]);

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}

static void
add (GListStore *store,
     guint       number)
{
  GObject *object = make_object (number);
  g_list_store_append (store, object);
  g_object_unref (object);
}

static void
insert (GListStore *store,
        guint position,
        guint number)
{
  GObject *object = make_object (number);
  g_list_store_insert (store, position, object);
  g_object_unref (object);
}

#define assert_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define ignore_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_selection(model, expected) G_STMT_START{ \
  char *s = selection_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define assert_selection_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define ignore_selection_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
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
selection_changed (GListModel *model,
                   guint       position,
                   guint       n_items,
                   GString    *changes)
{
  if (changes->len)
    g_string_append (changes, ", ");

  g_string_append_printf (changes, "%u:%u", position, n_items);
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkSelectionModel *
new_model (GListStore *store, gboolean autoselect, gboolean can_unselect)
{
  GtkSelectionModel *result;
  GString *changes;

  result = GTK_SELECTION_MODEL (gtk_single_selection_new (G_LIST_MODEL (store)));

  /* We want to return an empty selection unless autoselect is true,
   * so undo the initial selection due to autoselect defaulting to TRUE.
   */
  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (result), FALSE);
  gtk_single_selection_set_can_unselect (GTK_SINGLE_SELECTION (result), TRUE);
  gtk_selection_model_unselect_item (result, 0);
  assert_selection (result, "");

  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (result), autoselect);
  gtk_single_selection_set_can_unselect (GTK_SINGLE_SELECTION (result), can_unselect);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), selection_quark, changes, free_changes);
  g_signal_connect (result, "selection-changed", G_CALLBACK (selection_changed), changes);

  return result;
}

static void
test_create (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_store (1, 5, 2);
  selection = new_model (store, FALSE, FALSE);
  g_assert_false (gtk_single_selection_get_autoselect (GTK_SINGLE_SELECTION (selection)));

  assert_model (selection, "1 3 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_object_unref (store);

  assert_model (selection, "1 3 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_object_unref (selection);
}

static void
test_changes (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  if (glib_check_version (2, 58, 0) != NULL)
    {
      g_test_skip ("g_list_store_splice() is broken before GLIB 2.58.0");
      return;
    }

  store = new_store (1, 5, 1);
  selection = new_model (store, FALSE, FALSE);
  assert_model (selection, "1 2 3 4 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_list_store_remove (store, 3);
  assert_model (selection, "1 2 3 5");
  assert_changes (selection, "-3");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  insert (store, 3, 99);
  assert_model (selection, "1 2 3 99 5");
  assert_changes (selection, "+3");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  splice (store, 3, 2, (guint[]) { 97 }, 1);
  assert_model (selection, "1 2 3 97");
  assert_changes (selection, "3-2+1");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_object_unref (selection);
  g_object_unref (store);
}

static void
test_selection (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_store (1, 5, 1);
  selection = new_model (store, TRUE, FALSE);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_item (selection, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "4");
  assert_selection_changes (selection, "0:4");

  ret = gtk_selection_model_unselect_item (selection, 3);
  g_assert_false (ret);
  assert_selection (selection, "4");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_item (selection, 1, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "1:3");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_false (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_unselect_range (selection, 4, 2);
  g_assert_false (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_all (selection);
  g_assert_false (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_unselect_all (selection);
  g_assert_false (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_autoselect (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_empty_store ();
  selection = new_model (store, TRUE, FALSE);
  assert_model (selection, "");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  add (store, 1);
  assert_model (selection, "1");
  assert_changes (selection, "+0");
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  splice (store, 0, 1, (guint[]) { 7, 8, 9 }, 3);
  assert_model (selection, "7 8 9");
  assert_changes (selection, "0-1+3");
  assert_selection (selection, "7");
  assert_selection_changes (selection, "");

  splice (store, 0, 0, (guint[]) { 5, 6 }, 2);
  assert_model (selection, "5 6 7 8 9");
  assert_changes (selection, "0+2");
  assert_selection (selection, "7");
  assert_selection_changes (selection, "");

  g_list_store_remove (store, 2);
  assert_model (selection, "5 6 8 9");
  assert_changes (selection, "2-2+1");
  assert_selection (selection, "8");
  assert_selection_changes (selection, "");

  splice (store, 2, 2, NULL, 0);
  assert_model (selection, "5 6");
  assert_changes (selection, "1-3+1");
  assert_selection (selection, "6");
  assert_selection_changes (selection, "");

  splice (store, 0, 2, (guint[]) { 1, 2 }, 2);
  assert_model (selection, "1 2");
  assert_changes (selection, "0-2+2");
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  g_list_store_remove (store, 0);
  assert_model (selection, "2");
  assert_changes (selection, "-0");
  assert_selection (selection, "2");
  assert_selection_changes (selection, "");

  g_list_store_remove (store, 0);
  assert_model (selection, "");
  assert_changes (selection, "-0");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_autoselect_toggle (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_store (1, 1, 1);
  selection = new_model (store, TRUE, TRUE);
  assert_model (selection, "1");
  assert_changes (selection, "");
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (selection), FALSE);
  assert_model (selection, "1");
  assert_changes (selection, "");
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  gtk_selection_model_unselect_item (selection, 0);
  assert_model (selection, "1");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "0:1");

  gtk_single_selection_set_autoselect (GTK_SINGLE_SELECTION (selection), TRUE);
  assert_model (selection, "1");
  assert_changes (selection, "");
  assert_selection (selection, "1");
  assert_selection_changes (selection, "0:1");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_can_unselect (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;
  
  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_store (1, 5, 1);
  selection = new_model (store, TRUE, FALSE);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_unselect_item (selection, 0);
  g_assert_false (ret);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");

  gtk_single_selection_set_can_unselect (GTK_SINGLE_SELECTION (selection), TRUE);

  assert_selection (selection, "1");
  ret = gtk_selection_model_unselect_item (selection, 0);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "0:1");

  ignore_changes (selection);

  g_object_unref (store);
  g_object_unref (selection);
}

static int
sort_inverse (gconstpointer a, gconstpointer b, gpointer data)
{
  int ia = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (a), number_quark));
  int ib = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (b), number_quark));
  
  return ib - ia;
}

static void
test_persistence (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  if (glib_check_version (2, 59, 0) != NULL)
    {
      g_test_skip ("g_list_store_get_item() has overflow issues before GLIB 2.59.0");
      return;
    }

  store = new_store (1, 5, 1);
  selection = new_model (store, TRUE, FALSE);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "");
  g_assert_true (gtk_selection_model_is_selected (selection, 0));
  g_assert_false (gtk_selection_model_is_selected (selection, 4));

  g_list_store_sort (store, sort_inverse, NULL);

  assert_selection (selection, "1");
  assert_selection_changes (selection, "");
  g_assert_false (gtk_selection_model_is_selected (selection, 0));
  g_assert_true (gtk_selection_model_is_selected (selection, 4));

  ignore_changes (selection);

  g_object_unref (store);
  g_object_unref (selection);
}

static void
check_query_range (GtkSelectionModel *selection)
{
  guint i, j;
  guint position, n_items;
  gboolean selected;

  /* check that range always contains position, and has uniform selection */
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (selection)); i++)
    {
      gtk_selection_model_query_range (selection, i, &position, &n_items, &selected);
      g_assert_cmpint (position, <=, i);
      g_assert_cmpint (i, <, position + n_items);
      for (j = position; j < position + n_items; j++)
        g_assert_true (selected == gtk_selection_model_is_selected (selection, j));
    }
  
  /* check that out-of-range returns the correct invalid values */
  i = MIN (i, g_random_int ());
  gtk_selection_model_query_range (selection, i, &position, &n_items, &selected);
  g_assert_cmpint (position, ==, i);
  g_assert_cmpint (n_items, ==, 0);
  g_assert_true (!selected);
}

static void
test_query_range (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  selection = new_model (store, TRUE, TRUE);
  check_query_range (selection);

  gtk_selection_model_unselect_item (selection, 0);
  check_query_range (selection);

  gtk_selection_model_select_item (selection, 2,  TRUE);
  check_query_range (selection);

  gtk_selection_model_select_item (selection, 4, TRUE);
  check_query_range (selection);

  ignore_selection_changes (selection);

  g_object_unref (store);
  g_object_unref (selection);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");
  selection_quark = g_quark_from_static_string ("Mana mana, badibidibi");

  g_test_add_func ("/singleselection/create", test_create);
  g_test_add_func ("/singleselection/autoselect", test_autoselect);
  g_test_add_func ("/singleselection/autoselect-toggle", test_autoselect_toggle);
  g_test_add_func ("/singleselection/selection", test_selection);
  g_test_add_func ("/singleselection/can-unselect", test_can_unselect);
  g_test_add_func ("/singleselection/persistence", test_persistence);
  g_test_add_func ("/singleselection/query-range", test_query_range);
  g_test_add_func ("/singleselection/changes", test_changes);

  return g_test_run ();
}
