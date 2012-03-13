/*
 * Copyright © 2012 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_PINCH_PAN_GESTURE_H__
#define __GTK_PINCH_PAN_GESTURE_H__

#include <gtk/gtkgesture.h>

G_BEGIN_DECLS

#define GTK_TYPE_PINCH_PAN_GESTURE           (gtk_pinch_pan_gesture_get_type ())
#define GTK_PINCH_PAN_GESTURE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_PINCH_PAN_GESTURE, GtkPinchPanGesture))
#define GTK_PINCH_PAN_GESTURE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_PINCH_PAN_GESTURE, GtkPinchPanGestureClass))
#define GTK_IS_PINCH_PAN_GESTURE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_PINCH_PAN_GESTURE))
#define GTK_IS_PINCH_PAN_GESTURE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_PINCH_PAN_GESTURE))
#define GTK_PINCH_PAN_GESTURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PINCH_PAN_GESTURE, GtkPinchPanGestureClass))

typedef struct _GtkPinchPanGesture           GtkPinchPanGesture;
typedef struct _GtkPinchPanGestureClass      GtkPinchPanGestureClass;
typedef struct _GtkPinchPanGesturePrivate    GtkPinchPanGesturePrivate;

struct _GtkPinchPanGesture
{
  GtkGesture parent;

  GtkPinchPanGesturePrivate *priv;
};

struct _GtkPinchPanGestureClass
{
  GtkGestureClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

GType                 gtk_pinch_pan_gesture_get_type      (void) G_GNUC_CONST;

double                gtk_pinch_pan_gesture_get_rotation  (GtkPinchPanGesture   *gesture);
double                gtk_pinch_pan_gesture_get_zoom      (GtkPinchPanGesture   *gesture);
double                gtk_pinch_pan_gesture_get_x_offset  (GtkPinchPanGesture   *gesture);
double                gtk_pinch_pan_gesture_get_y_offset  (GtkPinchPanGesture   *gesture);

#include <gdk/gdk.h>

gboolean              _gtk_pinch_pan_gesture_begin        (GtkPinchPanGesture   *gesture,
                                                           GdkEvent             *event);
gboolean              _gtk_pinch_pan_gesture_update       (GtkPinchPanGesture   *gesture,
                                                           GdkEvent             *event);
gboolean              _gtk_pinch_pan_gesture_end          (GtkPinchPanGesture   *gesture,
                                                           GdkEvent             *event);
gboolean              _gtk_pinch_pan_gesture_cancel       (GtkPinchPanGesture   *gesture,
                                                           GdkEvent             *event);

G_END_DECLS

#endif /* __GTK_PINCH_PAN_GESTURE_H__ */
