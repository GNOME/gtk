/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtklistitemmanagerprivate.h"

struct _GtkListItemManager
{
  GObject parent_instance;

  GtkWidget *widget;
  GListModel *model;
  GtkListItemFactory *factory;
};

struct _GtkListItemManagerClass
{
  GObjectClass parent_class;
};

struct _GtkListItemManagerChange
{
  GHashTable *items;
};

G_DEFINE_TYPE (GtkListItemManager, gtk_list_item_manager, G_TYPE_OBJECT)

static void
gtk_list_item_manager_dispose (GObject *object)
{
  GtkListItemManager *self = GTK_LIST_ITEM_MANAGER (object);

  g_clear_object (&self->model);
  g_clear_object (&self->factory);

  G_OBJECT_CLASS (gtk_list_item_manager_parent_class)->dispose (object);
}

static void
gtk_list_item_manager_class_init (GtkListItemManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_list_item_manager_dispose;
}

static void
gtk_list_item_manager_init (GtkListItemManager *self)
{
}

GtkListItemManager *
gtk_list_item_manager_new (GtkWidget *widget)
{
  GtkListItemManager *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_MANAGER, NULL);

  self->widget = widget;

  return self;
}

void
gtk_list_item_manager_set_factory (GtkListItemManager *self,
                                   GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory));

  if (self->factory == factory)
    return;

  g_clear_object (&self->factory);

  self->factory = g_object_ref (factory);
}

GtkListItemFactory *
gtk_list_item_manager_get_factory (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->factory;
}

void
gtk_list_item_manager_set_model (GtkListItemManager *self,
                                 GListModel         *model)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  g_clear_object (&self->model);

  if (model)
    self->model = g_object_ref (model);
}

GListModel *
gtk_list_item_manager_get_model (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->model;
}

#if 0 
/*
 * gtk_list_item_manager_get_size:
 * @self: a #GtkListItemManager
 *
 * Queries the number of widgets currently handled by @self.
 *
 * This includes both widgets that have been acquired and
 * those currently waiting to be used again.
 *
 * Returns: Number of widgets handled by @self
 **/
guint
gtk_list_item_manager_get_size (GtkListItemManager *self)
{
  return g_hash_table_size (self->pool);
}
#endif

/*
 * gtk_list_item_manager_begin_change:
 * @self: a #GtkListItemManager
 *
 * Begins a change operation in response to a model's items-changed
 * signal.
 * During an ongoing change operation, list items will not be discarded
 * when released but will be kept around in anticipation of them being
 * added back in a different posiion later.
 *
 * Once it is known that no more list items will be reused,
 * gtk_list_item_manager_end_change() should be called. This should happen
 * as early as possible, so the list items held for the change can be
 * reqcquired.
 *
 * Returns: The object to use for this change
 **/
GtkListItemManagerChange *
gtk_list_item_manager_begin_change (GtkListItemManager *self)
{
  GtkListItemManagerChange *change;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  change = g_slice_new (GtkListItemManagerChange);
  change->items = g_hash_table_new (g_direct_hash, g_direct_equal);

  return change;
}

/*
 * gtk_list_item_manager_end_change:
 * @self: a #GtkListItemManager
 * @change: a change
 *
 * Ends a change operation begun with gtk_list_item_manager_begin_change()
 * and releases all list items still cached.
 **/
void
gtk_list_item_manager_end_change (GtkListItemManager       *self,
                                  GtkListItemManagerChange *change)
{
  GHashTableIter iter;
  gpointer list_item;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));

  g_hash_table_iter_init (&iter, change->items);
  while (g_hash_table_iter_next (&iter, NULL, &list_item))
    {
      gtk_list_item_manager_release_list_item (self, NULL, list_item);
    }

  g_hash_table_unref (change->items);
  g_slice_free (GtkListItemManagerChange, change);
}

/*
 * gtk_list_item_manager_change_contains:
 * @change: a #GtkListItemManagerChange
 * @list_item: The item that may have been released into this change set
 *
 * Checks if @list_item has been released as part of @change but not been
 * reacquired yet.
 *
 * This is useful to test before calling gtk_list_item_manager_end_change()
 * if special actions need to be performed when important list items - like
 * the focused item - are about to be deleted.
 *
 * Returns: %TRUE if the item is part of this change
 **/
gboolean
gtk_list_item_manager_change_contains (GtkListItemManagerChange *change,
                                       GtkWidget                *list_item)
{
  g_return_val_if_fail (change != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_ITEM (list_item), FALSE);

  return g_hash_table_lookup (change->items, gtk_list_item_get_item (GTK_LIST_ITEM (list_item))) == list_item;
}

