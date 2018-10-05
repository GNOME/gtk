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

#include "gtk/gtklistitemfactoryprivate.h"
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
typedef struct _GtkListItemManagerChange GtkListItemManagerChange;

GType                   gtk_list_item_manager_get_type          (void) G_GNUC_CONST;

GtkListItemManager *    gtk_list_item_manager_new               (GtkWidget              *widget);

void                    gtk_list_item_manager_set_factory       (GtkListItemManager     *self,
                                                                 GtkListItemFactory     *factory);
GtkListItemFactory *    gtk_list_item_manager_get_factory       (GtkListItemManager     *self);
void                    gtk_list_item_manager_set_model         (GtkListItemManager     *self,
                                                                 GtkSelectionModel      *model);
GtkSelectionModel *     gtk_list_item_manager_get_model         (GtkListItemManager     *self);

guint                   gtk_list_item_manager_get_size          (GtkListItemManager     *self);

void                    gtk_list_item_manager_select            (GtkListItemManager     *self,
                                                                 GtkListItem            *item,
                                                                 gboolean                modify,
                                                                 gboolean                extend);

GtkListItemManagerChange *
                        gtk_list_item_manager_begin_change      (GtkListItemManager     *self);
void                    gtk_list_item_manager_end_change        (GtkListItemManager     *self,
                                                                 GtkListItemManagerChange *change);
gboolean                gtk_list_item_manager_change_contains   (GtkListItemManagerChange *change,
                                                                 GtkWidget              *list_item);

GtkWidget *             gtk_list_item_manager_acquire_list_item (GtkListItemManager     *self,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
GtkWidget *             gtk_list_item_manager_try_reacquire_list_item
                                                                (GtkListItemManager     *self,
                                                                 GtkListItemManagerChange *change,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
void                    gtk_list_item_manager_update_list_item  (GtkListItemManager     *self,
                                                                 GtkWidget              *item,
                                                                 guint                   position);
void                    gtk_list_item_manager_move_list_item    (GtkListItemManager     *self,
                                                                 GtkWidget              *list_item,
                                                                 guint                   position,
                                                                 GtkWidget              *prev_sibling);
void                    gtk_list_item_manager_release_list_item (GtkListItemManager     *self,
                                                                 GtkListItemManagerChange *change,
                                                                 GtkWidget              *widget);

G_END_DECLS

#endif /* __GTK_LIST_ITEM_MANAGER_H__ */
