/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Carlos Garnacho <carlos@lanedo.com>
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

#include "config.h"
#include "gtkgesturesinterpreter.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include <gdk/gdk.h>
#include <math.h>

#define N_CIRCULAR_SIDES 12
#define VECTORIZATION_ANGLE_THRESHOLD (G_PI_2 / 10)
#define MINIMUM_CONFIDENCE_ALLOWED 0.8

G_DEFINE_TYPE (GtkGesturesInterpreter, gtk_gestures_interpreter, G_TYPE_OBJECT)
G_DEFINE_BOXED_TYPE (GtkGestureStroke, gtk_gesture_stroke,
                     gtk_gesture_stroke_copy, gtk_gesture_stroke_free)
G_DEFINE_BOXED_TYPE (GtkGesture, gtk_gesture,
                     gtk_gesture_copy, gtk_gesture_free)

typedef struct _GtkGesturesInterpreterPrivate GtkGesturesInterpreterPrivate;
typedef struct _StrokeVector StrokeVector;
typedef struct _RecordedGesture RecordedGesture;

struct _GtkGesturesInterpreterPrivate
{
  GHashTable *events;
  GArray *handled_gestures;
};

struct _StrokeVector
{
  gdouble angle;
  guint16 length;
};

struct _GtkGestureStroke
{
  GArray *gesture_data;
  guint total_length;
  gint dx;
  gint dy;
};

struct _GtkGesture
{
  GPtrArray *strokes;
  GtkGestureFlags flags;
};

struct _RecordedGesture
{
  GArray *coordinates;
  gint min_x;
  gint max_x;
  gint min_y;
  gint max_y;
};

enum {
  EVENTS_VECTORIZED,
  GESTURE_DETECTED,
  LAST_SIGNAL
};

static GHashTable *registered_gestures = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

static void
initialize_gestures_ht (void)
{
  GtkGestureStroke *stroke;
  GtkGesture *gesture;
  gdouble angle;
  guint id, n;

  if (registered_gestures)
    return;

  registered_gestures = g_hash_table_new (NULL, NULL);

  /* Swipe right */
  stroke = gtk_gesture_stroke_new ();
  gtk_gesture_stroke_append_vector (stroke, G_PI / 2, 100);
  gesture = gtk_gesture_new (stroke, 0);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_SWIPE_RIGHT);
  gtk_gesture_stroke_free (stroke);

  /* Swipe left */
  stroke = gtk_gesture_stroke_new ();
  gtk_gesture_stroke_append_vector (stroke, 3 * G_PI / 2, 100);
  gesture = gtk_gesture_new (stroke, 0);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_SWIPE_LEFT);
  gtk_gesture_stroke_free (stroke);

  /* Swipe up */
  stroke = gtk_gesture_stroke_new ();
  gtk_gesture_stroke_append_vector (stroke, 0, 100);
  gesture = gtk_gesture_new (stroke, 0);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_SWIPE_UP);
  gtk_gesture_stroke_free (stroke);

  /* Swipe down */
  stroke = gtk_gesture_stroke_new ();
  gtk_gesture_stroke_append_vector (stroke, G_PI, 100);
  gesture = gtk_gesture_new (stroke, 0);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_SWIPE_DOWN);
  gtk_gesture_stroke_free (stroke);

  /* Circular clockwise */
  stroke = gtk_gesture_stroke_new ();

  for (n = 0; n < N_CIRCULAR_SIDES; n++)
    {
      angle = 2 * G_PI * ((gdouble) n / N_CIRCULAR_SIDES);
      gtk_gesture_stroke_append_vector (stroke, angle, 50);
    }

  gesture = gtk_gesture_new (stroke,
                             GTK_GESTURE_FLAG_IGNORE_INITIAL_ORIENTATION);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_CIRCULAR_CLOCKWISE);
  gtk_gesture_stroke_free (stroke);

  /* Circular counterclockwise */
  stroke = gtk_gesture_stroke_new ();

  for (n = 0; n < N_CIRCULAR_SIDES; n++)
    {
      angle = 2 * G_PI * ((gdouble) (N_CIRCULAR_SIDES - n) / N_CIRCULAR_SIDES);
      gtk_gesture_stroke_append_vector (stroke, angle, 50);
    }

  gesture = gtk_gesture_new (stroke,
                             GTK_GESTURE_FLAG_IGNORE_INITIAL_ORIENTATION);
  id = gtk_gesture_register_static (gesture);
  g_assert (id == GTK_GESTURE_CIRCULAR_COUNTERCLOCKWISE);
  gtk_gesture_stroke_free (stroke);
}

