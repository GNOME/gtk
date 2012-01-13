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

/**
 * SECTION:gtkgesturesinterpreter
 * @Short_description: Gestures interpreter
 * @Title: GtkGesturesInterpreter
 * @See_also: #GdkEventMultitouch
 *
 * #GtkGesturesInterpreter handles interpretation of input events to check
 * whether they resemble a handled gesture. If you are using #GtkWidget<!-- -->s
 * and want only to enable events for devices with source %GDK_SOURCE_TOUCH,
 * you can use gtk_widget_enable_gesture() and the #GtkWidget::gesture signal
 * without the need of creating a #GtkGesturesInterpreter.
 *
 * A #GtkGesturesInterpreter may be told to handle a gesture through
 * gtk_gestures_interpreter_add_gesture(), either using a gesture provided
 * by the GtkGestureType enum, or creating and registering a gesture through
 * gtk_gesture_register() or gtk_gesture_register_static().
 *
 * The #GtkGestureInterpreter can be told to interpret user input events
 * through gtk_gestures_interpreter_feed_event(), the event is required
 * to provide coordinates in order to be handled.
 *
 * The recognized gesture may be requested through gtk_gestures_interpreter_finish(),
 * if the gesture drafted by the input events resembles well enough a handled
 * gesture, this function will provide the gesture ID that was recognized.
 */

#define N_CIRCULAR_SIDES 12
#define VECTORIZATION_ANGLE_THRESHOLD (G_PI_2 / 10)
#define MINIMUM_CONFIDENCE_ALLOWED 0.78
#define INITIAL_COORDINATE_THRESHOLD 0.3

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

  guint finished : 1;
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

  /**
   * GtkGesturesInterpreter::events-vectorized
   * @interpreter: the object which received the signal
   * @gesture: the #GtkGesture holding the user gesture
   *
   * This signal is emitted during gtk_gestures_interpreter_finish()
   * after the events introduced through gtk_gestures_interpreter_feed_event()
   * are vectorized and transformed into a #GtkGesture.
   */
  signals[EVENTS_VECTORIZED] =
    g_signal_new (I_("events-vectorized"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGesturesInterpreterClass,
				   events_vectorized),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, GTK_TYPE_GESTURE);

  /**
   * GtkGesturesInterpreter::gesture-detected
   * @interpreter: the object which received the signal
   * @gesture_id: the gesture ID of the recognized gesture
   * @confidence: [0..1] measuring the level of confidence on the recognition.
   *
   * This signal is emitted when the #GtkGesturesInterpreter
   * recognizes a gesture out of the events introduced through
   * gtk_gestures_interpreter_feed_event().
   */
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
find_angle_and_distance (GdkPoint *point0,
                         GdkPoint *point1,
                         gdouble  *angle,
                         guint    *distance)
{
  gdouble ang;
  gint x, y;

  x = point1->x - point0->x;
  y = point1->y - point0->y;

  ang = (atan2 (x, y) + G_PI);
  ang = fmod (ang, 2 * G_PI);
  *angle = (2 * G_PI) - ang;

  *distance = (guint) sqrt ((x * x) + (y * y));

  if (*distance == 0)
    *angle = 0;
}

