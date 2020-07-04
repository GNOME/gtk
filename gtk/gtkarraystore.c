/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkarraystore.h"

#define GTK_VECTOR_ELEMENT_TYPE GObject *
#define GTK_VECTOR_FREE_FUNC g_object_unref
#include "gtkvectorimpl.c"

/**
 * SECTION:gtkarraystore
 * @title: GtkArrayStore
 * @short_description: A simple array implementation of #GListModel
 *
 * #GtkArrayStore is a simple implementation of #GListModel that stores all
 * items in memory.
 *
 * It provides appending, deletions, and lookups in O(1) time and insertions
 * in O(N) time. it is implemented using an array.
 */

/**
 * GtkArrayStore:
 *
 * #GtkArrayStore is an opaque data structure and can only be accessed
 * using the following functions.
 **/

struct _GtkArrayStore
{
  GObject parent_instance;

  GtkVector items;
};

static void gtk_array_store_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkArrayStore, gtk_array_store, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_array_store_iface_init));

static void
gtk_array_store_dispose (GObject *object)
{
  GtkArrayStore *self = GTK_ARRAY_STORE (object);

  gtk_vector_clear (&self->items);

  G_OBJECT_CLASS (gtk_array_store_parent_class)->dispose (object);
}

static void
gtk_array_store_class_init (GtkArrayStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_array_store_dispose;
}

static GType
gtk_array_store_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_array_store_get_n_items (GListModel *list)
{
  GtkArrayStore *self = GTK_ARRAY_STORE (list);

  return gtk_vector_get_size (&self->items);
}

static gpointer
gtk_array_store_get_item (GListModel *list,
                          guint       position)
{
  GtkArrayStore *self = GTK_ARRAY_STORE (list);

  if (position >= gtk_vector_get_size (&self->items))
    return NULL;

  return g_object_ref (gtk_vector_get (&self->items, position));
}

static void
gtk_array_store_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_array_store_get_item_type;
  iface->get_n_items = gtk_array_store_get_n_items;
  iface->get_item = gtk_array_store_get_item;
}

static void
gtk_array_store_init (GtkArrayStore *self)
{
  gtk_vector_init (&self->items);
}

/**
 * gtk_array_store_new:
 * @item_type: the #GType of items in the list
 *
 * Creates a new #GtkArrayStore with items of type @item_type. @item_type
 * must be a subclass of #GObject.
 *
 * Returns: a new #GtkArrayStore
 */
GtkArrayStore *
gtk_array_store_new (void)
{
  return g_object_new (GTK_TYPE_ARRAY_STORE, NULL);
}

/**
 * gtk_array_store_append:
 * @self: a #GtkArrayStore
 * @item: (type GObject): the new item
 *
 * Appends @item to @self. @item must be of type #GtkArrayStore:item-type.
 *
 * This function takes a ref on @item.
 *
 * Use gtk_array_store_splice() to append multiple items at the same time
 * efficiently.
 */
void
gtk_array_store_append (GtkArrayStore *self,
                        gpointer       item)
{
  guint position;

  g_return_if_fail (GTK_IS_ARRAY_STORE (self));

  position = gtk_vector_get_size (&self->items);
  gtk_vector_append (&self->items, g_object_ref (item));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

/**
 * gtk_array_store_remove_all:
 * @self: a #GtkArrayStore
 *
 * Removes all items from @self.
 *
 * Since: 2.44
 */
void
gtk_array_store_remove_all (GtkArrayStore *self)
{
  guint n_items;

  g_return_if_fail (GTK_IS_ARRAY_STORE (self));

  n_items = gtk_vector_get_size (&self->items);
  gtk_vector_clear (&self->items);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
}

/**
 * gtk_array_store_splice:
 * @self: a #GtkArrayStore
 * @position: the position at which to make the change
 * @n_removals: the number of items to remove
 * @additions: (array length=n_additions) (element-type GObject): the items to add
 * @n_additions: the number of items to add
 *
 * Changes @self by removing @n_removals items and adding @n_additions
 * items to it. @additions must contain @n_additions items of type
 * #GtkArrayStore:item-type.  %NULL is not permitted.
 *
 * This function is more efficient than gtk_array_store_insert() and
 * gtk_array_store_remove(), because it only emits
 * #GListModel::items-changed once for the change.
 *
 * This function takes a ref on each item in @additions.
 *
 * The parameters @position and @n_removals must be correct (ie:
 * @position + @n_removals must be less than or equal to the length of
 * the list at the time this function is called).
 *
 * Since: 2.44
 */
void
gtk_array_store_splice (GtkArrayStore *self,
                        guint       position,
                        guint       n_removals,
                        gpointer   *additions,
                        guint       n_additions)
{
  guint i;

  g_return_if_fail (GTK_IS_ARRAY_STORE (self));
  g_return_if_fail (position + n_removals >= position); /* overflow */
  g_return_if_fail (position + n_removals <= gtk_vector_get_size (&self->items));

  for (i = 0; i < n_additions; i++)
    g_object_ref (additions[i]);

  gtk_vector_splice (&self->items, position, n_removals, (GObject **) additions, n_additions);

  g_list_model_items_changed (G_LIST_MODEL (self), position, n_removals, n_additions);
}