static void
gtk_gestures_interpreter_finalize (GObject *object)
{
  GtkGesturesInterpreterPrivate *priv;

  priv = GTK_GESTURES_INTERPRETER (object)->priv;

  g_hash_table_destroy (priv->events);
  g_array_free (priv->handled_gestures, TRUE);

  G_OBJECT_CLASS (gtk_gestures_interpreter_parent_class)->finalize (object);
}

static void
gtk_gestures_interpreter_class_init (GtkGesturesInterpreterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_gestures_interpreter_finalize;

  initialize_gestures_ht ();

  signals[EVENTS_VECTORIZED] =
    g_signal_new (I_("events-vectorized"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGesturesInterpreterClass,
				   events_vectorized),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, GTK_TYPE_GESTURE);
  signals[GESTURE_DETECTED] =
    g_signal_new (I_("gesture-detected"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGesturesInterpreterClass,
				   gesture_detected),
                  NULL, NULL,
                  _gtk_marshal_VOID__UINT_DOUBLE,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT, G_TYPE_DOUBLE);

  g_type_class_add_private (object_class, sizeof (GtkGesturesInterpreterPrivate));
}

static RecordedGesture *
recorded_gesture_new (void)
{
  RecordedGesture *recorded;

  recorded = g_slice_new0 (RecordedGesture);
  recorded->coordinates = g_array_new (FALSE, FALSE, sizeof (GdkPoint));
  recorded->min_x = G_MAXINT;
  recorded->max_x = G_MININT;
  recorded->min_y = G_MAXINT;
  recorded->max_y = G_MININT;

  return recorded;
}

static void
recorded_gesture_free (RecordedGesture *recorded)
{
  g_array_free (recorded->coordinates, TRUE);
  g_slice_free (RecordedGesture, recorded);
}

static void
recorded_gesture_append_coordinate (RecordedGesture *recorded,
                                    gint             x,
                                    gint             y)
{
  GdkPoint point, *prev = NULL;

  if (recorded->coordinates->len > 0)
    prev = &g_array_index (recorded->coordinates, GdkPoint,
			   recorded->coordinates->len - 1);

  if (prev &&
      prev->x == x &&
      prev->y == y)
    return;

  point.x = x;
  point.y = y;

  g_array_append_val (recorded->coordinates, point);

  recorded->min_x = MIN (recorded->min_x, x);
  recorded->max_x = MAX (recorded->max_x, x);
  recorded->min_y = MIN (recorded->min_y, y);
  recorded->max_y = MAX (recorded->max_y, y);
}

static void
gtk_gestures_interpreter_init (GtkGesturesInterpreter *interpreter)
{
  GtkGesturesInterpreterPrivate *priv;

  priv = interpreter->priv = G_TYPE_INSTANCE_GET_PRIVATE (interpreter,
                                                          GTK_TYPE_GESTURES_INTERPRETER,
                                                          GtkGesturesInterpreterPrivate);
  priv->events = g_hash_table_new_full (NULL, NULL, NULL,
                                        (GDestroyNotify) recorded_gesture_free);
  priv->handled_gestures = g_array_new (FALSE, FALSE, sizeof (guint));
}

/* Gesture stroke */
GtkGestureStroke *
gtk_gesture_stroke_new (void)
{
  GtkGestureStroke *stroke;

  stroke = g_slice_new0 (GtkGestureStroke);
  stroke->gesture_data = g_array_new (FALSE, FALSE,
                                      sizeof (StrokeVector));
  return stroke;
}

