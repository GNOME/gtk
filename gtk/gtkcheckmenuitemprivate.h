/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#ifndef __GTK_CHECK_MENU_ITEM_PRIVATE_H__
#define __GTK_CHECK_MENU_ITEM_PRIVATE_H__

#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkcssgadgetprivate.h>

G_BEGIN_DECLS

void       _gtk_check_menu_item_set_active       (GtkCheckMenuItem *check_menu_item,
                                                  gboolean          is_active);
GtkCssGadget * _gtk_check_menu_item_get_indicator_gadget (GtkCheckMenuItem *check_menu_item);

G_END_DECLS

#endif /* __GTK_CHECK_MENU_ITEM_PRIVATE_H__ */
