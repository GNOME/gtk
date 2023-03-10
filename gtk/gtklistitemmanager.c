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

  GtkListTile * (* split_func) (gpointer, GtkListTile *, guint);
  gpointer user_data;
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

static void
potentially_empty_rectangle_union (cairo_rectangle_int_t       *self,
                                   const cairo_rectangle_int_t *area)
{
  if (area->width <= 0 || area->height <= 0)
    return;

  if (self->width <= 0 || self->height <= 0)
    {
      *self = *area;
      return;
    }

  gdk_rectangle_union (self, area, self);
}

static void
gtk_list_item_manager_augment_node (GtkRbTree *tree,
                                    gpointer   node_augment,
                                    gpointer   node,
                                    gpointer   left,
                                    gpointer   right)
{
  GtkListTile *tile = node;
  GtkListTileAugment *aug = node_augment;

  aug->n_items = tile->n_items;
  aug->area = tile->area;

  if (left)
    {
      GtkListTileAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->n_items += left_aug->n_items;
      potentially_empty_rectangle_union (&aug->area, &left_aug->area);
    }

  if (right)
    {
      GtkListTileAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->n_items += right_aug->n_items;
      potentially_empty_rectangle_union (&aug->area, &right_aug->area);
    }
}

static void
gtk_list_item_manager_clear_node (gpointer _tile)
{
  GtkListTile *tile G_GNUC_UNUSED = _tile;

  g_assert (tile->widget == NULL);
}

GtkListItemManager *
gtk_list_item_manager_new (GtkWidget         *widget,
                           const char        *item_css_name,
                           GtkAccessibleRole  item_role,
                           GtkListTile *      (* split_func) (gpointer, GtkListTile *, guint),
                           gpointer           user_data)
{
  GtkListItemManager *self;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  self = g_object_new (GTK_TYPE_LIST_ITEM_MANAGER, NULL);

  /* not taking a ref because the widget refs us */
  self->widget = widget;
  self->item_css_name = g_intern_string (item_css_name);
  self->item_role = item_role;
  self->split_func = split_func;
  self->user_data = user_data;

  self->items = gtk_rb_tree_new_for_size (sizeof (GtkListTile),
                                          sizeof (GtkListTileAugment),
                                          gtk_list_item_manager_augment_node,
                                          gtk_list_item_manager_clear_node,
                                          NULL);

  return self;
}

void
gtk_list_item_manager_get_tile_bounds (GtkListItemManager *self,
                                       GdkRectangle       *out_bounds)
{
  GtkListTile *tile;
  GtkListTileAugment *aug;

  tile = gtk_rb_tree_get_root (self->items);
  if (tile == NULL)
    {
      *out_bounds = (GdkRectangle) { 0, 0, 0, 0 };
      return;
    }

  aug = gtk_rb_tree_get_augment (self->items, tile);
  *out_bounds = aug->area;
}

gpointer
gtk_list_item_manager_get_first (GtkListItemManager *self)
{
  return gtk_rb_tree_get_first (self->items);
}

gpointer
gtk_list_item_manager_get_last (GtkListItemManager *self)
{
  return gtk_rb_tree_get_last (self->items);
}

gpointer
gtk_list_item_manager_get_root (GtkListItemManager *self)
{
  return gtk_rb_tree_get_root (self->items);
}

/*
 * gtk_list_item_manager_get_nth:
 * @self: a `GtkListItemManager`
 * @position: position of the item
 * @offset: (out): offset into the returned tile
 *
 * Looks up the GtkListTile that represents @position.
 *
 * If a the returned tile represents multiple rows, the @offset into
 * the returned tile for @position will be set. If the returned tile
 * represents a row with an existing widget, @offset will always be 0.
 *
 * Returns: (type GtkListTile): the tile for @position or
 *   %NULL if position is out of range
 **/
gpointer
gtk_list_item_manager_get_nth (GtkListItemManager *self,
                               guint               position,
                               guint              *offset)
{
  GtkListTile *tile, *tmp;

  tile = gtk_rb_tree_get_root (self->items);

  while (tile)
    {
      tmp = gtk_rb_tree_node_get_left (tile);
      if (tmp)
        {
          GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, tmp);
          if (position < aug->n_items)
            {
              tile = tmp;
              continue;
            }
          position -= aug->n_items;
        }

      if (position < tile->n_items)
        break;
      position -= tile->n_items;

      tile = gtk_rb_tree_node_get_right (tile);
    }

  if (offset)
    *offset = tile ? position : 0;

  return tile;
}

