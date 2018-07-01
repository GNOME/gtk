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

#ifndef __GTK_ZOOM_H__
#define __GTK_ZOOM_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_ZOOM         (gtk_zoom_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkZoom, gtk_zoom, GTK, ZOOM, GtkContainer)

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_zoom_new                            (void);

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_zoom_get_child                      (GtkZoom                *zoom);
GDK_AVAILABLE_IN_ALL
void            gtk_zoom_set_child                      (GtkZoom                *zoom,
                                                         GtkWidget              *widget);


G_END_DECLS

#endif /* __GTK_ZOOM_H__ */
