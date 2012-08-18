/*
 * Copyright © 2011 William Hua
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
 * Author: William Hua <william@attente.ca>
 */

#ifndef __GTK_MODELMENU_QUARTZ_H__
#define __GTK_MODELMENU_QUARTZ_H__

#include "gtkapplication.h"

void gtk_quartz_set_main_menu   (GMenuModel     *model,
                                 GtkApplication *application);

void gtk_quartz_clear_main_menu (void);

#endif /* __GTK_MODELMENU_QUARTZ_H__ */
