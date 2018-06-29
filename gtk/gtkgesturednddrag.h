/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */
#ifndef __GTK_GESTURE_DND_DRAG_H__
#define __GTK_GESTURE_DND_DRAG_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_DND_DRAG         (gtk_gesture_dnd_drag_get_type ())
#define GTK_GESTURE_DND_DRAG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_GESTURE_DND_DRAG, GtkGestureDndDrag))
#define GTK_GESTURE_DND_DRAG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_GESTURE_DND_DRAG, GtkGestureDndDragClass))
#define GTK_IS_GESTURE_DND_DRAG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_GESTURE_DND_DRAG))
#define GTK_IS_GESTURE_DND_DRAG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_GESTURE_DND_DRAG))
#define GTK_GESTURE_DND_DRAG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_GESTURE_DND_DRAG, GtkGestureDndDragClass))

typedef struct _GtkGestureDndDrag GtkGestureDndDrag;
typedef struct _GtkGestureDndDragClass GtkGestureDndDragClass;

GDK_AVAILABLE_IN_ALL
GType                   gtk_gesture_dnd_drag_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkGesture *            gtk_gesture_dnd_drag_new                (void);

GDK_AVAILABLE_IN_ALL
GdkDragAction           gtk_gesture_dnd_drag_get_actions        (GtkGestureDndDrag      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_gesture_dnd_drag_set_actions        (GtkGestureDndDrag      *self,
                                                                 GdkDragAction           actions);
GDK_AVAILABLE_IN_ALL
GdkContentProvider *    gtk_gesture_dnd_drag_get_content        (GtkGestureDndDrag      *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_gesture_dnd_drag_set_content        (GtkGestureDndDrag      *self,
                                                                 GdkContentProvider     *content);

GDK_AVAILABLE_IN_ALL
gboolean                gtk_gesture_dnd_drag_get_start_point    (GtkGestureDndDrag      *self,
                                                                 gdouble                *x,
                                                                 gdouble                *y);
GDK_AVAILABLE_IN_ALL
GdkDragContext *        gtk_gesture_dnd_drag_get_drag           (GtkGestureDndDrag      *self);

G_END_DECLS

#endif /* __GTK_GESTURE_DND_DRAG_H__ */