static GtkGestureStroke *
recorded_gesture_vectorize (RecordedGesture *recorded)
{
  GtkGestureStroke *stroke;
  GdkPoint *point, *prev = NULL;
  gdouble acc_angle = -1;
  gint i, diff, len;

  if (recorded->coordinates->len == 0)
    return NULL;

  stroke = gtk_gesture_stroke_new ();

  if (recorded->coordinates->len == 1)
    {
      /* Append a single 0-distance vector */
      gtk_gesture_stroke_append_vector (stroke, 0, 0);
      return stroke;
    }

  diff = ((recorded->max_x - recorded->min_x) - (recorded->max_y - recorded->min_y));

  if (diff > 0)
    {
      /* Sample is more wide than high */
      recorded->min_y -= diff / 2;
      recorded->max_y += diff / 2;
    }
  else
    {
      /* Sample is more high than wide */
      recorded->min_x -= ABS (diff) / 2;
      recorded->max_x += ABS (diff) / 2;
    }

  if (recorded->coordinates->len == 1)
    {
      /* Append a single 0-distance vector */
      gtk_gesture_stroke_append_vector (stroke, 0, 0);
      return stroke;
    }

  for (i = 0; i < recorded->coordinates->len; i++)
    {
      gdouble angle;
      guint distance;

      point = &g_array_index (recorded->coordinates, GdkPoint, i);

      if (!prev)
	{
	  prev = point;
	  len = 0;
	  continue;
	}

      find_angle_and_distance (prev, point, &angle, &distance);
      len++;

      if (acc_angle < 0)
	acc_angle = angle;

      if (i == recorded->coordinates->len - 1 ||
	  angle > acc_angle + VECTORIZATION_ANGLE_THRESHOLD ||
	  angle < acc_angle - VECTORIZATION_ANGLE_THRESHOLD)
	{
	  /* The last coordinate is falling out of the
	   * allowed threshold around the accumulated
	   * angle, save this point and keep calculating
	   * from here.
	   */
          gtk_gesture_stroke_append_vector (stroke, angle, distance);

	  prev = point;
	  acc_angle = -1;
	  len = 0;
	}
      else
	{
	  /* The weight of the accumulated angle is
	   * directly proportional to the distance
	   * to prev, so initial angle skews are
	   * more forgivable than stretched lines.
	   */
	  acc_angle = (len * acc_angle + angle) / (len + 1);
	}
    }

  return stroke;
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

/**
 * gtk_gesture_stroke_new:
 *
 * Creates a new, empty stroke to be used in a gesture.
 *
 * Returns: (transfer full): a newly created #GtkGestureStroke
 *
 * Since: 3.4
 **/
GtkGestureStroke *
gtk_gesture_stroke_new (void)
{
  GtkGestureStroke *stroke;

  stroke = g_slice_new0 (GtkGestureStroke);
  stroke->gesture_data = g_array_new (FALSE, FALSE,
                                      sizeof (StrokeVector));
  return stroke;
}

/**
 * gtk_gesture_stroke_copy:
 * @stroke: a #GtkGestureStroke
 *
 * Copies a #GtkGestureStroke
 *
 * Returns: (transfer full): A copy of @stroke
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_stroke_free:
 * @stroke: a #GtkGestureStroke
 *
 * Frees a #GtkGestureStroke and all its contained data
 *
 * Since: 3.4
 **/
void
gtk_gesture_stroke_free (GtkGestureStroke *stroke)
{
  g_return_if_fail (stroke != NULL);

  g_array_free (stroke->gesture_data, TRUE);
  g_slice_free (GtkGestureStroke, stroke);
}

/**
 * gtk_gesture_stroke_append_vector:
 * @stroke: a #GtkGestureStroke
 * @angle: vector angle
 * @length: vector length
 *
 * Appends a vector to stroke.
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_stroke_get_n_vectors:
 * @stroke: a #GtkGestureStroke
 *
 * Returns the number of vectors that @stroke currently
 * contains.
 *
 * Returns: The number of vectors in @stroke
 *
 * Since: 3.4
 **/
guint
gtk_gesture_stroke_get_n_vectors (const GtkGestureStroke *stroke)
{
  g_return_val_if_fail (stroke != NULL, 0);

  return stroke->gesture_data->len;
}

/**
 * gtk_gesture_stroke_get_vector:
 * @stroke: a #GtkGestureStroke
 * @n_vector: number of vector to retrieve
 * @angle: (out) (allow-none): return location for the vector angle, or %NULL
 * @length: (out) (allow-none): return location for the vector distance, or %NULL
 * @relative_length: (out) (allow-none): return location for the relative vector
 *                   length within the stroke, or %NULL
 *
 * If @n_vector falls within the range of vectors contained in @stroke,
 * this function will return %TRUE and respectively fill in @angle and
 * @length with the vector direction angle and length. Optionally, if
 * @relative_length is not %NULL, the relative [0..1] length of the vector
 * within the whole stroke will be returned at that location.
 *
 * If @n_vector doesn't represent a vector of the stroke, %FALSE is
 * returned.
 *
 * Returns: %TRUE if @n_vector is a valid vector
 *
 * Since: 3.4
 **/
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
    {
      if (vector->length != 0)
        *relative_length = (gdouble) vector->length / stroke->total_length;
      else
        *relative_length = 0;
    }

  return TRUE;
}

