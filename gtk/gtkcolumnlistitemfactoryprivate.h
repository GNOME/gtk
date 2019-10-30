/*
 * Copyright © 2019 Benjamin Otte
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

#ifndef __GTK_COLUMN_LIST_ITEM_FACTORY_H__
#define __GTK_COLUMN_LIST_ITEM_FACTORY_H__

#include <gtk/gtklistitemwidgetprivate.h>
#include <gtk/gtkcolumnview.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_LIST_ITEM_FACTORY         (gtk_column_list_item_factory_get_type ())
#define GTK_COLUMN_LIST_ITEM_FACTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_LIST_ITEM_FACTORY, GtkColumnListItemFactory))
#define GTK_COLUMN_LIST_ITEM_FACTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_LIST_ITEM_FACTORY, GtkColumnListItemFactoryClass))
#define GTK_IS_COLUMN_LIST_ITEM_FACTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_LIST_ITEM_FACTORY))
#define GTK_IS_COLUMN_LIST_ITEM_FACTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_LIST_ITEM_FACTORY))
#define GTK_COLUMN_LIST_ITEM_FACTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_LIST_ITEM_FACTORY, GtkColumnListItemFactoryClass))

/**
 * GtkColumnListItemFactory:
 *
 * The object for the #GtkColumnListItemFactory.
 **/
typedef struct _GtkColumnListItemFactory GtkColumnListItemFactory;
typedef struct _GtkColumnListItemFactoryClass GtkColumnListItemFactoryClass;


GType                   gtk_column_list_item_factory_get_type   (void) G_GNUC_CONST;

GtkColumnListItemFactory *
                        gtk_column_list_item_factory_new        (GtkColumnView          *view);

void                    gtk_column_list_item_factory_add_column (GtkColumnListItemFactory       *factory,
                                                                 GtkListItemWidget              *list_item,
                                                                 GtkColumnViewColumn            *column,
                                                                 gboolean                        check_bind);
void                    gtk_column_list_item_factory_remove_column
                                                                (GtkColumnListItemFactory       *factory,
                                                                 GtkListItemWidget              *list_item,
                                                                 guint                           col_pos,
                                                                 GtkColumnViewColumn            *column);


G_END_DECLS

#endif /* __GTK_COLUMN_LIST_ITEM_FACTORY_H__ */
