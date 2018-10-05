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

#ifndef __GTK_LIST_ITEM_PRIVATE_H__
#define __GTK_LIST_ITEM_PRIVATE_H__

#include "gtklistitem.h"

#include "gtklistitemmanagerprivate.h"

G_BEGIN_DECLS

GtkListItem *   gtk_list_item_new                               (GtkListItemManager     *manager,
                                                                 const char             *css_name);

void            gtk_list_item_set_item                          (GtkListItem            *self,
                                                                 gpointer                item);
void            gtk_list_item_set_position                      (GtkListItem            *self,
                                                                 guint                   position);
void            gtk_list_item_set_selected                      (GtkListItem            *self,
                                                                 gboolean                selected);

G_END_DECLS

#endif  /* __GTK_LIST_ITEM_PRIVATE_H__ */
