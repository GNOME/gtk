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

#include "gtklistitemprivate.h"
#include "gtkwidgetprivate.h"

#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

struct _GtkListItemManager
{
  GObject parent_instance;

  GtkWidget *widget;
  GtkSelectionModel *model;
  GtkListItemFactory *factory;

  GtkRbTree *items;

  /* managing the visible region */
  GtkWidget *anchor; /* may be NULL if list is empty */
  int anchor_align; /* what to align the anchor to */
  guint anchor_start; /* start of region we allocate row widgets for */
  guint anchor_end; /* end of same region - first position to not have a widget */
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

void
gtk_list_item_manager_augment_node (GtkRbTree *tree,
                                    gpointer   node_augment,
                                    gpointer   node,
                                    gpointer   left,
                                    gpointer   right)
{
  GtkListItemManagerItem *item = node;
  GtkListItemManagerItemAugment *aug = node_augment;

  aug->n_items = item->n_items;

  if (left)
    {
      GtkListItemManagerItemAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->n_items += left_aug->n_items;
    }

  if (right)
    {
      GtkListItemManagerItemAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->n_items += right_aug->n_items;
    }
}

static void
gtk_list_item_manager_clear_node (gpointer _item)
{
  GtkListItemManagerItem *item = _item;

  g_assert (item->widget == NULL);
}

GtkListItemManager *
gtk_list_item_manager_new_for_size (GtkWidget            *widget,
                                    gsize                 element_size,
                                    gsize                 augment_size,
                                    GtkRbTreeAugmentFunc  augment_func)
{
  GtkListItemManager *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (element_size >= sizeof (GtkListItemManagerItem), NULL);
  g_return_val_if_fail (augment_size >= sizeof (GtkListItemManagerItemAugment), NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_MANAGER, NULL);

  /* not taking a ref because the widget refs us */
  self->widget = widget;

  self->items = gtk_rb_tree_new_for_size (element_size,
                                          augment_size,
                                          augment_func,
                                          gtk_list_item_manager_clear_node,
                                          NULL);

  return self;
}

gpointer
gtk_list_item_manager_get_first (GtkListItemManager *self)
{
  return gtk_rb_tree_get_first (self->items);
}

gpointer
gtk_list_item_manager_get_root (GtkListItemManager *self)
{
  return gtk_rb_tree_get_root (self->items);
}

/*
 * gtk_list_item_manager_get_nth:
 * @self: a #GtkListItemManager
 * @position: position of the item
 * @offset: (out): offset into the returned item
 *
 * Looks up the GtkListItemManagerItem that represents @position.
 *
 * If a the returned item represents multiple rows, the @offset into
 * the returned item for @position will be set. If the returned item
 * represents a row with an existing widget, @offset will always be 0.
 *
 * Returns: (type GtkListItemManagerItem): the item for @position or
 *     %NULL if position is out of range
 **/
gpointer
gtk_list_item_manager_get_nth (GtkListItemManager *self,
                               guint               position,
                               guint              *offset)
{
  GtkListItemManagerItem *item, *tmp;

  item = gtk_rb_tree_get_root (self->items);

  while (item)
    {
      tmp = gtk_rb_tree_node_get_left (item);
      if (tmp)
        {
          GtkListItemManagerItemAugment *aug = gtk_rb_tree_get_augment (self->items, tmp);
          if (position < aug->n_items)
            {
              item = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      if (position < item->n_items)
        break;
      position -= item->n_items;

      item = gtk_rb_tree_node_get_right (item);
    }

  if (offset)
    *offset = item ? position : 0;

  return item;
}

guint
gtk_list_item_manager_get_item_position (GtkListItemManager *self,
                                         gpointer            item)
{
  GtkListItemManagerItem *parent, *left;
  int pos;

  left = gtk_rb_tree_node_get_left (item);
  if (left)
    {
      GtkListItemManagerItemAugment *aug = gtk_rb_tree_get_augment (self->items, left);
      pos = aug->n_items;
    }
  else
    {
      pos = 0; 
    }

  for (parent = gtk_rb_tree_node_get_parent (item);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (item))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != item)
        {
          if (left)
            {
              GtkListItemManagerItemAugment *aug = gtk_rb_tree_get_augment (self->items, left);
              pos += aug->n_items;
            }
          pos += parent->n_items;
        }

      item = parent;
    }

  return pos;
}

gpointer
gtk_list_item_manager_get_item_augment (GtkListItemManager *self,
                                        gpointer            item)
{
  return gtk_rb_tree_get_augment (self->items, item);
}

static void
gtk_list_item_manager_remove_items (GtkListItemManager       *self,
                                    GtkListItemManagerChange *change,
                                    guint                     position,
                                    guint                     n_items)
{
  GtkListItemManagerItem *item;

  if (n_items == 0)
    return;

  item = gtk_list_item_manager_get_nth (self, position, NULL);

  while (n_items > 0)
    {
      if (item->n_items > n_items)
        {
          item->n_items -= n_items;
          gtk_rb_tree_node_mark_dirty (item);
          n_items = 0;
        }
      else
        {
          GtkListItemManagerItem *next = gtk_rb_tree_node_get_next (item);
          if (item->widget)
            gtk_list_item_manager_release_list_item (self, change, item->widget);
          item->widget = NULL;
          n_items -= item->n_items;
          gtk_rb_tree_remove (self->items, item);
          item = next;
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_add_items (GtkListItemManager *self,
                                 guint               position,
                                 guint               n_items)
{  
  GtkListItemManagerItem *item;
  guint offset;

  if (n_items == 0)
    return;

  item = gtk_list_item_manager_get_nth (self, position, &offset);

  if (item == NULL || item->widget)
    item = gtk_rb_tree_insert_before (self->items, item);
  item->n_items += n_items;
  gtk_rb_tree_node_mark_dirty (item);

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_unset_anchor (GtkListItemManager *self)
{
  self->anchor = NULL;
  self->anchor_align = 0;
  self->anchor_start = 0;
  self->anchor_end = 0;
}

static gboolean
gtk_list_item_manager_merge_list_items (GtkListItemManager     *self,
                                        GtkListItemManagerItem *first,
                                        GtkListItemManagerItem *second)
{
  if (first->widget || second->widget)
    return FALSE;

  first->n_items += second->n_items;
  gtk_rb_tree_node_mark_dirty (first);
  gtk_rb_tree_remove (self->items, second);

  return TRUE;
}

static void
gtk_list_item_manager_release_items (GtkListItemManager *self,
                                     GQueue             *released)
{
  GtkListItemManagerItem *item, *prev, *next;
  guint i;

  item = gtk_rb_tree_get_first (self->items);
  i = 0;
  while (i < self->anchor_start)
    {
      if (item->widget)
        {
          g_queue_push_tail (released, item->widget);
          item->widget = NULL;
          i++;
          prev = gtk_rb_tree_node_get_previous (item);
          if (prev && gtk_list_item_manager_merge_list_items (self, prev, item))
            item = prev;
          next = gtk_rb_tree_node_get_next (item);
          if (next && next->widget == NULL)
            {
              i += next->n_items;
              if (!gtk_list_item_manager_merge_list_items (self, next, item))
                g_assert_not_reached ();
              item = gtk_rb_tree_node_get_next (next);
            }
          else 
            {
              item = next;
            }
        }
      else
        {
          i += item->n_items;
          item = gtk_rb_tree_node_get_next (item);
        }
    }

  item = gtk_list_item_manager_get_nth (self, self->anchor_end, NULL);
  if (item == NULL)
    return;
  
  if (item->widget)
    {
      g_queue_push_tail (released, item->widget);
      item->widget = NULL;
      prev = gtk_rb_tree_node_get_previous (item);
      if (prev && gtk_list_item_manager_merge_list_items (self, prev, item))
        item = prev;
    }

  while ((next = gtk_rb_tree_node_get_next (item)))
    {
      if (next->widget)
        {
          g_queue_push_tail (released, next->widget);
          next->widget = NULL;
        }
      gtk_list_item_manager_merge_list_items (self, item, next);
    }
}

static void
gtk_list_item_manager_ensure_items (GtkListItemManager       *self,
                                    GtkListItemManagerChange *change,
                                    guint                     update_start)
{
  GtkListItemManagerItem *item, *new_item;
  guint i, offset;
  GtkWidget *widget, *insert_after;
  GQueue released = G_QUEUE_INIT;

  gtk_list_item_manager_release_items (self, &released);

  item = gtk_list_item_manager_get_nth (self, self->anchor_start, &offset);
  if (offset > 0)
    {
      new_item = gtk_rb_tree_insert_before (self->items, item);
      new_item->n_items = offset;
      item->n_items -= offset;
      gtk_rb_tree_node_mark_dirty (item);
    }

  insert_after = NULL;

  for (i = self->anchor_start; i < self->anchor_end; i++)
    {
      if (item->n_items > 1)
        {
          new_item = gtk_rb_tree_insert_before (self->items, item);
          new_item->n_items = 1;
          item->n_items--;
          gtk_rb_tree_node_mark_dirty (item);
        }
      else
        {
          new_item = item;
          item = gtk_rb_tree_node_get_next (item);
        }
      if (new_item->widget == NULL)
        {
          if (change)
            {
              new_item->widget = gtk_list_item_manager_try_reacquire_list_item (self,
                                                                                change,
                                                                                i,
                                                                                insert_after);
            }
          if (new_item->widget == NULL)
            {
              new_item->widget = g_queue_pop_head (&released);
              if (new_item->widget)
                {
                  gtk_list_item_manager_move_list_item (self,
                                                        new_item->widget,
                                                        i,
                                                        insert_after);
                }
              else
                {
                  new_item->widget = gtk_list_item_manager_acquire_list_item (self,
                                                                              i,
                                                                              insert_after);
                }
            }
        }
      else
        {
          if (update_start <= i)
            gtk_list_item_manager_update_list_item (self, new_item->widget, i);
        }
      insert_after = new_item->widget;
    }

  while ((widget = g_queue_pop_head (&released)))
    gtk_list_item_manager_release_list_item (self, NULL, widget);
}

void
gtk_list_item_manager_set_anchor (GtkListItemManager       *self,
                                  guint                     position,
                                  double                    align,
                                  GtkListItemManagerChange *change,
                                  guint                     update_start)
{
  GtkListItemManagerItem *item;
  guint items_before, items_after, total_items, n_items;

  g_assert (align >= 0.0 && align <= 1.0);

  if (self->model)
    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  else
    n_items = 0;
  if (n_items == 0)
    {
      gtk_list_item_manager_unset_anchor (self);
      return;
    }
  total_items = MIN (GTK_LIST_VIEW_MAX_LIST_ITEMS, n_items);
  if (align < 0.5)
    items_before = ceil (total_items * align);
  else
    items_before = floor (total_items * align);
  items_after = total_items - items_before;
  self->anchor_start = CLAMP (position, items_before, n_items - items_after) - items_before;
  self->anchor_end = self->anchor_start + total_items;
  g_assert (self->anchor_end <= n_items);

  gtk_list_item_manager_ensure_items (self, change, update_start);

  item = gtk_list_item_manager_get_nth (self, position, NULL);
  self->anchor = item->widget;
  g_assert (self->anchor);
  self->anchor_align = align;

  gtk_widget_queue_allocate (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_model_items_changed_cb (GListModel         *model,
                                              guint               position,
                                              guint               removed,
                                              guint               added,
                                              GtkListItemManager *self)
{
  GtkListItemManagerChange *change;

  change = gtk_list_item_manager_begin_change (self);

  gtk_list_item_manager_remove_items (self, change, position, removed);
  gtk_list_item_manager_add_items (self, position, added);

  /* The anchor was removed, but it may just have moved to a different position */
  if (self->anchor && gtk_list_item_manager_change_contains (change, self->anchor))
    {
      /* The anchor was removed, do a more expensive rebuild trying to find if
       * the anchor maybe got readded somewhere else */
      GtkListItemManagerItem *item, *new_item;
      GtkWidget *insert_after;
      guint i, offset, anchor_pos;
      
      item = gtk_list_item_manager_get_nth (self, position, &offset);
      for (new_item = item ? gtk_rb_tree_node_get_previous (item) : gtk_rb_tree_get_last (self->items);
           new_item && new_item->widget == NULL;
           new_item = gtk_rb_tree_node_get_previous (new_item))
        { }
      if (new_item)
        insert_after = new_item->widget;
      else
        insert_after = NULL; /* we're at the start */

      for (i = 0; i < added; i++)
        {
          GtkWidget *widget;

          widget = gtk_list_item_manager_try_reacquire_list_item (self,
                                                                  change,
                                                                  position + i,
                                                                  insert_after);
          if (widget == NULL)
            {
              offset++;
              continue;
            }

          if (offset > 0)
            {
              new_item = gtk_rb_tree_insert_before (self->items, item);
              new_item->n_items = offset;
              item->n_items -= offset;
              offset = 0;
              gtk_rb_tree_node_mark_dirty (item);
            }

          if (item->n_items == 1)
            {
              new_item = item;
              item = gtk_rb_tree_node_get_next (item);
            }
          else
            {
              new_item = gtk_rb_tree_insert_before (self->items, item);
              new_item->n_items = 1;
              item->n_items--;
              gtk_rb_tree_node_mark_dirty (item);
            }

          new_item->widget = widget;
          insert_after = widget;

          if (widget == self->anchor)
            {
              anchor_pos = position + i;
              break;
            }
        }

      if (i == added)
        {
          /* The anchor wasn't readded. Guess a good anchor position */
          anchor_pos = gtk_list_item_get_position (GTK_LIST_ITEM (self->anchor));

          anchor_pos = position + (anchor_pos - position) * added / removed;
          if (anchor_pos >= g_list_model_get_n_items (G_LIST_MODEL (self->model)) &&
              anchor_pos > 0)
            anchor_pos--;
        }
      gtk_list_item_manager_set_anchor (self, anchor_pos, self->anchor_align, change, position);
    }
  else
    {
      /* The anchor is still where it was.
       * We just may need to update its position and check that its surrounding widgets
       * exist (they might be new ones). */
      guint anchor_pos;
      
      if (self->anchor)
        {
          anchor_pos = gtk_list_item_get_position (GTK_LIST_ITEM (self->anchor));

          if (anchor_pos >= position)
            anchor_pos += added - removed;
        }
      else
        {
          anchor_pos = 0;
        }

      gtk_list_item_manager_set_anchor (self, anchor_pos, self->anchor_align, change, position);
    }

  gtk_list_item_manager_end_change (self, change);
}

static void
gtk_list_item_manager_model_selection_changed_cb (GListModel         *model,
                                                  guint               position,
                                                  guint               n_items,
                                                  GtkListItemManager *self)
{
  GtkListItemManagerItem *item;
  guint offset;

  item = gtk_list_item_manager_get_nth (self, position, &offset);

  if (offset)
    {
      position += item->n_items - offset;
      n_items -= item->n_items - offset;
      item = gtk_rb_tree_node_get_next (item);
    }

  while (n_items > 0)
    {
      if (item->widget)
        gtk_list_item_manager_update_list_item (self, item->widget, position);
      position += item->n_items;
      n_items -= MIN (n_items, item->n_items);
      item = gtk_rb_tree_node_get_next (item);
    }
}

guint
gtk_list_item_manager_get_anchor (GtkListItemManager *self,
                                  double             *align)
{
  guint anchor_pos;

  if (self->anchor)
    anchor_pos = gtk_list_item_get_position (GTK_LIST_ITEM (self->anchor));
  else
    anchor_pos = 0;

  if (align)
    *align = self->anchor_align;

  return anchor_pos;
}

static void
gtk_list_item_manager_clear_model (GtkListItemManager *self)
{
  if (self->model == NULL)
    return;

  gtk_list_item_manager_remove_items (self, NULL, 0, g_list_model_get_n_items (G_LIST_MODEL (self->model)));

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_selection_changed_cb,
                                        self);
  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_items_changed_cb,
                                        self);
  g_clear_object (&self->model);

  gtk_list_item_manager_unset_anchor (self);
}

static void
gtk_list_item_manager_dispose (GObject *object)
{
  GtkListItemManager *self = GTK_LIST_ITEM_MANAGER (object);

  gtk_list_item_manager_clear_model (self);

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

void
gtk_list_item_manager_set_factory (GtkListItemManager *self,
                                   GtkListItemFactory *factory)
{
  guint n_items, anchor;
  double anchor_align;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory));

  if (self->factory == factory)
    return;

  n_items = self->model ? g_list_model_get_n_items (G_LIST_MODEL (self->model)) : 0;
  anchor = gtk_list_item_manager_get_anchor (self, &anchor_align);
  gtk_list_item_manager_remove_items (self, NULL, 0, n_items);

  g_set_object (&self->factory, factory);

  gtk_list_item_manager_add_items (self, 0, n_items);
  gtk_list_item_manager_set_anchor (self, anchor, anchor_align, NULL, (guint) -1);
}

GtkListItemFactory *
gtk_list_item_manager_get_factory (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->factory;
}

void
gtk_list_item_manager_set_model (GtkListItemManager *self,
                                 GtkSelectionModel  *model)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));

  if (self->model == model)
    return;

  gtk_list_item_manager_clear_model (self);

  if (model)
    {
      self->model = g_object_ref (model);

      g_signal_connect (model,
                        "items-changed",
                        G_CALLBACK (gtk_list_item_manager_model_items_changed_cb),
                        self);
      g_signal_connect (model,
                        "selection-changed",
                        G_CALLBACK (gtk_list_item_manager_model_selection_changed_cb),
                        self);

      gtk_list_item_manager_add_items (self, 0, g_list_model_get_n_items (G_LIST_MODEL (model)));

      gtk_list_item_manager_set_anchor (self, 0, 0, NULL, (guint) -1);
    }
}

GtkSelectionModel *
gtk_list_item_manager_get_model (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->model;
}

void
gtk_list_item_manager_select (GtkListItemManager *self,
                              GtkListItem        *item,
                              gboolean            modify,
                              gboolean            extend)
{
  guint pos = gtk_list_item_get_position (item);

  if (modify)
    {
      if (gtk_list_item_get_selected (item))
        gtk_selection_model_unselect_item (self->model, pos);
      else
        gtk_selection_model_select_item (self->model, pos, FALSE);
    }
  else
    {
      gtk_selection_model_select_item (self->model, pos, TRUE);
    }
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
 * @prev_sibling: the widget this widget should be inserted before or %NULL
 *     if it should be the first widget
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
                                         GtkWidget          *prev_sibling)
{
  GtkListItem *result;
  gpointer item;
  gboolean selected;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (prev_sibling == NULL || GTK_IS_WIDGET (prev_sibling), NULL);

  result = gtk_list_item_new (self, "row");
  gtk_list_item_factory_setup (self->factory, result);

  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_factory_bind (self->factory, result, position, item, selected);
  g_object_unref (item);
  gtk_widget_insert_after (GTK_WIDGET (result), self->widget, prev_sibling);

  return GTK_WIDGET (result);
}

/**
 * gtk_list_item_manager_try_acquire_list_item_from_change:
 * @self: a #GtkListItemManager
 * @position: the row in the model to create a list item for
 * @prev_sibling: the widget this widget should be inserted after or %NULL
 *     if it should be the first widget
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
                                               GtkWidget                *prev_sibling)
{
  GtkListItem *result;
  gpointer item;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (prev_sibling == NULL || GTK_IS_WIDGET (prev_sibling), NULL);

  /* XXX: can we avoid temporarily allocating items on failure? */
  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  if (g_hash_table_steal_extended (change->items, item, NULL, (gpointer *) &result))
    {
      gtk_list_item_factory_update (self->factory, result, position, FALSE);
      gtk_widget_insert_after (GTK_WIDGET (result), self->widget, prev_sibling);
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
 * gtk_list_item_manager_move_list_item:
 * @self: a #GtkListItemManager
 * @list_item: an acquired #GtkListItem that should be moved to represent
 *     a different row
 * @position: the new position of that list item
 * @prev_sibling: the new previous sibling
 *
 * Moves the widget to represent a new position in the listmodel without
 * releasing the item.
 *
 * This is most useful when scrolling.
 **/
void
gtk_list_item_manager_move_list_item (GtkListItemManager     *self,
                                      GtkWidget              *list_item,
                                      guint                   position,
                                      GtkWidget              *prev_sibling)
{
  gpointer item;
  gboolean selected;

  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_factory_bind (self->factory, GTK_LIST_ITEM (list_item), position, item, selected);
  gtk_widget_insert_after (list_item, _gtk_widget_get_parent (list_item), prev_sibling);
  g_object_unref (item);
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
  gboolean selected;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM (item));

  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_factory_update (self->factory, GTK_LIST_ITEM (item), position, selected);
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

