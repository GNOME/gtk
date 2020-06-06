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
  GObject **objects;
  guint i;

  objects = g_new0 (GObject *, added);

  for (i = 0; i < added; i++)
    objects[i] = make_object (numbers[i]);

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);

  g_free (objects);
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

#define ignore_selection_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_selection_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
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
new_model (GListStore *store)
{
  GtkSelectionModel *result;
  GString *changes;

  result = GTK_SELECTION_MODEL (gtk_multi_selection_new (G_LIST_MODEL (store)));

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
  
  store = new_store (1, 5, 2);
  selection = new_model (store);

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
  
  store = new_store (1, 5, 1);
  selection = new_model (store);
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

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_selection (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;
  
  store = new_store (1, 5, 1);
  selection = new_model (store);
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_item (selection, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "4");
  assert_selection_changes (selection, "3:1");

  ret = gtk_selection_model_unselect_item (selection, 3);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "3:1");

  ret = gtk_selection_model_select_item (selection, 1, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "1:1");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 4 5");
  assert_selection_changes (selection, "3:2");

  ret = gtk_selection_model_unselect_range (selection, 3, 2);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "3:2");

  ret = gtk_selection_model_select_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "1 2 3 4 5");
  assert_selection_changes (selection, "0:5");

  ret = gtk_selection_model_unselect_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "0:5");

  g_object_unref (store);
  g_object_unref (selection);
}

/* Verify that select_range with exclusive = TRUE
 * sends a selection-changed signal that covers
 * preexisting items that got unselected
 */
static void
test_select_range (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);
  selection = new_model (store);
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 2, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "3 4");
  assert_selection_changes (selection, "2:2");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "3 4 5");
  assert_selection_changes (selection, "3:2");

  ret = gtk_selection_model_select_range (selection, 0, 1, TRUE);
  g_assert_true (ret);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "0:5");

  g_object_unref (store);
  g_object_unref (selection);
}

/* Test that removing and readding items
 * clears the selected state.
 */
static void
test_readd (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);

  selection = new_model (store);
  assert_model (selection, "1 2 3 4 5");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 2, 2, FALSE);
  g_assert_true (ret);
  assert_model (selection, "1 2 3 4 5");
  assert_selection (selection, "3 4");
  assert_selection_changes (selection, "2:2");

  g_list_model_items_changed (G_LIST_MODEL (store), 1, 3, 3);
  assert_changes (selection, "1-3+3");
  assert_selection (selection, "");

  g_object_unref (store);
  g_object_unref (selection);
}

typedef struct {
  guint start;
  guint n;
  gboolean in;
} SelectionData;

static void
select_some (guint position,
             guint *start,
             guint *n,
             gboolean *selected,
             gpointer data)
{
  SelectionData *sdata = data;
  guint i;

  for (i = 0; sdata[i].n != 0; i++)
    {
      if (sdata[i].start <= position &&
          position < sdata[i].start + sdata[i].n)
        break;
    }

  *start = sdata[i].start;
  *n = sdata[i].n;
  *selected = sdata[i].in;
}

static void
test_callback (void)
{
  GtkSelectionModel *selection;
  gboolean ret;
  GListStore *store;
  SelectionData data[] = {
    { 0, 2, FALSE },
    { 2, 3, TRUE },
    { 5, 2, FALSE },
    { 6, 3, TRUE },
    { 9, 1, FALSE },
    { 0, 0, FALSE }
  };

  SelectionData more_data[] = {
    { 0, 3, FALSE },
    { 3, 1, TRUE },
    { 4, 3, FALSE },
    { 7, 1, TRUE },
    { 0, 0, FALSE }
  };

  store = new_store (1, 10, 1);

  selection = new_model (store);
  assert_model (selection, "1 2 3 4 5 6 7 8 9 10");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_callback (selection, select_some, data);
  g_assert_true (ret);
  assert_selection (selection, "3 4 5 7 8 9");
  assert_selection_changes (selection, "2:7");

  ret = gtk_selection_model_unselect_callback (selection, select_some, more_data);
  g_assert_true (ret);
  assert_selection (selection, "3 5 7 9");
  assert_selection_changes (selection, "3:5");

  g_object_unref (store);
  g_object_unref (selection);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");
  selection_quark = g_quark_from_static_string ("Mana mana, badibidibi");

  g_test_add_func ("/multiselection/create", test_create);
#if GLIB_CHECK_VERSION (2, 58, 0) /* g_list_store_splice() is broken before 2.58 */
  g_test_add_func ("/multiselection/changes", test_changes);
#endif
  g_test_add_func ("/multiselection/selection", test_selection);
  g_test_add_func ("/multiselection/select-range", test_select_range);
  g_test_add_func ("/multiselection/readd", test_readd);
  g_test_add_func ("/multiselection/callback", test_callback);

  return g_test_run ();
}
