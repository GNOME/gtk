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

#ifndef __GTK_GESTURE_H__
#define __GTK_GESTURE_H__

#include <gtk/gtkeventtracker.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE           (gtk_gesture_get_type ())
#define GTK_GESTURE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_GESTURE, GtkGesture))
#define GTK_GESTURE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_GESTURE, GtkGestureClass))
#define GTK_IS_GESTURE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_GESTURE))
#define GTK_IS_GESTURE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_GESTURE))
#define GTK_GESTURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GESTURE, GtkGestureClass))

typedef struct _GtkGesture           GtkGesture;
typedef struct _GtkGestureClass      GtkGestureClass;
typedef struct _GtkGesturePrivate    GtkGesturePrivate;

struct _GtkGesture
{
  GtkEventTracker parent;

  GtkGesturePrivate *priv;
};

struct _GtkGestureClass
{
  GtkEventTrackerClass parent_class;

  void                (* sequence_given)            (GtkGesture *       gesture,
                                                     GdkEventSequence * sequence);
  void                (* sequence_stolen)           (GtkGesture *       gesture,
                                                     GdkEventSequence * sequence,
                                                     GtkGesture *       accepter);

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

GType                 gtk_gesture_get_type          (void) G_GNUC_CONST;

void                  gtk_gesture_add_sequence      (GtkGesture        *gesture,
                                                     GdkEventSequence  *sequence);
void                  gtk_gesture_remove_sequence   (GtkGesture        *gesture,
                                                     GdkEventSequence  *sequence);
gboolean              gtk_gesture_has_sequence      (GtkGesture        *gesture,
                                                     GdkEventSequence  *sequence);
gboolean              gtk_gesture_owns_sequence     (GtkGesture        *gesture,
                                                     GdkEventSequence  *sequence);
gboolean              gtk_gesture_owns_all_sequences(GtkGesture        *gesture);

void                  gtk_gesture_accept            (GtkGesture        *gesture);
gboolean              gtk_gesture_is_accepted       (GtkGesture        *gesture);

G_END_DECLS

#endif /* __GTK_GESTURE_H__ */
