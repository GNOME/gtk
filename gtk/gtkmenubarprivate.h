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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_MENU_BAR_PRIVATE_H__
#define __GTK_MENU_BAR_PRIVATE_H__


#include <gtk/gtkmenubar.h>


G_BEGIN_DECLS


void _gtk_menu_bar_cycle_focus (GtkMenuBar       *menubar,
				GtkDirectionType  dir);
GList* _gtk_menu_bar_get_viewable_menu_bars (GtkWindow *window);


G_END_DECLS


#endif /* __GTK_MENU_BAR_PRIVATE_H__ */
