/* GTK - The GIMP Toolkit
 * Copyright 2019 Matthias Clasen
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

#ifndef __GTK_DRAG_ICON_PRIVATE_H__
#define __GTK_DRAG_ICON_PRIVATE_H__

#include <gio/gio.h>
#include <gtk/gtkdragicon.h>

G_BEGIN_DECLS

GtkWidget *     gtk_drag_icon_new                          (void);

void            gtk_drag_icon_set_surface                  (GtkDragIcon *icon,
                                                            GdkSurface  *surface);
void            gtk_drag_icon_set_widget                   (GtkDragIcon *icon,
                                                            GtkWidget   *widget);

G_END_DECLS

#endif /* __GTK_DRAG_ICON_PRIVATE_H__ */