/* Gesture */

/**
 * gtk_gesture_new:
 * @stroke: a #GtkGestureStroke
 * @flags: #GtkGestureFlags applying to the gesture
 *
 * Creates a new gesture containing @stroke as its first (or only)
 * stroke. Use gtk_gesture_add_stroke() to add create gestures with
 * more than one stroke.
 *
 * If @flags contains %GTK_GESTURE_FLAG_IGNORE_INITIAL_ORIENTATION,
 * the gesture will be loosely compared with respect to the initial
 * orientation of the gesture, it should be used whenever the
 * orientation isn't an important matching factor (for example,
 * circular gestures).
 *
 * Returns: (transfer full): A newly created #GtkGesture
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_add_stroke:
 * @gesture: a #GtkGesture
 * @stroke: a #GtkGestureStroke
 * @dx: X offset with respect to the first stroke
 * @dy: Y offset with respect to the first stroke
 *
 * Adds a further stroke to @gesture, @dx and @dy represent
 * the offset with the initial coordinates of the stroke
 * that was added through gtk_gesture_new().
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_copy:
 * @gesture: a #GtkGesture
 *
 * Copies a #GtkGesture
 *
 * Returns: (transfer full): a copy of @gesture. gtk_gesture_free() must be
 *          called on it when done.
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_free:
 * @gesture: a #GtkGesture
 *
 * Frees a #GtkGesture
 *
 * Since: 3.4
 **/
void
gtk_gesture_free (GtkGesture *gesture)
{
  g_return_if_fail (gesture != NULL);

  g_ptr_array_free (gesture->strokes, TRUE);
  g_slice_free (GtkGesture, gesture);
}

/**
 * gtk_gesture_get_flags:
 * @gesture: a #GtkGesture
 *
 * Returns the #GtkGestureFlags applying to @gesture
 *
 * Returns: the gesture flags
 *
 * Since: 3.4
 **/
GtkGestureFlags
gtk_gesture_get_flags (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gesture->flags;
}

/**
 * gtk_gesture_get_n_strokes:
 * @gesture: a #GtkGesture
 *
 * Returns the number of strokes that compose @gesture
 *
 * Returns: the number of strokes
 *
 * Since: 3.4
 **/
guint
gtk_gesture_get_n_strokes (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gesture->strokes->len;
}

/**
 * gtk_gesture_get_stroke:
 * @gesture: a #GtkGesture
 * @n_stroke: number of stroke to retrieve
 * @dx: (out) (allow-none): return location for the X offset, or %NULL
 * @dy: (out) (allow-none): return location for the Y offset, or %NULL
 *
 * if @n_stroke falls within the number of strokes that
 * compose @gesture, this function will return the
 * #GtkGestureStroke, and fill in @dx and @dy with the
 * offset with respect to the first stroke.
 *
 * Returns: (transfer none): the @GtkGestureStroke, or %NULL
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_register:
 * @gesture: a #GtkGesture
 *
 * Registers a gesture so it can be used in a #GtkGesturesInterpreter.
 * This function creates an internal copy of @gesture
 *
 * Returns: the ID of the just registered gesture
 *
 * Since: 3.4
 **/
