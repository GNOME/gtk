/* GTK - The GIMP Toolkit
 * Copyright (C) 2017-2018, Red Hat, Inc.
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
#ifndef __GTK_GESTURE_STYLUS_H__
#define __GTK_GESTURE_STYLUS_H__

#include <gtk/gtkgesture.h>

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_STYLUS         (gtk_gesture_stylus_get_type ())
GDK_AVAILABLE_IN_ALL
GDK_DECLARE_EXPORTED_TYPE (GtkGestureStylus, gtk_gesture_stylus, GTK, GESTURE_STYLUS)

GDK_AVAILABLE_IN_ALL
GtkGesture *      gtk_gesture_stylus_new      (void);

GDK_AVAILABLE_IN_ALL
gboolean          gtk_gesture_stylus_get_axis (GtkGestureStylus *gesture,
					       GdkAxisUse        axis,
					       gdouble          *value);
GDK_AVAILABLE_IN_ALL
gboolean          gtk_gesture_stylus_get_axes (GtkGestureStylus  *gesture,
					       GdkAxisUse         axes[],
					       gdouble          **values);
GDK_AVAILABLE_IN_ALL
gboolean          gtk_gesture_stylus_get_backlog (GtkGestureStylus  *gesture,
						  GdkTimeCoord     **backlog,
						  guint             *n_elems);
GDK_AVAILABLE_IN_ALL
GdkDeviceTool *   gtk_gesture_stylus_get_device_tool (GtkGestureStylus *gesture);

G_END_DECLS

#endif /* __GTK_GESTURE_STYLUS_H__ */