/* This computes Manhattan distance */
static int
rectangle_distance (const cairo_rectangle_int_t *rect,
                    int                          x,
                    int                          y)
{
  int x_dist, y_dist;

  if (rect->x > x)
    x_dist = rect->x - x;
  else if (rect->x + rect->width < x)
    x_dist = x - (rect->x + rect->width);
  else
    x_dist = 0;

  if (rect->y > y)
    y_dist = rect->y - y;
  else if (rect->y + rect->height < y)
    y_dist = y - (rect->y + rect->height);
  else
    y_dist = 0;

  return x_dist + y_dist;
}

static GtkListTile *
gtk_list_tile_get_tile_at (GtkListItemManager *self,
                           GtkListTile        *tile,
                           int                 x,
                           int                 y,
                           int                *distance)
{
  GtkListTileAugment *aug;
  GtkListTile *left, *right, *result;
  int dist, left_dist, right_dist;

  left = gtk_rb_tree_node_get_left (tile);
  if (left)
    {
      aug = gtk_list_tile_get_augment (self, left);
      left_dist = rectangle_distance (&aug->area, x, y);
    }
  else
    left_dist = *distance;
  right = gtk_rb_tree_node_get_right (tile);
  if (right)
    {
      aug = gtk_list_tile_get_augment (self, right);
      right_dist = rectangle_distance (&aug->area, x, y);
    }
  else
    right_dist = *distance;

  dist = rectangle_distance (&tile->area, x, y);
  result = NULL;

  while (TRUE)
    {
      if (dist < left_dist && dist < right_dist)
        {
          if (dist >= *distance)
            return result;

          *distance = dist;
          return tile;
        }

      if (left_dist < right_dist)
        {
          if (left_dist >= *distance)
            return result;

          left = gtk_list_tile_get_tile_at (self, left, x, y, distance);
          if (left)
            result = left;
          left_dist = G_MAXINT;
        }
      else
        {
          if (right_dist >= *distance)
            return result;

          right = gtk_list_tile_get_tile_at (self, right, x, y, distance);
          if (right)
            result = right;
          right_dist = G_MAXINT;
        }
    }
}

/*
 * gtk_list_item_manager_get_tile_at:
 * @self: a GtkListItemManager
 * @x: x coordinate of tile
 * @y: y coordinate of tile
 *
 * Finds the tile occupying the coordinates at (x, y). If no
 * tile occupies the coordinates (for example, if the tile is out of bounds),
 * NULL is returned.
 *
 * Returns: (nullable): The tile at (x, y) or NULL
 **/
GtkListTile *
gtk_list_item_manager_get_tile_at (GtkListItemManager *self,
                                   int                 x,
                                   int                 y)
{
  int distance = 1;

  return gtk_list_tile_get_tile_at (self, gtk_list_item_manager_get_root (self), x, y, &distance);
}

/*
 * gtk_list_item_manager_get_nearest_tile:
 * @self: a GtkListItemManager
 * @x: x coordinate of tile
 * @y: y coordinate of tile
 *
 * Finds the tile closest to the coordinates at (x, y). If no
 * tile occupies the coordinates (for example, if the tile is out of bounds),
 * Manhattan distance is used to find the nearest tile.
 *
 * If multiple tiles have the same distance, the one closest to the start
 * will be returned.
 *
 * Returns: (nullable): The tile nearest to (x, y) or NULL if there are no
 *     tile
 **/
GtkListTile *
gtk_list_item_manager_get_nearest_tile (GtkListItemManager *self,
                                        int                 x,
                                        int                 y)
{
  int distance = G_MAXINT;

  return gtk_list_tile_get_tile_at (self, gtk_list_item_manager_get_root (self), x, y, &distance);
}

guint
gtk_list_tile_get_position (GtkListItemManager *self,
                            GtkListTile        *tile)
{
  GtkListTile *parent, *left;
  int pos;

  left = gtk_rb_tree_node_get_left (tile);
  if (left)
    {
      GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, left);
      pos = aug->n_items;
    }
  else
    {
      pos = 0; 
    }

  for (parent = gtk_rb_tree_node_get_parent (tile);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (tile))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != tile)
        {
          if (left)
            {
              GtkListTileAugment *aug = gtk_rb_tree_get_augment (self->items, left);
              pos += aug->n_items;
            }
          pos += parent->n_items;
        }

      tile = parent;
    }

  return pos;
}

