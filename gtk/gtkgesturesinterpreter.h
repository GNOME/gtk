/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_GESTURES_INTERPRETER_H__
#define __GTK_GESTURES_INTERPRETER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_GESTURE (gtk_gesture_get_type ())
#define GTK_TYPE_GESTURE_STROKE (gtk_gesture_stroke_get_type ())

#define GTK_TYPE_GESTURES_INTERPRETER         (gtk_gestures_interpreter_get_type ())
#define GTK_GESTURES_INTERPRETER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_GESTURES_INTERPRETER, GtkGesturesInterpreter))
#define GTK_GESTURES_INTERPRETER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_GESTURES_INTERPRETER, GtkGesturesInterpreterClass))
#define GTK_IS_GESTURES_INTERPRETER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_GESTURES_INTERPRETER))
#define GTK_IS_GESTURES_INTERPRETER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_GESTURES_INTERPRETER))
#define GTK_GESTURES_INTERPRETER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_GESTURES_INTERPRETER, GtkGesturesInterpreterClass))

typedef struct _GtkGestureStroke GtkGestureStroke;
typedef struct _GtkGesture GtkGesture;

typedef struct _GtkGesturesInterpreter GtkGesturesInterpreter;
typedef struct _GtkGesturesInterpreterClass GtkGesturesInterpreterClass;

struct _GtkGesturesInterpreter
{
  GObject parent_object;
  gpointer priv;
};

struct _GtkGesturesInterpreterClass
{
  GObjectClass parent_class;

  void (* events_vectorized) (GtkGesturesInterpreter *interpreter,
                              GtkGesture             *gesture);

  void (* gesture_detected)  (GtkGesturesInterpreter *interpreter,
                              guint                   gesture_id,
                              gdouble                 confidence);
};

/* Gesture stroke */
GType              gtk_gesture_stroke_get_type      (void) G_GNUC_CONST;
GtkGestureStroke * gtk_gesture_stroke_new           (void);
GtkGestureStroke * gtk_gesture_stroke_copy          (const GtkGestureStroke *stroke);
void               gtk_gesture_stroke_free          (GtkGestureStroke *stroke);
void               gtk_gesture_stroke_append_vector (GtkGestureStroke *stroke,
						     gdouble           angle,
						     guint             length);

guint              gtk_gesture_stroke_get_n_vectors (const GtkGestureStroke *stroke);
gboolean           gtk_gesture_stroke_get_vector    (const GtkGestureStroke *stroke,
                                                     guint                   n_vector,
                                                     gdouble                *angle,
                                                     guint                  *length,
                                                     gdouble                *relative_length);
/* Gesture */
GType        gtk_gesture_get_type   (void) G_GNUC_CONST;
GtkGesture * gtk_gesture_new        (const GtkGestureStroke *stroke,
                                     GtkGestureFlags         flags);
void         gtk_gesture_add_stroke (GtkGesture             *gesture,
				     const GtkGestureStroke *stroke,
				     gint                    dx,
				     gint                    dy);
GtkGesture * gtk_gesture_copy       (const GtkGesture       *gesture);
void         gtk_gesture_free       (GtkGesture             *gesture);

GtkGestureFlags          gtk_gesture_get_flags     (const GtkGesture *gesture);
guint                    gtk_gesture_get_n_strokes (const GtkGesture *gesture);
const GtkGestureStroke * gtk_gesture_get_stroke    (const GtkGesture *gesture,
                                                    guint             n_stroke,
                                                    gint             *dx,
                                                    gint             *dy);

guint              gtk_gesture_register        (const GtkGesture *gesture);
guint              gtk_gesture_register_static (const GtkGesture *gesture);
const GtkGesture * gtk_gesture_lookup          (guint             gesture_id);

/* Gestures interpreter */
GType gtk_gestures_interpreter_get_type (void) G_GNUC_CONST;

GtkGesturesInterpreter * gtk_gestures_interpreter_new (void);

gboolean gtk_gestures_interpreter_add_gesture    (GtkGesturesInterpreter *interpreter,
                                                  guint                   gesture_id);
void     gtk_gestures_interpreter_remove_gesture (GtkGesturesInterpreter *interpreter,
                                                  guint                   gesture_id);

gboolean gtk_gestures_interpreter_feed_event (GtkGesturesInterpreter *interpreter,
					      GdkEvent               *event);
gboolean gtk_gestures_interpreter_finish     (GtkGesturesInterpreter *interpreter,
                                              guint                  *gesture_id);

G_END_DECLS

#endif /* __GTK_GESTURES_INTERPRETER_H__ */
