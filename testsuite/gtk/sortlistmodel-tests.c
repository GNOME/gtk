/*
 * Copyright © 2020 Matthias Clasen
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

#include <gtk/gtk.h>
#include <locale.h>

static GListModel *
get_random_model (guint size)
{
  GListStore *store = g_list_store_new (G_TYPE_OBJECT);
  guint n;
  guint i;

  n = g_random_int_range (size / 2, size);
  for (i = 0; i < n; i++)
    {
      char *string = g_strdup_printf ("%d", g_random_int_range (0, 1000000));
      g_list_store_append (store, gtk_string_object_new (string));
      g_free (string);
    }

  return G_LIST_MODEL (store);
}

static GListModel *
copy_model (GListModel *model)
{
  GListStore *store = g_list_store_new (G_TYPE_OBJECT);
  guint i, n;

  n = g_list_model_get_n_items (model);
  for (i = 0; i < n; i++)
    {
      gpointer item = g_list_model_get_item (model, i);
      g_list_store_append (store, item);
      g_object_unref (item);
    }

  return G_LIST_MODEL (store);
}

static GListModel *
get_aaaa_model (guint size)
{
  GListStore *store = g_list_store_new (G_TYPE_OBJECT);
  guint i;

  for (i = 0; i < size; i++)
    {
      g_list_store_append (store, gtk_string_object_new ("AAA"));
    }

  return G_LIST_MODEL (store);
}

static int
compare_func (gconstpointer a, gconstpointer b, gpointer data)
{
  const GtkStringObject *ao = a;
  const GtkStringObject *bo = b;
  char *ar, *br;
  int ret;

  ar = g_utf8_strreverse (gtk_string_object_get_string ((GtkStringObject*)ao), -1);
  br = g_utf8_strreverse (gtk_string_object_get_string ((GtkStringObject*)bo), -1);
  ret = strcmp (ar, br);
  g_free (ar);
  g_free (br);

  return ret;
}

static GtkSorter *
get_random_string_sorter (void)
{
  GtkSorter *sorter;

  switch (g_random_int_range (0, 3))
    {
    case 0:
      sorter = gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"));
      break;

    case 1:
      sorter = gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string"));
      gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), TRUE);
      break;

    case 2:
      sorter = gtk_custom_sorter_new (compare_func, NULL, NULL);
      break;

    default:
      g_assert_not_reached ();
    }

  return sorter;
}

static void
assert_model_equal (GListModel *model1, GListModel *model2)
{
  guint n1, n2, i;

  n1 = g_list_model_get_n_items (model1);
  n2 = g_list_model_get_n_items (model2);

  g_assert_cmpuint (n1, ==, n2);

  for (i = 0; i < n1; i++)
    {
      GObject *o1, *o2;

      o1 = g_list_model_get_item (model1, i);
      o2 = g_list_model_get_item (model2, i);

      g_assert_cmpstr (gtk_string_object_get_string (GTK_STRING_OBJECT (o1)),
                       ==,
                       gtk_string_object_get_string (GTK_STRING_OBJECT (o2)));

      g_object_unref (o1);
      g_object_unref (o2);
    }
}

static void
assert_model_equal2 (GListModel *model1, GListModel *model2)
{
  guint n1, n2, i;

  n1 = g_list_model_get_n_items (model1);
  n2 = g_list_model_get_n_items (model2);

  g_assert_cmpuint (n1, ==, n2);

  for (i = 0; i < n1; i++)
    {
      GObject *o1, *o2;

      o1 = g_list_model_get_item (model1, i);
      o2 = g_list_model_get_item (model2, i);

      g_assert_true (o1 == o2);

      g_object_unref (o1);
      g_object_unref (o2);
    }
}

static void
test_two_sorters (void)
{
  GListModel *store;
  GListModel *model1, *model2, *model3;
  GtkSorter *sorter1, *sorter2, *sorter3;
  guint i;

  for (i = 0; i < 100; i++)
    {
      store = get_random_model (1000);
      sorter1 = get_random_string_sorter ();
      sorter2 = get_random_string_sorter ();

      model1 = G_LIST_MODEL (gtk_sort_list_model_new (store, sorter1));
      model2 = G_LIST_MODEL (gtk_sort_list_model_new (model1, sorter2));

      sorter3 = gtk_multi_sorter_new ();
      gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter3), g_object_ref (sorter2));
      gtk_multi_sorter_append (GTK_MULTI_SORTER (sorter3), g_object_ref (sorter1));

      model3 = G_LIST_MODEL (gtk_sort_list_model_new (store, sorter3));

      assert_model_equal (model2, model3);

      g_object_unref (model1);
      g_object_unref (model2);
      g_object_unref (model3);
      g_object_unref (sorter1);
      g_object_unref (sorter2);
      g_object_unref (sorter3);
    }
}

static void
test_sort_twice (void)
{
  GListModel *store;
  GListModel *model1, *model2;
  GtkSorter *sorter;
  guint i;

  for (i = 0; i < 100; i++)
    {
      store = get_random_model (1000);
      sorter = get_random_string_sorter ();

      model1 = G_LIST_MODEL (gtk_sort_list_model_new (store, sorter));
      model2 = G_LIST_MODEL (gtk_sort_list_model_new (model1, sorter));

      assert_model_equal (model1, model2);

      g_object_unref (store);
      g_object_unref (model1);
      g_object_unref (model2);
      g_object_unref (sorter);
    }
}

static void
test_stable_sort (void)
{
  GListModel *store;
  GListModel *model;
  GtkSorter *sorter;
  guint i;

  for (i = 0; i < 100; i++)
    {
      store = get_aaaa_model (1000);
      sorter = get_random_string_sorter ();

      model = G_LIST_MODEL (gtk_sort_list_model_new (store, sorter));

      assert_model_equal2 (model, store);

      g_object_unref (store);
      g_object_unref (model);
      g_object_unref (sorter);
    }
}

static void
test_insert_random (void)
{
  GListModel *store1;
  GListModel *store2;
  GtkSorter *sorter;
  GtkSortListModel *s1, *s2;
  guint i, j;

  for (i = 0; i < 20; i++)
    {
      store1 = get_random_model (1000);
      store2 = copy_model (store1);

      sorter = get_random_string_sorter ();

      s1 = gtk_sort_list_model_new (G_LIST_MODEL (store1), sorter);
      s2 = gtk_sort_list_model_new (G_LIST_MODEL (store2), sorter);

      for (j = 0; j < 20; j++)
        {
          char *string = g_strdup_printf ("%d", g_random_int_range (0, 100000));
          GtkStringObject *obj = gtk_string_object_new (string);
          guint pos1 = g_random_int_range (0, g_list_model_get_n_items (store1) + 1);
          guint pos2 = g_random_int_range (0, g_list_model_get_n_items (store2) + 1);

          g_list_store_insert (G_LIST_STORE (store1), pos1, obj);
          g_list_store_insert (G_LIST_STORE (store2), pos2, obj);

          g_object_unref (obj);
          g_free (string);

          assert_model_equal (G_LIST_MODEL (s1), G_LIST_MODEL (s2));
        }

      g_object_unref (s1);
      g_object_unref (s2);

      g_object_unref (sorter);
      g_object_unref (store1);
      g_object_unref (store2);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/sortlistmodel/two-sorters", test_two_sorters);
  g_test_add_func ("/sortlistmodel/sort-twice", test_sort_twice);
  g_test_add_func ("/sortlistmodel/stable-sort", test_stable_sort);
  g_test_add_func ("/sortlistmodel/insert-random", test_insert_random);

  return g_test_run ();
}
