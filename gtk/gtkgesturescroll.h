/* GTK - The GIMP Toolkit
 * Copyright (C) 202, Purism SPC
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
 * Author(s): Alexander Mikhaylenko <alexm@gnome.org>
 */
#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtkgesturesingle.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_SCROLL         (gtk_gesture_scroll_get_type ())
#define GTK_GESTURE_SCROLL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_GESTURE_SCROLL, GtkGestureScroll))
#define GTK_GESTURE_SCROLL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_GESTURE_SCROLL, GtkGestureScrollClass))
#define GTK_IS_GESTURE_SCROLL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_GESTURE_SCROLL))
#define GTK_IS_GESTURE_SCROLL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_GESTURE_SCROLL))
#define GTK_GESTURE_SCROLL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_GESTURE_SCROLL, GtkGestureScrollClass))

typedef struct _GtkGestureScroll GtkGestureScroll;
typedef struct _GtkGestureScrollClass GtkGestureScrollClass;

GDK_AVAILABLE_IN_ALL
GType        gtk_gesture_scroll_get_type          (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkGesture * gtk_gesture_scroll_new               (void);

G_END_DECLS
