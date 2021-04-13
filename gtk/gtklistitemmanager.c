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

#include "gtklistitemwidgetprivate.h"
#include "gtkwidgetprivate.h"

#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

struct _GtkListItemManager
{
  GObject parent_instance;

  GtkWidget *widget;
  GtkSelectionModel *model;
  GtkListItemFactory *factory;
  gboolean single_click_activate;
  const char *item_css_name;
  GtkAccessibleRole item_role;

  GtkRbTree *items;
  GSList *trackers;
};

struct _GtkListItemManagerClass
{
  GObjectClass parent_class;
};

struct _GtkListItemTracker
{
  guint position;
  GtkListItemWidget *widget;
  guint n_before;
  guint n_after;
};

static GtkWidget *      gtk_list_item_manager_acquire_list_item (GtkListItemManager     *self,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
static GtkWidget *      gtk_list_item_manager_try_reacquire_list_item
                                                                (GtkListItemManager     *self,
                                                                 GHashTable             *change,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
static void             gtk_list_item_manager_update_list_item  (GtkListItemManager     *self,
                                                                 GtkWidget              *item,
                                                                 guint                   position);
static void             gtk_list_item_manager_move_list_item    (GtkListItemManager     *self,
                                                                 GtkWidget              *list_item,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
static void             gtk_list_item_manager_release_list_item (GtkListItemManager     *self,
                                                                 GHashTable             *change,
                                                                 GtkWidget              *widget);
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
  GtkListItemManagerItem *item G_GNUC_UNUSED = _item;

  g_assert (item->widget == NULL);
}

GtkListItemManager *
gtk_list_item_manager_new_for_size (GtkWidget            *widget,
                                    const char           *item_css_name,
                                    GtkAccessibleRole     item_role,
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
  self->item_css_name = g_intern_string (item_css_name);
  self->item_role = item_role;

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
gtk_list_item_tracker_unset_position (GtkListItemManager *self,
                                      GtkListItemTracker *tracker)
{
  tracker->widget = NULL;
  tracker->position = GTK_INVALID_LIST_POSITION;
}

static gboolean
gtk_list_item_tracker_query_range (GtkListItemManager *self,
                                   GtkListItemTracker *tracker,
                                   guint               n_items,
                                   guint              *out_start,
                                   guint              *out_n_items)
{
  /* We can't look at tracker->widget here because we might not
   * have set it yet.
   */
  if (tracker->position == GTK_INVALID_LIST_POSITION)
    return FALSE;

  /* This is magic I made up that is meant to be both
   * correct and doesn't overflow when start and/or end are close to 0 or
   * close to max.
   * But beware, I didn't test it.
   */
  *out_n_items = tracker->n_before + tracker->n_after + 1;
  *out_n_items = MIN (*out_n_items, n_items);

  *out_start = MAX (tracker->position, tracker->n_before) - tracker->n_before;
  *out_start = MIN (*out_start, n_items - *out_n_items);

  return TRUE;
}

static void
gtk_list_item_query_tracked_range (GtkListItemManager *self,
                                   guint               n_items,
                                   guint               position,
                                   guint              *out_n_items,
                                   gboolean           *out_tracked)
{
  GSList *l;
  guint tracker_start, tracker_n_items;

  g_assert (position < n_items);

  *out_tracked = FALSE;
  *out_n_items = n_items - position;

  /* step 1: Check if position is tracked */

  for (l = self->trackers; l; l = l->next)
    {
      if (!gtk_list_item_tracker_query_range (self, l->data, n_items, &tracker_start, &tracker_n_items))
        continue;

      if (tracker_start > position)
        {
          *out_n_items = MIN (*out_n_items, tracker_start - position);
        }
      else if (tracker_start + tracker_n_items <= position)
        {
          /* do nothing */
        }
      else
        {
          *out_tracked = TRUE;
          *out_n_items = tracker_start + tracker_n_items - position;
          break;
        }
    }

  /* If nothing's tracked, we're done */
  if (!*out_tracked)
    return;

  /* step 2: make the tracked range as large as possible
   * NB: This is O(N_TRACKERS^2), but the number of trackers should be <5 */
restart:
  for (l = self->trackers; l; l = l->next)
    {
      if (!gtk_list_item_tracker_query_range (self, l->data, n_items, &tracker_start, &tracker_n_items))
        continue;

      if (tracker_start + tracker_n_items <= position + *out_n_items)
        continue;
      if (tracker_start > position + *out_n_items)
        continue;

      if (*out_n_items + position < tracker_start + tracker_n_items)
        {
          *out_n_items = tracker_start + tracker_n_items - position;
          goto restart;
        }
    }
}

static void
gtk_list_item_manager_remove_items (GtkListItemManager *self,
                                    GHashTable         *change,
                                    guint               position,
                                    guint               n_items)
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
  guint position, i, n_items, query_n_items;
  gboolean tracked;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  position = 0;

  while (position < n_items)
    {
      gtk_list_item_query_tracked_range (self, n_items, position, &query_n_items, &tracked);
      if (tracked)
        {
          position += query_n_items;
          continue;
        }

      item = gtk_list_item_manager_get_nth (self, position, &i);
      i = position - i;
      while (i < position + query_n_items)
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
      position += query_n_items;
    }
}

static void
gtk_list_item_manager_ensure_items (GtkListItemManager *self,
                                    GHashTable         *change,
                                    guint               update_start)
{
  GtkListItemManagerItem *item, *new_item;
  GtkWidget *widget, *insert_after;
  guint position, i, n_items, query_n_items, offset;
  GQueue released = G_QUEUE_INIT;
  gboolean tracked;

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  position = 0;

  gtk_list_item_manager_release_items (self, &released);

  while (position < n_items)
    {
      gtk_list_item_query_tracked_range (self, n_items, position, &query_n_items, &tracked);
      if (!tracked)
        {
          position += query_n_items;
          continue;
        }

      item = gtk_list_item_manager_get_nth (self, position, &offset);
      for (new_item = item;
           new_item && new_item->widget == NULL;
           new_item = gtk_rb_tree_node_get_previous (new_item))
         { /* do nothing */ }
      insert_after = new_item ? new_item->widget : NULL;

      if (offset > 0)
        {
          new_item = gtk_rb_tree_insert_before (self->items, item);
          new_item->n_items = offset;
          item->n_items -= offset;
          gtk_rb_tree_node_mark_dirty (item);
        }

      for (i = 0; i < query_n_items; i++)
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
                                                                                    position + i,
                                                                                    insert_after);
                }
              if (new_item->widget == NULL)
                {
                  new_item->widget = g_queue_pop_head (&released);
                  if (new_item->widget)
                    {
                      gtk_list_item_manager_move_list_item (self,
                                                            new_item->widget,
                                                            position + i,
                                                            insert_after);
                    }
                  else
                    {
                      new_item->widget = gtk_list_item_manager_acquire_list_item (self,
                                                                                  position + i,
                                                                                  insert_after);
                    }
                }
            }
          else
            {
              if (update_start <= position + i)
                gtk_list_item_manager_update_list_item (self, new_item->widget, position + i);
            }
          insert_after = new_item->widget;
        }
      position += query_n_items;
    }

  while ((widget = g_queue_pop_head (&released)))
    gtk_list_item_manager_release_list_item (self, NULL, widget);
}

