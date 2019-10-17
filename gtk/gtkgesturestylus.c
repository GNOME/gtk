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

/**
 * SECTION:gtkgesturestylus
 * @Short_description: Gesture for stylus input
 * @Title: GtkGestureStylus
 * @See_also: #GtkGesture, #GtkGestureSingle
 *
 * #GtkGestureStylus is a #GtkGesture implementation specific to stylus
 * input. The provided signals just provide the basic information
 */

#include "config.h"
#include "gtkgesturestylus.h"
#include "gtkgesturestylusprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"

G_DEFINE_TYPE (GtkGestureStylus, gtk_gesture_stylus, GTK_TYPE_GESTURE_SINGLE)

enum {
  PROXIMITY,
  DOWN,
  MOTION,
  UP,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static gboolean
gtk_gesture_stylus_handle_event (GtkEventController *controller,
                                 const GdkEvent     *event)
{
  GdkModifierType modifiers;
  guint n_signal;
  gdouble x, y;

  GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_stylus_parent_class)->handle_event (controller, event);

  if (!gdk_event_get_device_tool (event))
    return FALSE;
  if (!gdk_event_get_coords (event, &x, &y))
    return FALSE;

  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_BUTTON_PRESS:
      n_signal = DOWN;
      break;
    case GDK_BUTTON_RELEASE:
      n_signal = UP;
      break;
    case GDK_MOTION_NOTIFY:
      gdk_event_get_state (event, &modifiers);

      if (modifiers & GDK_BUTTON1_MASK)
        n_signal = MOTION;
      else
        n_signal = PROXIMITY;
      break;
    default:
      return FALSE;
    }

  g_signal_emit (controller, signals[n_signal], 0, x, y);

  return TRUE;
}