GtkGestureStroke *
gtk_gesture_stroke_copy (const GtkGestureStroke *stroke)
{
  GtkGestureStroke *copy;

  copy = gtk_gesture_stroke_new ();
  g_array_append_vals (copy->gesture_data,
                       stroke->gesture_data->data,
                       stroke->gesture_data->len);
  copy->total_length = stroke->total_length;
  copy->dx = stroke->dx;
  copy->dy = stroke->dy;

  return copy;
}

void
gtk_gesture_stroke_free (GtkGestureStroke *stroke)
{
  g_return_if_fail (stroke != NULL);

  g_array_free (stroke->gesture_data, TRUE);
  g_slice_free (GtkGestureStroke, stroke);
}

void
gtk_gesture_stroke_append_vector (GtkGestureStroke *stroke,
                                  gdouble           angle,
                                  guint             length)
{
  StrokeVector vector;

  g_return_if_fail (stroke != NULL);

  vector.angle = fmod (angle, 2 * G_PI);
  vector.length = length;
  g_array_append_val (stroke->gesture_data, vector);

  stroke->total_length += length;
}

guint
gtk_gesture_stroke_get_n_vectors (const GtkGestureStroke *stroke)
{
  g_return_val_if_fail (stroke != NULL, 0);

  return stroke->gesture_data->len;
}

gboolean
gtk_gesture_stroke_get_vector (const GtkGestureStroke *stroke,
                               guint                   n_vector,
                               gdouble                *angle,
                               guint                  *length,
                               gdouble                *relative_length)
{
  StrokeVector *vector;

  g_return_val_if_fail (stroke != NULL, FALSE);
  g_return_val_if_fail (n_vector < stroke->gesture_data->len, FALSE);

  vector = &g_array_index (stroke->gesture_data, StrokeVector, n_vector);

  if (angle)
    *angle = vector->angle;

  if (length)
    *length = vector->length;

  if (relative_length)
    *relative_length = (gdouble) vector->length / stroke->total_length;

  return TRUE;
}

/* Gesture */
GtkGesture *
gtk_gesture_new (const GtkGestureStroke *stroke,
                 GtkGestureFlags         flags)
{
  GtkGesture *gesture;

  g_return_val_if_fail (stroke != NULL, NULL);
  g_return_val_if_fail (stroke->gesture_data->len > 0, NULL);

  gesture = g_slice_new0 (GtkGesture);
  gesture->flags = flags;
  gesture->strokes =
    g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_gesture_stroke_free);

  g_ptr_array_add (gesture->strokes,
                   gtk_gesture_stroke_copy (stroke));
  return gesture;
}

void
gtk_gesture_add_stroke (GtkGesture             *gesture,
                        const GtkGestureStroke *stroke,
                        gint                    dx,
                        gint                    dy)
{
  GtkGestureStroke *copy;

  g_return_if_fail (gesture != NULL);
  g_return_if_fail (stroke != NULL);
  g_return_if_fail (stroke->gesture_data->len > 0);

  copy = gtk_gesture_stroke_copy (stroke);
  copy->dx = dx;
  copy->dy = dy;

  g_ptr_array_add (gesture->strokes, copy);
}

GtkGesture *
gtk_gesture_copy (const GtkGesture *gesture)
{
  GtkGesture *copy;
  guint i;

  g_return_val_if_fail (gesture != NULL, NULL);

  copy = g_slice_new0 (GtkGesture);
  copy->flags = gesture->flags;
  copy->strokes =
    g_ptr_array_new_with_free_func ((GDestroyNotify) gtk_gesture_stroke_free);

  for (i = 0; i < gesture->strokes->len; i++)
    {
      GtkGestureStroke *stroke;

      stroke = g_ptr_array_index (gesture->strokes, i);
      g_ptr_array_add (copy->strokes,
                       gtk_gesture_stroke_copy (stroke));
    }

  return copy;
}

void
gtk_gesture_free (GtkGesture *gesture)
{
  g_return_if_fail (gesture != NULL);

  g_ptr_array_free (gesture->strokes, TRUE);
  g_slice_free (GtkGesture, gesture);
}

GtkGestureFlags
gtk_gesture_get_flags (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gesture->flags;
}

guint
gtk_gesture_get_n_strokes (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gesture->strokes->len;
}