static void
gtk_list_item_manager_model_items_changed_cb (GListModel         *model,
                                              guint               position,
                                              guint               removed,
                                              guint               added,
                                              GtkListItemManager *self)
{
  GHashTable *change;
  GSList *l;
  guint n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  change = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify )gtk_widget_unparent);

  gtk_list_item_manager_remove_items (self, change, position, removed);
  gtk_list_item_manager_add_items (self, position, added);

  /* Check if any tracked item was removed */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;

      if (tracker->widget == NULL)
        continue;

      if (g_hash_table_lookup (change, gtk_list_item_widget_get_item (tracker->widget)))
        break;
    }

  /* At least one tracked item was removed, do a more expensive rebuild
   * trying to find where it moved */
  if (l)
    {
      GtkListItemManagerItem *item, *new_item;
      GtkWidget *insert_after;
      guint i, offset;
      
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
        }
    }

  /* Update tracker positions if necessary, they need to have correct
   * positions for gtk_list_item_manager_ensure_items().
   * We don't update the items, they will be updated by ensure_items()
   * and then we can update them. */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;

      if (tracker->position == GTK_INVALID_LIST_POSITION)
        {
          /* if the list is no longer empty, set the tracker to a valid position. */
          if (n_items > 0 && n_items == added && removed == 0)
            tracker->position = 0;
        }
      else if (tracker->position >= position + removed)
        {
          tracker->position += added - removed;
        }
      else if (tracker->position >= position)
        {
          if (g_hash_table_lookup (change, gtk_list_item_widget_get_item (tracker->widget)))
            {
              /* The item is gone. Guess a good new position */
              tracker->position = position + (tracker->position - position) * added / removed;
              if (tracker->position >= n_items)
                {
                  if (n_items == 0)
                    tracker->position = GTK_INVALID_LIST_POSITION;
                  else
                    tracker->position--;
                }
              tracker->widget = NULL;
            }
          else
            {
              /* item was put in its right place in the expensive loop above,
               * and we updated its position while at it. So grab it from there.
               */
              tracker->position = gtk_list_item_widget_get_position (tracker->widget);
            }
        }
      else
        {
          /* nothing changed for items before position */
        }
    }

  gtk_list_item_manager_ensure_items (self, change, position + added);

  /* final loop through the trackers: Grab the missing widgets.
   * For items that had been removed and a new position was set, grab
   * their item now that we ensured it exists.
   */
  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;
      GtkListItemManagerItem *item;

      if (tracker->widget != NULL || 
          tracker->position == GTK_INVALID_LIST_POSITION)
        continue;

      item = gtk_list_item_manager_get_nth (self, tracker->position, NULL);
      g_assert (item != NULL);
      g_assert (item->widget);
      tracker->widget = GTK_LIST_ITEM_WIDGET (item->widget);
    }

  g_hash_table_unref (change);

  gtk_widget_queue_resize (self->widget);
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
      if (item->n_items - offset > n_items)
        n_items = 0;
      else
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

