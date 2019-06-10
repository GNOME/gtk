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

#include "gtklistitemfactoryprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY         (gtk_functions_list_item_factory_get_type ())
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactory))
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactoryClass))
#define GTK_IS_FUNCTIONS_LIST_ITEM_FACTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY))
#define GTK_IS_FUNCTIONS_LIST_ITEM_FACTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY))
#define GTK_FUNCTIONS_LIST_ITEM_FACTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_FUNCTIONS_LIST_ITEM_FACTORY, GtkFunctionsListItemFactoryClass))

typedef struct _GtkFunctionsListItemFactory GtkFunctionsListItemFactory;
typedef struct _GtkFunctionsListItemFactoryClass GtkFunctionsListItemFactoryClass;

GType                   gtk_functions_list_item_factory_get_type (void) G_GNUC_CONST;

GtkListItemFactory *    gtk_functions_list_item_factory_new     (GtkListItemSetupFunc    setup_func,
                                                                 GtkListItemBindFunc     bind_func,
                                                                 gpointer                user_data,
                                                                 GDestroyNotify          user_destroy);


G_END_DECLS

#endif /* __GTK_FUNCTIONS_LIST_ITEM_FACTORY_H__ */
