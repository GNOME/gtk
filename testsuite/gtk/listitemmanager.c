/*
 * Copyright © 2023 Benjamin Otte
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
#include "gtk/gtklistitemmanagerprivate.h"
#include "gtk/gtklistbaseprivate.h"

static GListModel *
create_source_model (guint min_size, guint max_size)
{
  GtkStringList *list;
  guint i, size;

  size = g_test_rand_int_range (min_size, max_size + 1);
  list = gtk_string_list_new (NULL);

  for (i = 0; i < size; i++)
    gtk_string_list_append (list, g_test_rand_bit () ? "A" : "B");

  return G_LIST_MODEL (list);
}

static void
check_list_item_manager (GtkListItemManager *items)
{
  GListModel *model = G_LIST_MODEL (gtk_list_item_manager_get_model (items));
  GtkListTile *tile;
  guint n_items = 0;

  for (tile = gtk_list_item_manager_get_first (items);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        {
          GObject *item = g_list_model_get_item (model, n_items);
          g_assert_cmphex (GPOINTER_TO_SIZE (item), ==, GPOINTER_TO_SIZE (gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (tile->widget))));
          g_object_unref (item);
          g_assert_cmpint (n_items, ==, gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (tile->widget)));
        }
      if (tile->n_items)
        n_items += tile->n_items;
    }

  g_assert_cmpint (n_items, ==, g_list_model_get_n_items (model));
}

static GtkListTile *
split_simple (GtkWidget   *widget,
              GtkListTile *tile,
              guint        n_items)
{
  GtkListItemManager *items = g_object_get_data (G_OBJECT (widget), "the-items");

  return gtk_list_tile_split (items, tile, n_items);
}

static GtkListItemBase *
create_simple (GtkWidget *widget)
{
  return g_object_new (GTK_TYPE_LIST_BASE, NULL);
}

static void
test_create (void)
{
  GtkListItemManager *items;
  GtkWidget *widget;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_create_with_items (void)
{
  GListModel *source;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  source = create_source_model (1, 50);
  selection = gtk_no_selection_new (G_LIST_MODEL (source));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));

  g_object_unref (selection);
  gtk_window_destroy (GTK_WINDOW (widget));
}

static void
test_exhaustive (void)
{
  GListStore *store;
  GtkFlattenListModel *flatten;
  GtkNoSelection *selection;
  GtkListItemManager *items;
  GtkWidget *widget;
  gsize i;

  widget = gtk_window_new ();
  items = gtk_list_item_manager_new (widget,
                                     split_simple,
                                     create_simple);
  g_object_set_data_full (G_OBJECT (widget), "the-items", items, g_object_unref);

  store = g_list_store_new (G_TYPE_OBJECT);
  flatten = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  selection = gtk_no_selection_new (G_LIST_MODEL (flatten));
  gtk_list_item_manager_set_model (items, GTK_SELECTION_MODEL (selection));

  for (i = 0; i < 500; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position;

      switch (g_test_rand_int_range (0, 4))
      {
        case 0:
          check_list_item_manager (items);
          break;

        case 1:
          /* remove a model */
          remove = TRUE;
          break;

        case 2:
          /* add a model */
          add = TRUE;
          break;

        case 3:
          /* replace a model */
          remove = TRUE;
          add = TRUE;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the filters will see no changes */
          GListModel *source = create_source_model (1, 50);
          g_list_store_splice (store,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store, position);
        }
    }

  check_list_item_manager (items);

  g_object_unref (selection);
  gtk_window_destroy (GTK_WINDOW (widget));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/listitemmanager/create", test_create);
  g_test_add_func ("/listitemmanager/create_with_items", test_create_with_items);
  g_test_add_func ("/listitemmanager/exhaustive", test_exhaustive);

  return g_test_run ();
}