gpointer
gtk_list_tile_get_augment (GtkListItemManager *self,
                           GtkListTile        *tile)
{
  return gtk_rb_tree_get_augment (self->items, tile);
}

/*
 * gtk_list_tile_set_area:
 * @self: the list item manager
 * @tile: tile to set area for
 * @area: (nullable): area to set or NULL to clear
 *     the area
 *
 * Updates the area of the tile.
 *
 * The area is given in the internal coordinate system,
 * so the x/y flip due to orientation and the left/right
 * flip for RTL languages will happen later.
 *
 * This function should only be called from inside size_allocate().
 **/
void
gtk_list_tile_set_area (GtkListItemManager          *self,
                        GtkListTile                 *tile,
                        const cairo_rectangle_int_t *area)
{
  cairo_rectangle_int_t empty_area = { 0, 0, 0, 0 };

  if (!area)
    area = &empty_area;

  if (gdk_rectangle_equal (&tile->area, area))
    return;

  tile->area = *area;
  gtk_rb_tree_node_mark_dirty (tile);
}

void
gtk_list_tile_set_area_position (GtkListItemManager *self,
                                 GtkListTile        *tile,
                                 int                 x,
                                 int                 y)
{
  if (tile->area.x == x && tile->area.y == y)
    return;

  tile->area.x = x;
  tile->area.y = y;
  gtk_rb_tree_node_mark_dirty (tile);
}

