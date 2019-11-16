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

static GtkFilterListModel *
new_model (guint      size,
           GtkFilter *filter)
{
  GtkFilterListModel *result;

  result = gtk_filter_list_model_new (G_LIST_MODEL (new_store (1, size, 1)), filter);

  return result;
}

static gboolean
divisible_by (gpointer item,
              gpointer data)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (item, number_quark)) % GPOINTER_TO_UINT (data) == 0;
}

static void
test_simple (void)
{
  GtkFilterListModel *model;
  GtkFilter *filter;

  filter = gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (3), NULL);
  model = new_model (20, filter);
  g_object_unref (filter);
  assert_model (model, "3 6 9 12 15 18");
  g_object_unref (model);
}

static void
test_any_simple (void)
{
  GtkFilterListModel *model;
  GtkFilter *any, *filter1, *filter2;

  any = gtk_any_filter_new ();
  filter1 = gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (3), NULL);
  filter2 = gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (5), NULL);

  model = new_model (20, any);
  assert_model (model, "");

  gtk_any_filter_append (GTK_ANY_FILTER (any), filter1);
  assert_model (model, "3 6 9 12 15 18");

  gtk_any_filter_append (GTK_ANY_FILTER (any), filter2);
  assert_model (model, "3 5 6 9 10 12 15 18 20");

  gtk_any_filter_remove (GTK_ANY_FILTER (any), 0);
  assert_model (model, "5 10 15 20");

  /* doesn't exist */
  gtk_any_filter_remove (GTK_ANY_FILTER (any), 10);
  assert_model (model, "5 10 15 20");

  gtk_any_filter_remove (GTK_ANY_FILTER (any), 0);
  assert_model (model, "");

  g_object_unref (model);
  g_object_unref (filter2);
  g_object_unref (filter1);
  g_object_unref (any);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");

  g_test_add_func ("/filter/simple", test_simple);
  g_test_add_func ("/filter/any/simple", test_any_simple);

  return g_test_run ();
}
