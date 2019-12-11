/* GTK - The GIMP Toolkit
 * Copyright (C) 2014, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */
#ifndef __GTK_GESTURE_PAN_H__
#define __GTK_GESTURE_PAN_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkenums.h>
#include <gtk/gtkgesturedrag.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_PAN         (gtk_gesture_pan_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GtkGesturePan, gtk_gesture_pan, GTK, GESTURE_PAN)

GDK_AVAILABLE_IN_ALL
GtkGesture *      gtk_gesture_pan_new             (GtkOrientation  orientation);

GDK_AVAILABLE_IN_ALL
GtkOrientation    gtk_gesture_pan_get_orientation (GtkGesturePan  *gesture);

GDK_AVAILABLE_IN_ALL
void              gtk_gesture_pan_set_orientation (GtkGesturePan  *gesture,
                                                   GtkOrientation  orientation);


G_END_DECLS

#endif /* __GTK_GESTURE_PAN_H__ */
