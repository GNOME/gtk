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


#ifndef __GTK_LIST_ITEM_MANAGER_H__
#define __GTK_LIST_ITEM_MANAGER_H__

#include "gtk/gtktypes.h"
#include "gtk/gtkenums.h"

#include "gtk/gtklistitemfactory.h"
#include "gtk/gtkrbtreeprivate.h"
#include "gtk/gtkselectionmodel.h"

G_BEGIN_DECLS

#define GTK_TYPE_LIST_ITEM_MANAGER         (gtk_list_item_manager_get_type ())
#define GTK_LIST_ITEM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManager))
#define GTK_LIST_ITEM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManagerClass))
#define GTK_IS_LIST_ITEM_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_LIST_ITEM_MANAGER))
#define GTK_IS_LIST_ITEM_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_LIST_ITEM_MANAGER))
#define GTK_LIST_ITEM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_LIST_ITEM_MANAGER, GtkListItemManagerClass))

typedef struct _GtkListItemManager GtkListItemManager;
typedef struct _GtkListItemManagerClass GtkListItemManagerClass;
typedef struct _GtkListTile GtkListTile;
typedef struct _GtkListTileAugment GtkListTileAugment;
typedef struct _GtkListItemTracker GtkListItemTracker;

struct _GtkListTile
{
  GtkWidget *widget;
  guint n_items;
  /* area occupied by tile. May be empty if tile has no allcoation */
  cairo_rectangle_int_t area;
};

struct _GtkListTileAugment
{
  guint n_items;
  /* union of all areas of tile and children */
  cairo_rectangle_int_t area;
};


GType                   gtk_list_item_manager_get_type          (void) G_GNUC_CONST;

GtkListItemManager *    gtk_list_item_manager_new               (GtkWidget              *widget,
                                                                 const char             *item_css_name,
                                                                 GtkAccessibleRole       item_role,
                                                                 GtkListTile *           (* split_func) (gpointer, GtkListTile *, guint),
                                                                 gpointer                user_data);

void                    gtk_list_item_manager_get_tile_bounds   (GtkListItemManager     *self,
                                                                 GdkRectangle           *out_bounds);
gpointer                gtk_list_item_manager_get_root          (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_first         (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_last          (GtkListItemManager     *self);
gpointer                gtk_list_item_manager_get_nth           (GtkListItemManager     *self,
                                                                 guint                   position,
                                                                 guint                  *offset);
GtkListTile *           gtk_list_item_manager_get_tile_at       (GtkListItemManager     *self,
                                                                 int                     x,
                                                                 int                     y);
GtkListTile *           gtk_list_item_manager_get_nearest_tile  (GtkListItemManager     *self,
                                                                 int                     x,
                                                                 int                     y);


guint                   gtk_list_tile_get_position              (GtkListItemManager     *self,
                                                                 GtkListTile            *tile);
gpointer                gtk_list_tile_get_augment               (GtkListItemManager     *self,
                                                                 GtkListTile            *tile);
void                    gtk_list_tile_set_area                  (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 const cairo_rectangle_int_t *area);
void                    gtk_list_tile_set_area_position         (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 int                     x,
                                                                 int                     y);
void                    gtk_list_tile_set_area_size             (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 int                     width,
                                                                 int                     height);

GtkListTile *           gtk_list_tile_split                     (GtkListItemManager     *self,
                                                                 GtkListTile            *tile,
                                                                 guint                   n_items);
GtkListTile *           gtk_list_tile_gc                        (GtkListItemManager     *self,
                                                                 GtkListTile            *tile);

void                    gtk_list_item_manager_set_factory       (GtkListItemManager     *self,
                                                                 GtkListItemFactory     *factory);
GtkListItemFactory *    gtk_list_item_manager_get_factory       (GtkListItemManager     *self);
void                    gtk_list_item_manager_set_model         (GtkListItemManager     *self,
                                                                 GtkSelectionModel      *model);
GtkSelectionModel *     gtk_list_item_manager_get_model         (GtkListItemManager     *self);

guint                   gtk_list_item_manager_get_size          (GtkListItemManager     *self);
void                    gtk_list_item_manager_set_single_click_activate
                                                                (GtkListItemManager     *self,
                                                                 gboolean                single_click_activate);
gboolean                gtk_list_item_manager_get_single_click_activate
                                                                (GtkListItemManager     *self);

GtkListItemTracker *    gtk_list_item_tracker_new               (GtkListItemManager     *self);
void                    gtk_list_item_tracker_free              (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker);
void                    gtk_list_item_tracker_set_position      (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker,
                                                                 guint                   position,
                                                                 guint                   n_before,
                                                                 guint                   n_after);
guint                   gtk_list_item_tracker_get_position      (GtkListItemManager     *self,
                                                                 GtkListItemTracker     *tracker);


G_END_DECLS

#endif /* __GTK_LIST_ITEM_MANAGER_H__ */