guint
gtk_gesture_register (const GtkGesture *gesture)
{
  g_return_val_if_fail (gesture != NULL, 0);

  return gtk_gesture_register_static (gtk_gesture_copy (gesture));
}

/**
 * gtk_gesture_register_static:
 * @gesture: a #GtkGesture
 *
 * Registers a gesture so it can be used in a #GtkGesturesInterpreter.
 * This function assumes @gesture is statically stored, so doesn't
 * create a copy of it.
 *
 * Returns: the ID of the just registered gesture
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gesture_lookup:
 * @gesture_id: a gesture ID
 *
 * Returns the #GtkGesture corresponding to @gesture_id,
 * or %NULL if there is no gesture registered with such ID.
 *
 * Returns: (transfer none): the #GtkGesture, or %NULL
 *
 * Since: 3.4
 **/
const GtkGesture *
gtk_gesture_lookup (guint gesture_id)
{
  if (!registered_gestures)
    return NULL;

  return g_hash_table_lookup (registered_gestures,
                              GUINT_TO_POINTER (gesture_id));
}

/* Gesture interpreter */

/**
 * gtk_gestures_interpreter_new:
 *
 * Creates a new #GtkGesturesInterpreter
 *
 * Returns: (transfer full): the newly created #GtkGesturesInterpreter
 *
 * Since: 3.4
 **/
GtkGesturesInterpreter *
gtk_gestures_interpreter_new (void)
{
  return g_object_new (GTK_TYPE_GESTURES_INTERPRETER, NULL);
}

/**
 * gtk_gestures_interpreter_add_gesture:
 * @interpreter: a #GtkGesturesInterpreter
 * @gesture_id: gesture ID to enable in interpreter
 *
 * Tells @interpreter to handle @gesture_id. @gesture_id may be
 * either a custom #GtkGesture registered through gtk_gesture_register()
 * or gtk_gesture_register_static(), or a value from the #GtkGestureType
 * enum.
 *
 * If @gesture_id doesn't represent a registered gesture, or is already
 * handled by @interpreter, %FALSE will be returned
 *
 * Returns: %TRUE if the gesture is now handled
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gestures_interpreter_remove_gesture:
 * @interpreter: a #GtkGesturesInterpreter
 * @gesture_id: gesture ID to stop handling
 *
 * Removes @gesture_id from being handled by @interpreter.
 *
 * Since: 3.4
 **/
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

/**
 * gtk_gestures_interpreter_get_n_active_strokes:
 * @interpreter: a #GtkGesturesInterpreter
 *
 * Returns the number of devices/touch sequences currently interacting
 * with @interpreter.
 *
 * Returns: the number of strokes being performed in the interpreter.
 *
 * Since: 3.4
 **/
guint
gtk_gestures_interpreter_get_n_active_strokes (GtkGesturesInterpreter *interpreter)
{
  GtkGesturesInterpreterPrivate *priv;
  GHashTableIter iter;
  guint n_touches = 0;
  gpointer data;

  g_return_val_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter), 0);

  priv = interpreter->priv;
  g_hash_table_iter_init (&iter, priv->events);

  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      RecordedGesture *recorded = data;

      if (!recorded->finished)
        n_touches++;
    }

  return n_touches;
}

/**
 * gtk_gestures_interpreter_feed_event:
 * @interpreter: a #GtkGesturesInterpreter
 * @event: a #GdkEvent containing coordinates
 *
 * Feeds an input event into @interpreter, the coordinates of @event will
 * be used to build the user gesture that will be later compared to the
 * handled gestures.
 *
 * If @event doesn't contain coordinates information (gdk_event_get_coords()
 * returns %FALSE), %FALSE will be returned
 *
 * Returns: %TRUE if the event was useful to build the user gesture
 *
 * Since: 3.4
 **/
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

  if (event->type == GDK_BUTTON_RELEASE ||
      event->type == GDK_TOUCH_RELEASE)
    recorded->finished = TRUE;

  return TRUE;
}

