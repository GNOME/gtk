/* GTK - The GIMP Toolkit
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_POPOVER_MENU_PRIVATE_H__
#define __GTK_POPOVER_MENU_PRIVATE_H__

#include "gtkpopovermenu.h"

G_BEGIN_DECLS

GtkWidget *gtk_popover_menu_get_active_item  (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_active_item  (GtkPopoverMenu *menu,
                                              GtkWidget      *item);
GtkWidget *gtk_popover_menu_get_open_submenu (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_open_submenu (GtkPopoverMenu *menu,
                                              GtkWidget      *submenu);
GtkWidget *gtk_popover_menu_get_parent_menu  (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_parent_menu  (GtkPopoverMenu *menu,
                                              GtkWidget      *parent);

G_END_DECLS

#endif /* __GTK_POPOVER_PRIVATE_H__ */
