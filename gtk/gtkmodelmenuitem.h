/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __GTK_MODEL_MENU_ITEM_H__
#define __GTK_MODEL_MENU_ITEM_H__

#include <gtk/gtkcheckmenuitem.h>

#define GTK_TYPE_MODEL_MENU_ITEM                            (gtk_model_menu_item_get_type ())
#define GTK_MODEL_MENU_ITEM(inst)                           (G_TYPE_CHECK_INSTANCE_CAST ((inst),                      \
                                                             GTK_TYPE_MODEL_MENU_ITEM, GtkModelMenuItem))
#define GTK_IS_MODEL_MENU_ITEM(inst)                        (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                      \
                                                             GTK_TYPE_MODEL_MENU_ITEM))

typedef struct _GtkModelMenuItem                            GtkModelMenuItem;

G_GNUC_INTERNAL
GType                   gtk_model_menu_item_get_type                    (void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkMenuItem *           gtk_model_menu_item_new                         (GMenuModel        *model,
                                                                         gint               item_index,
                                                                         const gchar       *action_namespace);

#endif /* __GTK_MODEL_MENU_ITEM_H__ */
