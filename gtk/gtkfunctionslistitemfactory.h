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

#ifndef __GTK_FUNCTIONS_LIST_ITEM_FACTORY_H__
#define __GTK_FUNCTIONS_LIST_ITEM_FACTORY_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtklistitemfactory.h>
#include <gtk/gtklistitem.h>

G_BEGIN_DECLS

#define GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY         (gtk_functions_list_item_factory_get_type ())
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactory))
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactoryClass))
#define GTK_IS_FUNCTIONS_LIST_ITEM_FACTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY))
#define GTK_IS_FUNCTIONS_LIST_ITEM_FACTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY))
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactoryClass))

typedef struct _GtkFunctionsListItemFactory GtkFunctionsListItemFactory;
typedef struct _GtkFunctionsListItemFactoryClass GtkFunctionsListItemFactoryClass;

/**
 * GtkListItemSetupFunc:
 * @item: the #GtkListItem to set up
 * @user_data: (closure): user data
 *
 * Called whenever a new list item needs to be setup for managing a row in
 * the list.
 *
 * At this point, the list item is not bound yet, so gtk_list_item_get_item()
 * will return %NULL.
 * The list item will later be bound to an item via the #GtkListItemBindFunc.
 */
typedef void (* GtkListItemSetupFunc) (GtkListItem *item, gpointer user_data);

/**
 * GtkListItemBindFunc:
 * @item: the #GtkListItem to bind
 * @user_data: (closure): user data
 *
 * Binds a#GtkListItem previously set up via a #GtkListItemSetupFunc to
 * an @item.
 *
 * Rebinding a @item to different @items is supported as well as
 * unbinding it by setting @item to %NULL.
 */
typedef void (* GtkListItemBindFunc) (GtkListItem *item,
                                      gpointer   user_data);

GDK_AVAILABLE_IN_ALL
GType                   gtk_functions_list_item_factory_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkListItemFactory *    gtk_functions_list_item_factory_new     (GtkListItemSetupFunc    setup_func,
                                                                 GtkListItemBindFunc     bind_func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          user_destroy);


G_END_DECLS

#endif /* __GTK_FUNCTIONS_LIST_ITEM_FACTORY_H__ */
