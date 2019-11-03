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


#ifndef __GTK_LIST_ITEM_FACTORY_PRIVATE_H__
#define __GTK_LIST_ITEM_FACTORY_PRIVATE_H__

#include <gtk/gtklistitem.h>
#include "gtk/gtklistitemwidgetprivate.h"

G_BEGIN_DECLS

struct _GtkListItemFactory
{
  GObject parent_instance;
};

struct _GtkListItemFactoryClass
{
  GObjectClass parent_class;

  /* setup @list_item so it can be bound */
  void                  (* setup)                               (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget,
                                                                 GtkListItem            *list_item);
  /* undo the effects of GtkListItemFactoryClass::setup() */
  void                  (* teardown)                            (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget,
                                                                 GtkListItem            *list_item);

  /* Update properties on @list_item to the given @item, which is in @position and @selected state.
   * One or more of those properties might be unchanged. */
  void                  (* update)                              (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget,
                                                                 GtkListItem            *list_item,
                                                                 guint                   position,
                                                                 gpointer                item,
                                                                 gboolean                selected);
};

void                    gtk_list_item_factory_setup             (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget);
void                    gtk_list_item_factory_teardown          (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget);

void                    gtk_list_item_factory_update            (GtkListItemFactory     *self,
                                                                 GtkListItemWidget      *widget,
                                                                 guint                   position,
                                                                 gpointer                item,
                                                                 gboolean                selected);


G_END_DECLS

#endif /* __GTK_LIST_ITEM_FACTORY_PRIVATE_H__ */
