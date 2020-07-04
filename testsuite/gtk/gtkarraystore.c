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

/**
 * SECTION:gtkarraystore
 * @title: GtkArrayStore2
 * @short_description: A simple array implementation of #GListModel
 *
 * #GtkArrayStore2 is a simple implementation of #GListModel that stores all
 * items in memory.
 *
 * It provides appending, deletions, and lookups in O(1) time and insertions
 * in O(N) time. it is implemented using an array.
 */

/**
 * GtkArrayStore2:
 *
 * #GtkArrayStore2 is an opaque data structure and can only be accessed
 * using the following functions.
 **/

struct _GtkArrayStore2
{
  GObject parent_instance;

  GType item_type;
  GPtrArray *items;
};

enum
{
  PROP_0,
  PROP_ITEM_TYPE,
  N_PROPERTIES
};

static void gtk_array_store2_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkArrayStore2, gtk_array_store2, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_array_store2_iface_init));

static void
gtk_array_store2_dispose (GObject *object)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (object);

  g_ptr_array_free (self->items, TRUE);

  G_OBJECT_CLASS (gtk_array_store2_parent_class)->dispose (object);
}

static void
gtk_array_store2_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_array_store2_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE: /* construct-only */
      g_assert (g_type_is_a (g_value_get_gtype (value), G_TYPE_OBJECT));
      self->item_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_array_store2_class_init (GtkArrayStore2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_array_store2_dispose;
  object_class->get_property = gtk_array_store2_get_property;
  object_class->set_property = gtk_array_store2_set_property;

  /**
   * GtkArrayStore2:item-type:
   *
   * The type of items contained in this list self. Items must be
   * subclasses of #GObject.
   **/
  g_object_class_install_property (object_class, PROP_ITEM_TYPE,
    g_param_spec_gtype ("item-type", "", "", G_TYPE_OBJECT,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GType
gtk_array_store2_get_item_type (GListModel *list)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (list);

  return self->item_type;
}

static guint
gtk_array_store2_get_n_items (GListModel *list)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (list);

  return self->items->len;
}

static gpointer
gtk_array_store2_get_item (GListModel *list,
                          guint       position)
{
  GtkArrayStore2 *self = GTK_ARRAY_STORE2 (list);

  if (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static void
gtk_array_store2_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_array_store2_get_item_type;
  iface->get_n_items = gtk_array_store2_get_n_items;
  iface->get_item = gtk_array_store2_get_item;
}

static void
gtk_array_store2_init (GtkArrayStore2 *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * gtk_array_store2_new:
 * @item_type: the #GType of items in the list
 *
 * Creates a new #GtkArrayStore2 with items of type @item_type. @item_type
 * must be a subclass of #GObject.
 *
 * Returns: a new #GtkArrayStore2
 */
GtkArrayStore2 *
gtk_array_store2_new (GType item_type)
{
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (GTK_TYPE_ARRAY_STORE2,
                       "item-type", item_type,
                       NULL);
}

/**
 * gtk_array_store2_append:
 * @self: a #GtkArrayStore2
 * @item: (type GObject): the new item
 *
 * Appends @item to @self. @item must be of type #GtkArrayStore2:item-type.
 *
 * This function takes a ref on @item.
 *
 * Use gtk_array_store2_splice() to append multiple items at the same time
 * efficiently.
 */
void
gtk_array_store2_append (GtkArrayStore2 *self,
                        gpointer       item)
{
  guint position;

  g_return_if_fail (GTK_IS_ARRAY_STORE2 (self));
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (item), self->item_type));

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (item));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

/**
 * gtk_array_store2_remove_all:
 * @self: a #GtkArrayStore2
 *
 * Removes all items from @self.
 *
 * Since: 2.44
 */
void
gtk_array_store2_remove_all (GtkArrayStore2 *self)
{
  guint n_items;

  g_return_if_fail (GTK_IS_ARRAY_STORE2 (self));

  n_items = self->items->len;
  g_ptr_array_remove_range (self->items, 0, n_items);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, 0);
}

/**
 * gtk_array_store2_splice:
 * @self: a #GtkArrayStore2
 * @position: the position at which to make the change
 * @n_removals: the number of items to remove
 * @additions: (array length=n_additions) (element-type GObject): the items to add
 * @n_additions: the number of items to add
 *
 * Changes @self by removing @n_removals items and adding @n_additions
 * items to it. @additions must contain @n_additions items of type
 * #GtkArrayStore2:item-type.  %NULL is not permitted.
 *
 * This function is more efficient than gtk_array_store2_insert() and
 * gtk_array_store2_remove(), because it only emits
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
gtk_array_store2_splice (GtkArrayStore2 *self,
                        guint       position,
                        guint       n_removals,
                        gpointer   *additions,
                        guint       n_additions)
{
  guint i;

  g_return_if_fail (GTK_IS_ARRAY_STORE2 (self));
  g_return_if_fail (position + n_removals >= position); /* overflow */
  g_return_if_fail (position + n_removals <= self->items->len);

  g_ptr_array_remove_range (self->items, position, n_removals);

  for (i = 0; i < n_additions; i++)
    g_ptr_array_add (self->items, g_object_ref (additions[i]));

  g_list_model_items_changed (G_LIST_MODEL (self), position, n_removals, n_additions);
}