static gdouble
get_angle_diff (gdouble angle1,
                gdouble angle2)
{
  gdouble diff;

  diff = fabs (angle1 - angle2);

  if (diff > G_PI)
    diff = (2 * G_PI) - diff;

  return diff;
}

/* This function "bends" the user gesture so it's equal
 * to the stock one, the areas resulting from bending the
 * sections are added up to calculate the weight.
 *
 * The max weight stores the worst case where every compared
 * vector goes in the opposite direction.
 *
 * Both weights are then used to determine the level of
 * confidence about the user gesture resembling the stock
 * gesture.
 */
static gboolean
compare_strokes (const GtkGestureStroke *stroke_gesture,
                 const GtkGestureStroke *stroke_stock,
                 gdouble                 angle_skew,
                 gdouble                *confidence)
{
  gdouble gesture_relative_length, stock_relative_length;
  gdouble gesture_angle, stock_angle;
  guint n_vectors_gesture, n_vectors_stock;
  guint gesture_length, stock_length;
  guint cur_gesture, cur_stock;
  gdouble weight = 0, max_weight = 0;

  cur_gesture = cur_stock = 0;
  n_vectors_gesture = gtk_gesture_stroke_get_n_vectors (stroke_gesture);
  n_vectors_stock = gtk_gesture_stroke_get_n_vectors (stroke_stock);

  if (!gtk_gesture_stroke_get_vector (stroke_gesture, 0, &gesture_angle,
                                      &gesture_length, &gesture_relative_length))
    return FALSE;

  if (!gtk_gesture_stroke_get_vector (stroke_stock, 0, &stock_angle,
                                      &stock_length, &stock_relative_length))
    return FALSE;

  if (gesture_length == 0 && n_vectors_gesture == 1)
    {
      /* If both gestures have 0-length, they're good */
      if (stock_length == 0 && n_vectors_stock == 1)
        {
          *confidence = 1;
          return TRUE;
        }

      return FALSE;
    }

  while (cur_stock < n_vectors_stock &&
         cur_gesture < n_vectors_gesture)
    {
      gdouble min_rel_length, angle;

      min_rel_length = MIN (gesture_relative_length, stock_relative_length);

      angle = stock_angle + angle_skew;

      if (angle > 2 * G_PI)
        angle -= 2 * G_PI;

      /* Add up the area resulting from bending the current vector
       * in the user gesture so it's shaped like the stock one.
       */
      weight +=
        get_angle_diff (gesture_angle, angle) *
        sqrt (min_rel_length);

      /* Max weight stores the most disastrous angle difference,
       * to be used later to determine the confidence of the
       * result.
       */
      max_weight += G_PI * sqrt (min_rel_length);

      gesture_relative_length -= min_rel_length;
      stock_relative_length -= min_rel_length;

      if (gesture_relative_length <= 0)
        {
          cur_gesture++;

          if (cur_gesture < n_vectors_gesture)
            gtk_gesture_stroke_get_vector (stroke_gesture, cur_gesture,
                                           &gesture_angle, &gesture_length,
                                           &gesture_relative_length);
        }

      if (stock_relative_length <= 0)
        {
          cur_stock++;

          if (cur_stock < n_vectors_stock)
            gtk_gesture_stroke_get_vector (stroke_stock, cur_stock,
                                           &stock_angle, &stock_length,
                                           &stock_relative_length);
        }
    }

  *confidence = (max_weight - weight) / max_weight;

  return TRUE;
}

static gdouble
gesture_get_angle_skew (const GtkGestureStroke *stroke_gesture,
                        const GtkGestureStroke *stroke_stock,
                        GtkGestureFlags         flags)
{
  gdouble gesture_angle, stock_angle;

  if ((flags & GTK_GESTURE_FLAG_IGNORE_INITIAL_ORIENTATION) == 0)
    return 0;

  if (!gtk_gesture_stroke_get_vector (stroke_gesture, 0, &gesture_angle,
                                      NULL, NULL))
    return 0;

  if (!gtk_gesture_stroke_get_vector (stroke_stock, 0, &stock_angle,
                                      NULL, NULL))
    return 0;

  /* Find out the angle skew to apply to the whole stock gesture, so
   * the initial orientation of both gestures is most similar.
   */
  return gesture_angle - stock_angle;
}