static void
gtk_gesture_stylus_class_init (GtkGestureStylusClass *klass)
{
  GtkEventControllerClass *event_controller_class;

  event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  event_controller_class->handle_event = gtk_gesture_stylus_handle_event;

  signals[PROXIMITY] =
    g_signal_new (I_("proximity"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, proximity),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[PROXIMITY],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  signals[DOWN] =
    g_signal_new (I_("down"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, down),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[DOWN],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  signals[MOTION] =
    g_signal_new (I_("motion"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, motion),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[MOTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  signals[UP] =
    g_signal_new (I_("up"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureStylusClass, up),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[UP],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_gesture_stylus_init (GtkGestureStylus *gesture)
{
}

/**
 * gtk_gesture_stylus_new:
 *
 * Creates a new #GtkGestureStylus.
 *
 * Returns: a newly created stylus gesture
 **/
GtkGesture *
gtk_gesture_stylus_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_STYLUS,
                       NULL);
}

static const GdkEvent *
gesture_get_current_event (GtkGestureStylus *gesture)
{
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  return gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
}

/**
 * gtk_gesture_stylus_get_axis:
 * @gesture: a #GtkGestureStylus
 * @axis: requested device axis
 * @value: (out): return location for the axis value
 *
 * Returns the current value for the requested @axis. This function
 * must be called from either the #GtkGestureStylus::down,
 * #GtkGestureStylus::motion, #GtkGestureStylus::up or #GtkGestureStylus::proximity
 * signals.
 *
 * Returns: #TRUE if there is a current value for the axis
 **/
gboolean
gtk_gesture_stylus_get_axis (GtkGestureStylus *gesture,
			     GdkAxisUse        axis,
			     gdouble          *value)
{
  const GdkEvent *event;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (axis < GDK_AXIS_LAST, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  event = gesture_get_current_event (gesture);
  if (!event)
    return FALSE;

  return gdk_event_get_axis (event, axis, value);
}

/**
 * gtk_gesture_stylus_get_axes:
 * @gesture: a GtkGestureStylus
 * @axes: (array): array of requested axes, terminated with #GDK_AXIS_IGNORE
 * @values: (out) (array): return location for the axis values
 *
 * Returns the current values for the requested @axes. This function
 * must be called from either the #GtkGestureStylus::down,
 * #GtkGestureStylus::motion, #GtkGestureStylus::up or #GtkGestureStylus::proximity
 * signals.
 *
 * Returns: #TRUE if there is a current value for the axes
 **/
gboolean
gtk_gesture_stylus_get_axes (GtkGestureStylus  *gesture,
			     GdkAxisUse         axes[],
			     gdouble          **values)
{
  const GdkEvent *event;
  GArray *array;
  gint i = 0;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (values != NULL, FALSE);

  event = gesture_get_current_event (gesture);
  if (!event)
    return FALSE;

  array = g_array_new (TRUE, FALSE, sizeof (gdouble));

  while (axes[i] != GDK_AXIS_IGNORE)
    {
      gdouble value;

      if (axes[i] >= GDK_AXIS_LAST)
        {
          g_warning ("Requesting unknown axis %d, did you "
                     "forget to add a last GDK_AXIS_IGNORE axis?",
                     axes[i]);
          g_array_free (array, TRUE);
          return FALSE;
        }

      gdk_event_get_axis (event, axes[i], &value);
      g_array_append_val (array, value);
      i++;
    }

  *values = (gdouble *) g_array_free (array, FALSE);
  return TRUE;
}

/**
 * gtk_gesture_stylus_get_backlog:
 * @gesture: a #GtkGestureStylus
 * @backlog: (out) (array length=n_elems): coordinates and times for the backlog events
 * @n_elems: (out): return location for the number of elements
 *
 * By default, GTK+ will limit rate of input events. On stylus input where
 * accuracy of strokes is paramount, this function returns the accumulated
 * coordinate/timing state before the emission of the current
 * #GtkGestureStylus:motion signal.
 *
 * This function may only be called within a #GtkGestureStylus::motion
 * signal handler, the state given in this signal and obtainable through
 * gtk_gesture_stylus_get_axis() call express the latest (most up-to-date)
 * state in motion history.
 *
 * @backlog is provided in chronological order.
 *
 * Returns: #TRUE if there is a backlog to unfold in the current state.
 **/
gboolean
gtk_gesture_stylus_get_backlog (GtkGestureStylus  *gesture,
				GdkTimeCoord     **backlog,
				guint             *n_elems)
{
  const GdkEvent *event;
  GArray *backlog_array;
  GList *history = NULL, *l;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);
  g_return_val_if_fail (backlog != NULL && n_elems != NULL, FALSE);

  event = gesture_get_current_event (gesture);

  if (event)
    history = gdk_event_get_motion_history (event);
  if (!history)
    return FALSE;

  backlog_array = g_array_new (FALSE, FALSE, sizeof (GdkTimeCoord));
  for (l = history; l; l = l->next)
    {
      GdkTimeCoord *time_coord = l->data;
      graphene_point_t p;

      g_array_append_val (backlog_array, *time_coord);
      time_coord = &g_array_index (backlog_array, GdkTimeCoord, backlog_array->len - 1);
      if (gtk_widget_compute_point (gtk_get_event_widget (event),
                                    gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)),
                                    &GRAPHENE_POINT_INIT (time_coord->axes[GDK_AXIS_X],
                                                          time_coord->axes[GDK_AXIS_Y]),
                                    &p))
        {
          time_coord->axes[GDK_AXIS_X] = p.x;
          time_coord->axes[GDK_AXIS_Y] = p.y;
        }
      else
        {
          g_array_set_size (backlog_array, backlog_array->len - 1);
        }
    }

  *n_elems = backlog_array->len;
  *backlog = (GdkTimeCoord *) g_array_free (backlog_array, FALSE);
  g_list_free (history);

  return TRUE;
}

/**
 * gtk_gesture_stylus_get_device_tool:
 * @gesture: a #GtkGestureStylus
 *
 * Returns the #GdkDeviceTool currently driving input through this gesture.
 * This function must be called from either the #GtkGestureStylus::down,
 * #GtkGestureStylus::motion, #GtkGestureStylus::up or #GtkGestureStylus::proximity
 * signal handlers.
 *
 * Returns: (nullable) (transfer none): The current stylus tool
 **/
GdkDeviceTool *
gtk_gesture_stylus_get_device_tool (GtkGestureStylus *gesture)
{
  const GdkEvent *event;

  g_return_val_if_fail (GTK_IS_GESTURE_STYLUS (gesture), FALSE);

  event = gesture_get_current_event (gesture);
  if (!event)
    return NULL;

  return gdk_event_get_device_tool (event);
}