static void
gtk_list_item_manager_clear_model (GtkListItemManager *self)
{
  GSList *l;

  if (self->model == NULL)
    return;

  gtk_list_item_manager_remove_items (self, NULL, 0, g_list_model_get_n_items (G_LIST_MODEL (self->model)));
  for (l = self->trackers; l; l = l->next)
    {
      gtk_list_item_tracker_unset_position (self, l->data);
    }

  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_selection_changed_cb,
                                        self);
  g_signal_handlers_disconnect_by_func (self->model,
                                        gtk_list_item_manager_model_items_changed_cb,
                                        self);
  g_clear_object (&self->model);
}

static void
gtk_list_item_manager_dispose (GObject *object)
{
  GtkListItemManager *self = GTK_LIST_ITEM_MANAGER (object);

  gtk_list_item_manager_clear_model (self);

  g_clear_object (&self->factory);

  g_clear_pointer (&self->items, gtk_rb_tree_unref);

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
  guint n_items;
  GSList *l;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_FACTORY (factory));

  if (self->factory == factory)
    return;

  n_items = self->model ? g_list_model_get_n_items (G_LIST_MODEL (self->model)) : 0;
  gtk_list_item_manager_remove_items (self, NULL, 0, n_items);

  g_set_object (&self->factory, factory);

  gtk_list_item_manager_add_items (self, 0, n_items);

  gtk_list_item_manager_ensure_items (self, NULL, G_MAXUINT);

  for (l = self->trackers; l; l = l->next)
    {
      GtkListItemTracker *tracker = l->data;
      GtkListItemManagerItem *item;

      if (tracker->widget == NULL)
        continue;

      item = gtk_list_item_manager_get_nth (self, tracker->position, NULL);
      g_assert (item);
      tracker->widget = GTK_LIST_ITEM_WIDGET (item->widget);
    }
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
    }
}