static GArray *
map_gesture_strokes (const GtkGesture *gesture,
                     const GtkGesture *stock,
                     guint             matched_gesture_stroke,
                     gint              match_dx,
                     gint              match_dy,
                     gdouble           angle_skew)
{
  GdkPoint stock_bounding_box[2] = {{ G_MAXINT, G_MAXINT }, { G_MININT, G_MININT }};
  GdkPoint gesture_bounding_box[2] = {{ G_MAXINT, G_MAXINT }, { G_MININT, G_MININT }};
  GdkPoint *stock_points, *gesture_points;
  guint n_strokes, i, j;
  GArray *map;

  n_strokes = gtk_gesture_get_n_strokes (stock);
  map = g_array_sized_new (FALSE, FALSE, sizeof (guint), n_strokes);
  stock_points = g_new (GdkPoint, n_strokes);
  gesture_points = g_new (GdkPoint, n_strokes);

  /* Get initial coordinates for every stroke in the stock gesture */
  for (i = 0; i < n_strokes; i++)
    {
      gtk_gesture_get_stroke (stock, i,
                              &(stock_points[i].x),
                              &(stock_points[i].y));

      stock_bounding_box[0].x = MIN (stock_bounding_box[0].x,
                                     stock_points[i].x);
      stock_bounding_box[0].y = MIN (stock_bounding_box[0].y,
                                     stock_points[i].y);
      stock_bounding_box[1].x = MAX (stock_bounding_box[1].x,
                                     stock_points[i].x);
      stock_bounding_box[1].y = MAX (stock_bounding_box[1].y,
                                     stock_points[i].y);
    }

  /* Get initial coordinates for every stroke in the
   * user gesture and rotate by the angle skew
   */
  for (i = 0; i < n_strokes; i++)
    {
      gint dx, dy, x, y;

      gtk_gesture_get_stroke (gesture, i, &dx, &dy);

      x = dx - match_dx;
      y = dy - match_dy;

      /* rotate by the angle skew */
      gesture_points[i].x = (int) ((x * cosf (angle_skew)) - (y * sinf (angle_skew)));
      gesture_points[i].y = (int) ((x * sinf (angle_skew)) + (y * cosf (angle_skew)));

      gesture_bounding_box[0].x = MIN (gesture_bounding_box[0].x,
                                       gesture_points[i].x);
      gesture_bounding_box[0].y = MIN (gesture_bounding_box[0].y,
                                       gesture_points[i].y);
      gesture_bounding_box[1].x = MAX (gesture_bounding_box[1].x,
                                       gesture_points[i].x);
      gesture_bounding_box[1].y = MAX (gesture_bounding_box[1].y,
                                       gesture_points[i].y);

      /* This is the first stroke in the stock gesture,
       * it's already mapped and matched, so no need to
       * go for it.
       */
      if (i == matched_gesture_stroke)
        {
          g_array_append_val (map, i);
          continue;
        }
    }

  /* Enforce a minimum non-zero size to the bounding box */
  if (stock_bounding_box[0].x == stock_bounding_box[1].x)
    stock_bounding_box[1].x++;
  if (stock_bounding_box[0].y == stock_bounding_box[1].y)
    stock_bounding_box[1].y++;
  if (gesture_bounding_box[0].x == gesture_bounding_box[1].x)
    gesture_bounding_box[1].x++;
  if (gesture_bounding_box[0].y == gesture_bounding_box[1].y)
    gesture_bounding_box[1].y++;

  /* Now assign the closest matches,
   * or bail out if there's none
   */
  for (i = 1; i < n_strokes; i++)
    {
      gdouble stock_x, stock_y, gesture_x, gesture_y;
      gdouble diff_x, diff_y, min_x, min_y;
      gint stock_side, gesture_side;
      guint match = 0;

      min_x = min_y = G_MAXDOUBLE;
      stock_side = MAX ((stock_bounding_box[1].x - stock_bounding_box[0].x),
                        (stock_bounding_box[1].y - stock_bounding_box[0].y));

      stock_x = ((gdouble) stock_points[i].x) / stock_side;
      stock_y = ((gdouble) stock_points[i].y) / stock_side;

      for (j = 0; j < n_strokes; j++)
        {
          if (j == matched_gesture_stroke)
            continue;

          gesture_side = MAX ((gesture_bounding_box[1].x - gesture_bounding_box[0].x),
                              (gesture_bounding_box[1].y - gesture_bounding_box[0].y));

          /* Normalize coordinates */
          gesture_x = ((gdouble) gesture_points[j].x) / gesture_side;
          gesture_y = ((gdouble) gesture_points[j].y) / gesture_side;

          diff_x = ABS (gesture_x - stock_x);
          diff_y = ABS (gesture_y - stock_y);

          /* Is this the closest match? */
          if (diff_x < min_x && diff_y < min_y)
            {
              min_x = diff_x;
              min_y = diff_y;
              match = j;
            }
        }

      /* The closest match is still way off
       * from where it's supposed to be.
       */
      if (min_x > INITIAL_COORDINATE_THRESHOLD ||
          min_y > INITIAL_COORDINATE_THRESHOLD)
        {
          g_array_free (map, TRUE);
          map = NULL;
          break;
        }

      g_array_append_val (map, match);
    }

  g_free (gesture_points);
  g_free (stock_points);

  return map;
}