/*
 * gtk_list_item_manager_acquire_list_item:
 * @self: a #GtkListItemManager
 * @position: the row in the model to create a list item for
 * @next_sibling: the widget this widget should be inserted before or %NULL
 *     if none
 *
 * Creates a list item widget to use for @position. No widget may
 * yet exist that is used for @position.
 *
 * When the returned item is no longer needed, the caller is responsible
 * for calling gtk_list_item_manager_release_list_item().  
 * A particular case is when the row at @position is removed. In that case,
 * all list items in the removed range must be released before
 * gtk_list_item_manager_model_changed() is called.
 *
 * Returns: a properly setup widget to use in @position
 **/
GtkWidget *
gtk_list_item_manager_acquire_list_item (GtkListItemManager *self,
                                         guint               position,
                                         GtkWidget          *next_sibling)
{
  GtkListItem *result;
  gpointer item;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (next_sibling == NULL || GTK_IS_WIDGET (next_sibling), NULL);

  result = gtk_list_item_factory_create (self->factory);

  item = g_list_model_get_item (self->model, position);
  gtk_list_item_factory_bind (self->factory, result, position, item);
  g_object_unref (item);
  gtk_widget_insert_before (GTK_WIDGET (result), self->widget, next_sibling);

  return GTK_WIDGET (result);
}

/**
 * gtk_list_item_manager_try_acquire_list_item_from_change:
 * @self: a #GtkListItemManager
 * @position: the row in the model to create a list item for
 * @next_sibling: the widget this widget should be inserted before or %NULL
 *     if none
 *
 * Like gtk_list_item_manager_acquire_list_item(), but only tries to acquire list
 * items from those previously released as part of @change.
 * If no matching list item is found, %NULL is returned and the caller should use
 * gtk_list_item_manager_acquire_list_item().
 *
 * Returns: (nullable): a properly setup widget to use in @position or %NULL if
 *     no item for reuse existed
 **/
GtkWidget *
gtk_list_item_manager_try_reacquire_list_item (GtkListItemManager       *self,
                                               GtkListItemManagerChange *change,
                                               guint                     position,
                                               GtkWidget                *next_sibling)
{
  GtkListItem *result;
  gpointer item;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (next_sibling == NULL || GTK_IS_WIDGET (next_sibling), NULL);

  /* XXX: can we avoid temporarily allocating items on failure? */
  item = g_list_model_get_item (self->model, position);
  if (g_hash_table_steal_extended (change->items, item, NULL, (gpointer *) &result))
    {
      gtk_list_item_factory_update (self->factory, result, position);
      gtk_widget_insert_before (GTK_WIDGET (result), self->widget, next_sibling);
      /* XXX: Should we let the listview do this? */
      gtk_widget_queue_resize (GTK_WIDGET (result));
    }
  else
    {
      result = NULL;
    }
  g_object_unref (item);

  return GTK_WIDGET (result);
}

/**
 * gtk_list_item_manager_update_list_item:
 * @self: a #GtkListItemManager
 * @item: a #GtkListItem that has been acquired
 * @position: the new position of that list item
 *
 * Updates the position of the given @item. This function must be called whenever
 * the position of an item changes, like when new items are added before it.
 **/
void
gtk_list_item_manager_update_list_item (GtkListItemManager *self,
                                        GtkWidget          *item,
                                        guint               position)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  gtk_list_item_factory_update (self->factory, GTK_LIST_ITEM (item), position);
}

/*
 * gtk_list_item_manager_release_list_item:
 * @self: a #GtkListItemManager
 * @change: (allow-none): The change associated with this release or
 *     %NULL if this is a final removal
 * @item: an item previously acquired with
 *     gtk_list_item_manager_acquire_list_item()
 *
 * Releases an item that was previously acquired via
 * gtk_list_item_manager_acquire_list_item() and is no longer in use.
 **/
void
gtk_list_item_manager_release_list_item (GtkListItemManager       *self,
                                         GtkListItemManagerChange *change,
                                         GtkWidget                *item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  if (change != NULL)
    {
      if (g_hash_table_insert (change->items, gtk_list_item_get_item (GTK_LIST_ITEM (item)), item))
        return;
      
      g_warning ("FIXME: Handle the same item multiple times in the list.\nLars says this totally should not happen, but here we are.");
    }

  gtk_list_item_factory_unbind (self->factory, GTK_LIST_ITEM (item));
  gtk_widget_unparent (item);
}