GtkSelectionModel *
gtk_list_item_manager_get_model (GtkListItemManager *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  return self->model;
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
static GtkWidget *
gtk_list_item_manager_acquire_list_item (GtkListItemManager *self,
                                         guint               position,
                                         GtkWidget          *prev_sibling)
{
  GtkWidget *result;
  gpointer item;
  gboolean selected;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (prev_sibling == NULL || GTK_IS_WIDGET (prev_sibling), NULL);

  result = gtk_list_item_widget_new (self->factory,
                                     self->item_css_name,
                                     self->item_role);

  gtk_list_item_widget_set_single_click_activate (GTK_LIST_ITEM_WIDGET (result), self->single_click_activate);

  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_widget_update (GTK_LIST_ITEM_WIDGET (result), position, item, selected);
  g_object_unref (item);
  gtk_widget_insert_after (result, self->widget, prev_sibling);

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
static GtkWidget *
gtk_list_item_manager_try_reacquire_list_item (GtkListItemManager *self,
                                               GHashTable         *change,
                                               guint               position,
                                               GtkWidget          *prev_sibling)
{
  GtkWidget *result;
  gpointer item;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);
  g_return_val_if_fail (prev_sibling == NULL || GTK_IS_WIDGET (prev_sibling), NULL);

  /* XXX: can we avoid temporarily allocating items on failure? */
  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  if (g_hash_table_steal_extended (change, item, NULL, (gpointer *) &result))
    {
      GtkListItemWidget *list_item = GTK_LIST_ITEM_WIDGET (result);
      gtk_list_item_widget_update (list_item,
                                   position,
                                   gtk_list_item_widget_get_item (list_item),
                                   gtk_selection_model_is_selected (self->model, position));
      gtk_widget_insert_after (result, self->widget, prev_sibling);
      /* XXX: Should we let the listview do this? */
      gtk_widget_queue_resize (result);
    }
  else
    {
      result = NULL;
    }
  g_object_unref (item);

  return result;
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
static void
gtk_list_item_manager_move_list_item (GtkListItemManager     *self,
                                      GtkWidget              *list_item,
                                      guint                   position,
                                      GtkWidget              *prev_sibling)
{
  gpointer item;
  gboolean selected;

  item = g_list_model_get_item (G_LIST_MODEL (self->model), position);
  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_widget_update (GTK_LIST_ITEM_WIDGET (list_item),
                               position,
                               item,
                               selected);
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
static void
gtk_list_item_manager_update_list_item (GtkListItemManager *self,
                                        GtkWidget          *item,
                                        guint               position)
{
  GtkListItemWidget *list_item = GTK_LIST_ITEM_WIDGET (item);
  gboolean selected;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_WIDGET (item));

  selected = gtk_selection_model_is_selected (self->model, position);
  gtk_list_item_widget_update (list_item,
                               position,
                               gtk_list_item_widget_get_item (list_item),
                               selected);
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
static void
gtk_list_item_manager_release_list_item (GtkListItemManager *self,
                                         GHashTable         *change,
                                         GtkWidget          *item)
{
  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));
  g_return_if_fail (GTK_IS_LIST_ITEM_WIDGET (item));

  if (change != NULL)
    {
      if (!g_hash_table_replace (change, gtk_list_item_widget_get_item (GTK_LIST_ITEM_WIDGET (item)), item))
        {
          g_warning ("FIXME: Handle the same item multiple times in the list.\nLars says this totally should not happen, but here we are.");
        }

      return;
    }

  gtk_widget_unparent (item);
}

void
gtk_list_item_manager_set_single_click_activate (GtkListItemManager *self,
                                                 gboolean            single_click_activate)
{
  GtkListItemManagerItem *item;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));

  self->single_click_activate = single_click_activate;

  for (item = gtk_rb_tree_get_first (self->items);
       item != NULL;
       item = gtk_rb_tree_node_get_next (item))
    {
      if (item->widget)
        gtk_list_item_widget_set_single_click_activate (GTK_LIST_ITEM_WIDGET (item->widget), single_click_activate);
    }
}

gboolean
gtk_list_item_manager_get_single_click_activate (GtkListItemManager   *self)
{
  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), FALSE);

  return self->single_click_activate;
}

GtkListItemTracker *
gtk_list_item_tracker_new (GtkListItemManager *self)
{
  GtkListItemTracker *tracker;

  g_return_val_if_fail (GTK_IS_LIST_ITEM_MANAGER (self), NULL);

  tracker = g_slice_new0 (GtkListItemTracker);

  tracker->position = GTK_INVALID_LIST_POSITION;

  self->trackers = g_slist_prepend (self->trackers, tracker);

  return tracker;
}

void
gtk_list_item_tracker_free (GtkListItemManager *self,
                            GtkListItemTracker *tracker)
{
  gtk_list_item_tracker_unset_position (self, tracker);

  self->trackers = g_slist_remove (self->trackers, tracker);

  g_slice_free (GtkListItemTracker, tracker);

  gtk_list_item_manager_ensure_items (self, NULL, G_MAXUINT);

  gtk_widget_queue_resize (self->widget);
}

void
gtk_list_item_tracker_set_position (GtkListItemManager *self,
                                    GtkListItemTracker *tracker,
                                    guint               position,
                                    guint               n_before,
                                    guint               n_after)
{
  GtkListItemManagerItem *item;
  guint n_items;

  gtk_list_item_tracker_unset_position (self, tracker);

  if (self->model == NULL)
    return;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
  if (position >= n_items)
    position = n_items - 1; /* for n_items == 0 this underflows to GTK_INVALID_LIST_POSITION */

  tracker->position = position;
  tracker->n_before = n_before;
  tracker->n_after = n_after;

  gtk_list_item_manager_ensure_items (self, NULL, G_MAXUINT);

  item = gtk_list_item_manager_get_nth (self, position, NULL);
  if (item)
    tracker->widget = GTK_LIST_ITEM_WIDGET (item->widget);

  gtk_widget_queue_resize (self->widget);
}

guint
gtk_list_item_tracker_get_position (GtkListItemManager *self,
                                    GtkListItemTracker *tracker)
{
  return tracker->position;
}