void
gtk_list_tile_set_area_size (GtkListItemManager *self,
                             GtkListTile        *tile,
                             int                 width,
                             int                 height)
{
  if (tile->area.width == width && tile->area.height == height)
    return;

  tile->area.width = width;
  tile->area.height = height;
  gtk_rb_tree_node_mark_dirty (tile);
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

static GtkListTile *
gtk_list_item_manager_ensure_split (GtkListItemManager *self,
                                    GtkListTile        *tile,
                                    guint               n_items)
{
  return self->split_func (self->user_data, tile, n_items);
}

static void
gtk_list_item_manager_remove_items (GtkListItemManager *self,
                                    GHashTable         *change,
                                    guint               position,
                                    guint               n_items)
{
  GtkListTile *tile, *next;
  guint offset;

  if (n_items == 0)
    return;

  tile = gtk_list_item_manager_get_nth (self, position, &offset);
  if (offset)
    tile = gtk_list_item_manager_ensure_split (self, tile, offset);

  while (n_items > 0)
    {
      if (tile->n_items > n_items)
        {
          gtk_list_item_manager_ensure_split (self, tile, n_items);
          g_assert (tile->n_items <= n_items);
        }

      next = gtk_rb_tree_node_get_next (tile);
      if (tile->widget)
        gtk_list_item_manager_release_list_item (self, change, tile->widget);
      tile->widget = NULL;
      n_items -= tile->n_items;
      tile->n_items = 0;
      gtk_rb_tree_node_mark_dirty (tile);

      tile = next;
    }

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static void
gtk_list_item_manager_add_items (GtkListItemManager *self,
                                 guint               position,
                                 guint               n_items)
{  
  GtkListTile *tile;
  guint offset;

  if (n_items == 0)
    return;

  tile = gtk_list_item_manager_get_nth (self, position, &offset);
  if (offset)
    tile = gtk_list_item_manager_ensure_split (self, tile, offset);

  tile = gtk_rb_tree_insert_before (self->items, tile);
  tile->n_items = n_items;
  gtk_rb_tree_node_mark_dirty (tile);

  gtk_widget_queue_resize (GTK_WIDGET (self->widget));
}

static gboolean
gtk_list_item_manager_merge_list_items (GtkListItemManager *self,
                                        GtkListTile        *first,
                                        GtkListTile        *second)
{
  if (first->widget || second->widget)
    return FALSE;

  first->n_items += second->n_items;
  gtk_rb_tree_node_mark_dirty (first);
  gtk_rb_tree_remove (self->items, second);

  return TRUE;
}

/*
 * gtk_list_tile_split:
 * @self: the listitemmanager
 * @tile: a tile to split into two
 * @n_items: nuber of items to keep in tile
 *
 * Splits the given tile into two tiles. The original
 * tile will remain with @n_items items, the remaining
 * items will be given to the new tile, which will be
 * nserted after the tile.
 *
 * It is valid for either tile to have 0 items after
 * the split.
 *
 * Returns: The new tile
 **/
GtkListTile *
gtk_list_tile_split (GtkListItemManager *self,
                     GtkListTile        *tile,
                     guint               n_items)
{
  GtkListTile *result;

  g_assert (n_items <= tile->n_items);

  result = gtk_rb_tree_insert_after (self->items, tile);
  result->n_items = tile->n_items - n_items;
  tile->n_items = n_items;
  gtk_rb_tree_node_mark_dirty (tile);

  return result;
}

/*
 * gtk_list_tile_gc:
 * @self: the listitemmanager
 * @tile: a tile
 *
 * Tries to get rid of tiles when they aren't needed anymore,
 * either because their referenced listitems were deleted or
 * because they can be merged with the next item(s).
 *
 * Note that this only looks forward, but never backward.
 *
 * Returns: The next tile
 **/
GtkListTile *
gtk_list_tile_gc (GtkListItemManager *self,
                  GtkListTile        *tile)
{
  GtkListTile *next;

  while (tile)
    {
      next = gtk_rb_tree_node_get_next (tile);

      if (tile->n_items == 0)
        {
          gtk_rb_tree_remove (self->items, tile);
          tile = next;
          continue;
        }

      if (next == NULL)
        break;

      if (gtk_list_item_manager_merge_list_items (self, tile, next))
        continue;

      break;
    }

  return tile;
}

static void
gtk_list_item_manager_release_items (GtkListItemManager *self,
                                     GQueue             *released)
{
  GtkListTile *tile;
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

      tile = gtk_list_item_manager_get_nth (self, position, &i);
      i = position - i;
      while (i < position + query_n_items)
        {
          g_assert (tile != NULL);
          if (tile->widget)
            {
              g_queue_push_tail (released, tile->widget);
              tile->widget = NULL;
            }
          i += tile->n_items;
          tile = gtk_rb_tree_node_get_next (tile);
        }
      position += query_n_items;
    }
}

static void
gtk_list_item_manager_ensure_items (GtkListItemManager *self,
                                    GHashTable         *change,
                                    guint               update_start)
{
  GtkListTile *tile, *other_tile;
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

      tile = gtk_list_item_manager_get_nth (self, position, &offset);
      for (other_tile = tile;
           other_tile && other_tile->widget == NULL;
           other_tile = gtk_rb_tree_node_get_previous (other_tile))
         { /* do nothing */ }
      insert_after = other_tile ? other_tile->widget : NULL;

      if (offset > 0)
        tile = gtk_list_item_manager_ensure_split (self, tile, offset);

      for (i = 0; i < query_n_items; i++)
        {
          g_assert (tile != NULL);

          while (tile->n_items == 0)
            tile = gtk_rb_tree_node_get_next (tile);

          if (tile->n_items > 1)
            gtk_list_item_manager_ensure_split (self, tile, 1);

          if (tile->widget == NULL)
            {
              if (change)
                {
                  tile->widget = gtk_list_item_manager_try_reacquire_list_item (self,
                                                                                change,
                                                                                position + i,
                                                                                insert_after);
                }
              if (tile->widget == NULL)
                {
                  tile->widget = g_queue_pop_head (&released);
                  if (tile->widget)
                    {
                      gtk_list_item_manager_move_list_item (self,
                                                            tile->widget,
                                                            position + i,
                                                            insert_after);
                    }
                  else
                    {
                      tile->widget = gtk_list_item_manager_acquire_list_item (self,
                                                                              position + i,
                                                                              insert_after);
                    }
                }
            }
          else
            {
              if (update_start <= position + i)
                gtk_list_item_manager_update_list_item (self, tile->widget, position + i);
            }
          insert_after = tile->widget;

          tile = gtk_rb_tree_node_get_next (tile);
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
      GtkListTile *tile, *new_tile;
      GtkWidget *insert_after;
      guint i, offset;
      
      tile = gtk_list_item_manager_get_nth (self, position, &offset);
      for (new_tile = tile ? gtk_rb_tree_node_get_previous (tile) : gtk_rb_tree_get_last (self->items);
           new_tile && new_tile->widget == NULL;
           new_tile = gtk_rb_tree_node_get_previous (new_tile))
        { }
      if (new_tile)
        insert_after = new_tile->widget;
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

          while (offset >= tile->n_items)
            {
              offset -= tile->n_items;
              tile = gtk_rb_tree_node_get_next (tile);
            }
          if (offset > 0)
            {
              tile = gtk_list_item_manager_ensure_split (self, tile, offset);
              offset = 0;
            }

          new_tile = tile;
          if (tile->n_items == 1)
            tile = gtk_rb_tree_node_get_next (tile);
          else
            tile = gtk_list_item_manager_ensure_split (self, tile, 1);

          new_tile->widget = widget;
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
      GtkListTile *tile;

      if (tracker->widget != NULL || 
          tracker->position == GTK_INVALID_LIST_POSITION)
        continue;

      tile = gtk_list_item_manager_get_nth (self, tracker->position, NULL);
      g_assert (tile != NULL);
      g_assert (tile->widget);
      tracker->widget = GTK_LIST_ITEM_WIDGET (tile->widget);
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
  GtkListTile *tile;
  guint offset;

  tile = gtk_list_item_manager_get_nth (self, position, &offset);

  if (offset)
    {
      position += tile->n_items - offset;
      if (tile->n_items - offset > n_items)
        n_items = 0;
      else
        n_items -= tile->n_items - offset;
      tile = gtk_rb_tree_node_get_next (tile);
    }

  while (n_items > 0)
    {
      if (tile->widget)
        gtk_list_item_manager_update_list_item (self, tile->widget, position);
      position += tile->n_items;
      n_items -= MIN (n_items, tile->n_items);
      tile = gtk_rb_tree_node_get_next (tile);
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
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

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
      GtkListTile *tile;

      if (tracker->widget == NULL)
        continue;

      tile = gtk_list_item_manager_get_nth (self, tracker->position, NULL);
      g_assert (tile);
      tracker->widget = GTK_LIST_ITEM_WIDGET (tile->widget);
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
 * @self: a `GtkListItemManager`
 * @position: the row in the model to create a list item for
 * @prev_sibling: the widget this widget should be inserted before or %NULL
 *   if it should be the first widget
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
 * @self: a `GtkListItemManager`
 * @position: the row in the model to create a list item for
 * @prev_sibling: the widget this widget should be inserted after or %NULL
 *   if it should be the first widget
 *
 * Like gtk_list_item_manager_acquire_list_item(), but only tries to acquire list
 * items from those previously released as part of @change.
 * If no matching list item is found, %NULL is returned and the caller should use
 * gtk_list_item_manager_acquire_list_item().
 *
 * Returns: (nullable): a properly setup widget to use in @position or %NULL if
 *   no item for reuse existed
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
 * @self: a `GtkListItemManager`
 * @list_item: an acquired `GtkListItem` that should be moved to represent
 *   a different row
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
 * @self: a `GtkListItemManager`
 * @item: a `GtkListItem` that has been acquired
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
 * @self: a `GtkListItemManager`
 * @change: (nullable): The change associated with this release or
 *   %NULL if this is a final removal
 * @item: an item previously acquired with
 *   gtk_list_item_manager_acquire_list_item()
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
          g_warning ("Duplicate item detected in list. Picking one randomly.");
        }

      return;
    }

  gtk_widget_unparent (item);
}

void
gtk_list_item_manager_set_single_click_activate (GtkListItemManager *self,
                                                 gboolean            single_click_activate)
{
  GtkListTile *tile;

  g_return_if_fail (GTK_IS_LIST_ITEM_MANAGER (self));

  self->single_click_activate = single_click_activate;

  for (tile = gtk_rb_tree_get_first (self->items);
       tile != NULL;
       tile = gtk_rb_tree_node_get_next (tile))
    {
      if (tile->widget)
        gtk_list_item_widget_set_single_click_activate (GTK_LIST_ITEM_WIDGET (tile->widget), single_click_activate);
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
  GtkListTile *tile;
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

  tile = gtk_list_item_manager_get_nth (self, position, NULL);
  if (tile)
    tracker->widget = GTK_LIST_ITEM_WIDGET (tile->widget);

  gtk_widget_queue_resize (self->widget);
}

guint
gtk_list_item_tracker_get_position (GtkListItemManager *self,
                                    GtkListItemTracker *tracker)
{
  return tracker->position;
}
