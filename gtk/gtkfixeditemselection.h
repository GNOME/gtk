/*
 * Copyright © 2021 Benjamin Otte
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

#ifndef __GTK_FIXED_ITEM_SELECTION_H__
#define __GTK_FIXED_ITEM_SELECTION_H__

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_FIXED_ITEM_SELECTION (gtk_fixed_item_selection_get_type ())

GDK_AVAILABLE_IN_4_6
G_DECLARE_FINAL_TYPE (GtkFixedItemSelection, gtk_fixed_item_selection, GTK, FIXED_ITEM_SELECTION, GObject)

GDK_AVAILABLE_IN_4_6
GtkFixedItemSelection * gtk_fixed_item_selection_new                    (GListModel             *model);

GDK_AVAILABLE_IN_4_6
GListModel *            gtk_fixed_item_selection_get_model              (GtkFixedItemSelection  *self);
GDK_AVAILABLE_IN_4_6
void                    gtk_fixed_item_selection_set_model              (GtkFixedItemSelection  *self,
                                                                         GListModel             *model);
GDK_AVAILABLE_IN_4_6
gpointer                gtk_fixed_item_selection_get_selected_item      (GtkFixedItemSelection  *self);
GDK_AVAILABLE_IN_4_6
void                    gtk_fixed_item_selection_set_selected_item      (GtkFixedItemSelection  *self,
                                                                         gpointer                item);
GDK_AVAILABLE_IN_4_6
void                    gtk_fixed_item_selection_set_selected_position  (GtkFixedItemSelection  *self,
                                                                         guint                   position);


G_END_DECLS

#endif /* __GTK_FIXED_ITEM_SELECTION_H__ */