static gboolean
compare_gestures (const GtkGesture *gesture,
                  const GtkGesture *stock,
                  gdouble          *confidence)
{
  const GtkGestureStroke *stock_first_stroke, *gesture_stroke;
  guint gesture_n_strokes, stock_n_strokes;
  GtkGestureFlags flags;
  gdouble angle_skew;

  gesture_n_strokes = gtk_gesture_get_n_strokes (gesture);
  stock_n_strokes = gtk_gesture_get_n_strokes (stock);

  if (gesture_n_strokes != stock_n_strokes)
    return FALSE;

  flags = gtk_gesture_get_flags (stock);
  stock_first_stroke = gtk_gesture_get_stroke (stock, 0, NULL, NULL);

  if (gesture_n_strokes == 1)
    {
      /* Only 1 stroke to be compared */
      gesture_stroke = gtk_gesture_get_stroke (gesture, 0, NULL, NULL);
      angle_skew = gesture_get_angle_skew (gesture_stroke,
                                           stock_first_stroke,
                                           flags);

      return compare_strokes (gesture_stroke, stock_first_stroke,
                              angle_skew, confidence);
    }
  else
    {
      const GtkGestureStroke *stock_stroke;
      gdouble intermediate_confidence, accum;
      guint i, j, n;

      /* Find the best candidate(s) to being the
       * first stroke of the stock gesture.
       */
      for (i = 0; i < gesture_n_strokes; i++)
        {
          gint dx, dy;
          GArray *map;

          gesture_stroke = gtk_gesture_get_stroke (gesture, i, &dx, &dy);

          angle_skew = gesture_get_angle_skew (gesture_stroke,
                                               stock_first_stroke,
                                               flags);

          if (!compare_strokes (gesture_stroke, stock_first_stroke,
                                angle_skew, &intermediate_confidence) ||
              intermediate_confidence < MINIMUM_CONFIDENCE_ALLOWED)
            continue;

          accum = intermediate_confidence;
          map = map_gesture_strokes (gesture, stock, i, dx, dy, angle_skew);

          if (!map)
            continue;

          /* Now compare the remaining strokes as per the map */
          for (j = 1; j < map->len; j++)
            {
              n = g_array_index (map, guint, j);
              stock_stroke = gtk_gesture_get_stroke (stock, j, NULL, NULL);
              gesture_stroke = gtk_gesture_get_stroke (gesture, n, NULL, NULL);

              if (!compare_strokes (gesture_stroke, stock_stroke,
                                    angle_skew, &intermediate_confidence))
                break;

              accum += intermediate_confidence;

              if ((accum / j) < MINIMUM_CONFIDENCE_ALLOWED)
                break;
            }

          g_array_free (map, TRUE);

          if (j == stock_n_strokes &&
              (accum / stock_n_strokes) >= MINIMUM_CONFIDENCE_ALLOWED)
            {
              *confidence = (accum / stock_n_strokes);
              return TRUE;
            }
        }
    }

  return FALSE;
}

