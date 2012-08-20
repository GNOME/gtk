/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
 * Copyright (C) 2012 Bastien Nocera
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

#ifndef __GTK_MENU_BUTTON_PRIVATE_H__
#define __GTK_MENU_BUTTON_PRIVATE_H__

#include <gtk/gtkmenubutton.h>

G_BEGIN_DECLS

typedef void (* GtkMenuButtonShowMenuCallback) (gpointer user_data);

void       _gtk_menu_button_set_popup_with_func (GtkMenuButton                 *menu_button,
                                                 GtkWidget                     *menu,
                                                 GtkMenuButtonShowMenuCallback  func,
                                                 gpointer                       user_data);

G_END_DECLS

#endif /* __GTK_MENU_BUTTON_PRIVATE_H__ */
