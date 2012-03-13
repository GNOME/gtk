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

#ifndef __GTK_GESTURE_RECOGNIZER_H__
#define __GTK_GESTURE_RECOGNIZER_H__

#include <gtk/gtkeventrecognizer.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE_RECOGNIZER           (gtk_gesture_recognizer_get_type ())
#define GTK_GESTURE_RECOGNIZER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_GESTURE_RECOGNIZER, GtkGestureRecognizer))
#define GTK_GESTURE_RECOGNIZER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_GESTURE_RECOGNIZER, GtkGestureRecognizerClass))
#define GTK_IS_GESTURE_RECOGNIZER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_GESTURE_RECOGNIZER))
#define GTK_IS_GESTURE_RECOGNIZER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_GESTURE_RECOGNIZER))
#define GTK_GESTURE_RECOGNIZER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GESTURE_RECOGNIZER, GtkGestureRecognizerClass))

typedef struct _GtkGestureRecognizer             GtkGestureRecognizer;
typedef struct _GtkGestureRecognizerClass        GtkGestureRecognizerClass;
typedef struct _GtkGestureRecognizerPrivate      GtkGestureRecognizerPrivate;

struct _GtkGestureRecognizer
{
  GtkEventRecognizer parent;

  GtkGestureRecognizerPrivate *priv;
};

struct _GtkGestureRecognizerClass
{
  GtkEventRecognizerClass parent_class;
};

GType                 gtk_gesture_recognizer_get_type               (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GTK_GESTURE_RECOGNIZER_H__ */
