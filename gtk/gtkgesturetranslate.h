/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
 * Copyright (C) 2019, Yariv Barkan
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
#ifndef __GTK_GESTURE_TRANSLATE_H__
#define __GTK_GESTURE_TRANSLATE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkgesture.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_TRANSLATE         (gtk_gesture_translate_get_type ())
#define GTK_GESTURE_TRANSLATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_GESTURE_TRANSLATE, GtkGestureTranslate))
#define GTK_GESTURE_TRANSLATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_GESTURE_TRANSLATE, GtkGestureTranslateClass))
#define GTK_IS_GESTURE_TRANSLATE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_GESTURE_TRANSLATE))
#define GTK_IS_GESTURE_TRANSLATE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_GESTURE_TRANSLATE))
#define GTK_GESTURE_TRANSLATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_GESTURE_TRANSLATE, GtkGestureTranslateClass))

typedef struct _GtkGestureTranslate GtkGestureTranslate;
typedef struct _GtkGestureTranslateClass GtkGestureTranslateClass;

GDK_AVAILABLE_IN_ALL
GType        gtk_gesture_translate_get_type   (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkGesture * gtk_gesture_translate_new        (void);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_gesture_translate_get_start  (GtkGestureTranslate *gesture,
                                               gdouble             *x,
                                               gdouble             *y);

GDK_AVAILABLE_IN_ALL
gboolean     gtk_gesture_translate_get_offset (GtkGestureTranslate *gesture,
                                               gdouble             *x_offset,
                                               gdouble             *y_offset);

G_END_DECLS

#endif /* __GTK_GESTURE_TRANSLATE_H__ */