/**
 * gtk_gestures_interpreter_finish:
 * @interpreter: a #GtkGesturesInterpreter
 * @gesture_id: (out) (allow-none): return location for the resulting
 *              gesture ID, or %NULL
 *
 * Finishes the user gesture and compares it to the handled gestures,
 * returning %TRUE and filling in @gesture_id with the resulting gesture
 * in case of success.
 *
 * If %FALSE is returned, no gesture was recognized.
 *
 * Returns: %TRUE if the gesture matched
 *
 * Since: 3.4
 **/
gboolean
gtk_gestures_interpreter_finish (GtkGesturesInterpreter *interpreter,
                                 guint                  *gesture_id)
{
  GtkGesturesInterpreterPrivate *priv;
  GtkGesture *user_gesture = NULL;
  GtkGestureStroke *stroke;
  RecordedGesture *recorded;
  GHashTableIter iter;
  gdouble confidence, max_confidence = -1;
  guint i, gesture_id_found = 0;
  GdkPoint *point, base_point;

  g_return_val_if_fail (GTK_IS_GESTURES_INTERPRETER (interpreter), FALSE);

  priv = interpreter->priv;

  if (g_hash_table_size (priv->events) == 0)
    return FALSE;

  g_hash_table_iter_init (&iter, priv->events);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &recorded))
    {
      stroke = recorded_gesture_vectorize (recorded);

      if (!stroke)
        continue;

      point = &g_array_index (recorded->coordinates, GdkPoint, 0);

      if (!user_gesture)
        {
          user_gesture = gtk_gesture_new (stroke, 0);
          base_point = *point;
        }
      else
        gtk_gesture_add_stroke (user_gesture, stroke,
                                  point->x - base_point.x,
                                  point->y - base_point.y);

      gtk_gesture_stroke_free (stroke);
      g_hash_table_iter_remove (&iter);
    }

  if (!user_gesture)
    return FALSE;

  g_signal_emit (interpreter, signals[EVENTS_VECTORIZED], 0, user_gesture);

  for (i = 0; i < priv->handled_gestures->len; i++)
    {
      guint handled_gesture_id;
      const GtkGesture *handled_gesture;

      handled_gesture_id = g_array_index (priv->handled_gestures, guint, i);
      handled_gesture = gtk_gesture_lookup (handled_gesture_id);
      g_assert (handled_gesture != NULL);

      if (!compare_gestures (user_gesture, handled_gesture, &confidence))
        continue;

      if (confidence > max_confidence)
        {
          gesture_id_found = handled_gesture_id;
	  max_confidence = confidence;
	}
    }

  gtk_gesture_free (user_gesture);

  if (gesture_id_found == 0)
    return FALSE;

  g_signal_emit (interpreter, signals[GESTURE_DETECTED], 0,
		 gesture_id_found, max_confidence);

  if (max_confidence < MINIMUM_CONFIDENCE_ALLOWED)
    return FALSE;

  if (gesture_id)
    *gesture_id = gesture_id_found;

  return TRUE;
}