const GtkGestureStroke *
gtk_gesture_get_stroke (const GtkGesture *gesture,
                        guint             n_stroke,
                        gint             *dx,
                        gint             *dy)
{
  GtkGestureStroke *stroke;

  g_return_val_if_fail (gesture != NULL, NULL);
  g_return_val_if_fail (n_stroke < gesture->strokes->len, NULL);

  stroke = g_ptr_array_index (gesture->strokes, n_stroke);

  if (stroke)
    {
      if (dx)
        *dx = stroke->dx;

      if (dy)
        *dy = stroke->dy;
    }

  return stroke;
}

guint
gtk_gesture_register (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gtk_gesture_register_static (gtk_gesture_copy (gesture));
}

guint
gtk_gesture_register_static (const GtkGesture *gesture)
{
  static guint gesture_id = 0;

  g_return_val_if_fail (gesture != NULL, 0);

  initialize_gestures_ht ();
  gesture_id++;

  g_hash_table_insert (registered_gestures,
                       GUINT_TO_POINTER (gesture_id),
                       (gpointer) gesture);
  return gesture_id;
}

const GtkGesture *
gtk_gesture_lookup (guint gesture_id)
{
  if (!registered_gestures)
    return NULL;

  return g_hash_table_lookup (registered_gestures,
                              GUINT_TO_POINTER (gesture_id));
}

/* Gesture interpreter */
GtkGesturesInterpreter *
gtk_gestures_interpreter_new (void)
{
  return g_object_new (GTK_TYPE_GESTURES_INTERPRETER, NULL);
}

gboolean
gtk_gestures_interpreter_add_gesture (GtkGesturesInterpreter *interpreter,
                                      guint                   gesture_id)
{
  GtkGesturesInterpreterPrivate *priv;
  guint i;

  g_return_val_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter), FALSE);
  g_return_val_if_fail (gtk_gesture_lookup (gesture_id) != NULL, FALSE);

  priv = interpreter->priv;

  for (i = 0; i < priv->handled_gestures->len; i++)
    {
      if (gesture_id == g_array_index (priv->handled_gestures, guint, i))
        return FALSE;
    }

  g_array_append_val (priv->handled_gestures, gesture_id);

  return TRUE;
}

void
gtk_gestures_interpreter_remove_gesture (GtkGesturesInterpreter *interpreter,
                                         guint                   gesture_id)
{
  GtkGesturesInterpreterPrivate *priv;
  guint i;

  g_return_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter));
  g_return_if_fail (gtk_gesture_lookup (gesture_id) != NULL);

  priv = interpreter->priv;

  for (i = 0; i < priv->handled_gestures->len; i++)
    {
      guint handled_gesture_id;

      handled_gesture_id = g_array_index (priv->handled_gestures, guint, i);

      if (handled_gesture_id == gesture_id)
        {
          g_array_remove_index_fast (priv->handled_gestures, i);
          break;
        }
    }
}

gboolean
gtk_gestures_interpreter_feed_event (GtkGesturesInterpreter *interpreter,
				     GdkEvent               *event)
{
  GtkGesturesInterpreterPrivate *priv;
  RecordedGesture *recorded;
  guint touch_id;
  gdouble x, y;

  g_return_val_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  priv = interpreter->priv;

  if (!gdk_event_get_source_device (event))
    return FALSE;

  if (!gdk_event_get_coords (event, &x, &y))
    return FALSE;

  if (!gdk_event_get_touch_id (event, &touch_id))
    touch_id = 0;

  recorded = g_hash_table_lookup (priv->events,
                                  GUINT_TO_POINTER (touch_id));
  if (!recorded)
    {
      recorded = recorded_gesture_new ();
      g_hash_table_insert (priv->events,
                           GUINT_TO_POINTER (touch_id),
                           recorded);
    }

  recorded_gesture_append_coordinate (recorded, x, y);

  return TRUE;
}

gboolean
gtk_gestures_interpreter_finish (GtkGesturesInterpreter *interpreter,
                                 guint                  *gesture_id)
{
  g_return_val_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter), FALSE);

  return TRUE;
}
