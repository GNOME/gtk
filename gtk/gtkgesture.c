/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
#include "config.h"
#include <gtk/gtkgesture.h>
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkintl.h"

typedef struct _GtkGesturePrivate GtkGesturePrivate;
typedef struct _PointData PointData;

enum {
  PROP_N_POINTS = 1,
  PROP_WINDOW
};

enum {
  CHECK,
  BEGIN,
  END,
  UPDATE,
  CANCEL,
  SEQUENCE_STATE_CHANGED,
  N_SIGNALS
};

struct _PointData
{
  GdkEvent *event;
  gdouble widget_x;
  gdouble widget_y;
  guint press_handled : 1;
  guint state : 2;
};

struct _GtkGesturePrivate
{
  GHashTable *points;
  GdkEventSequence *last_sequence;
  GdkWindow *user_window;
  GdkWindow *window;
  GdkDevice *device;
  guint n_points;
  guint recognized : 1;
};

static guint signals[N_SIGNALS] = { 0 };

#define BUTTONS_MASK (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkGesture, gtk_gesture, GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_gesture_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      g_value_set_uint (value, priv->n_points);
      break;
    case PROP_WINDOW:
      g_value_set_object (value, priv->user_window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  switch (prop_id)
    {
    case PROP_N_POINTS:
      priv->n_points = g_value_get_uint (value);
      break;
    case PROP_WINDOW:
      gtk_gesture_set_window (GTK_GESTURE (object),
                              g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_finalize (GObject *object)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (GTK_GESTURE (object));

  g_hash_table_destroy (priv->points);

  G_OBJECT_CLASS (gtk_gesture_parent_class)->finalize (object);
}

static guint
_gtk_gesture_effective_n_points (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;
      if (data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      n_points++;
    }

  return n_points;
}

static gboolean
gtk_gesture_check_impl (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;
  guint n_points;

  priv = gtk_gesture_get_instance_private (gesture);
  n_points = _gtk_gesture_effective_n_points (gesture);

  return n_points == priv->n_points;
}

static void
_gtk_gesture_set_recognized (GtkGesture       *gesture,
                             gboolean          recognized,
                             GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->recognized == recognized)
    return;

  priv->recognized = recognized;

  if (recognized)
    g_signal_emit (gesture, signals[BEGIN], 0, sequence);
  else
    g_signal_emit (gesture, signals[END], 0, sequence);
}

static gboolean
_gtk_gesture_do_check (GtkGesture *gesture)
{
  gboolean retval;

  g_signal_emit (G_OBJECT (gesture), signals[CHECK], 0, &retval);
  retval = retval != FALSE;

  return retval;
}

static gboolean
_gtk_gesture_check_recognized (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv = gtk_gesture_get_instance_private (gesture);
  guint current_n_points;

  current_n_points = _gtk_gesture_effective_n_points (gesture);

  if (priv->recognized && current_n_points != priv->n_points)
    _gtk_gesture_set_recognized (gesture, FALSE, sequence);
  else if (!priv->recognized &&
           current_n_points == priv->n_points &&
           g_hash_table_size (priv->points) == priv->n_points &&
           _gtk_gesture_do_check (gesture))
    _gtk_gesture_set_recognized (gesture, TRUE, sequence);

  return priv->recognized;
}

/* Finds the first window pertaining to the controller's widget */
static GdkWindow *
_find_widget_window (GtkGesture *gesture,
                     GdkWindow  *window)
{
  GtkWidget *widget, *window_widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  while (window)
    {
      gdk_window_get_user_data (window, (gpointer*) &window_widget);

      if (window_widget == widget ||
          gtk_widget_get_window (widget) == window)
        return window;

      window = gdk_window_get_effective_parent (window);
    }

  return NULL;
}

static void
_update_widget_coordinates (GtkGesture *gesture,
                            PointData  *data)
{
  GdkWindow *window, *event_widget_window;
  GtkWidget *event_widget, *widget;
  GtkAllocation allocation;
  gdouble event_x, event_y;
  gint wx, wy, x, y;

  event_widget = gtk_get_event_widget (data->event);
  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  event_widget_window = gtk_widget_get_window (event_widget);
  gdk_event_get_coords (data->event, &event_x, &event_y);
  window = data->event->any.window;

  while (window != event_widget_window)
    {
      gdk_window_get_position (window, &wx, &wy);
      event_x += wx;
      event_y += wy;
      window = gdk_window_get_effective_parent (window);
    }

  if (!gtk_widget_get_has_window (event_widget))
    {
      gtk_widget_get_allocation (event_widget, &allocation);
      event_x -= allocation.x;
      event_y -= allocation.y;
    }

  gtk_widget_translate_coordinates (event_widget, widget,
                                    event_x, event_y, &x, &y);
  data->widget_x = x;
  data->widget_y = y;
}

static gboolean
_gtk_gesture_update_point (GtkGesture     *gesture,
                           const GdkEvent *event,
                           gboolean        add)
{
  GdkEventSequence *sequence;
  GdkWindow *widget_window;
  GtkGesturePrivate *priv;
  GdkDevice *device;
  PointData *data;
  gdouble x, y;

  if (!gdk_event_get_coords (event, &x, &y))
    return FALSE;

  device = gdk_event_get_device (event);

  if (!device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  widget_window = _find_widget_window (gesture, event->any.window);

  if (add)
    {
      /* If the event happens with the wrong device, or
       * on the wrong window, ignore.
       */
      if (priv->device && priv->device != device)
        return FALSE;
      if (priv->window && priv->window != widget_window)
        return FALSE;
      if (priv->user_window && priv->user_window != widget_window)
        return FALSE;
    }
  else if (!priv->device || !priv->window)
    return FALSE;

  sequence = gdk_event_get_event_sequence (event);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    {
      if (!add)
        return FALSE;

      if (g_hash_table_size (priv->points) == 0)
        {
          priv->window = widget_window;
          priv->device = device;
        }

      data = g_new0 (PointData, 1);
      g_hash_table_insert (priv->points, sequence, data);
    }

  if (data->event)
    gdk_event_free (data->event);

  data->event = gdk_event_copy (event);
  _update_widget_coordinates (gesture, data);

  return TRUE;
}

static void
_gtk_gesture_check_empty (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);

  if (g_hash_table_size (priv->points) == 0)
    {
      priv->window = NULL;
      priv->device = NULL;
    }
}

static void
_gtk_gesture_remove_point (GtkGesture     *gesture,
                           const GdkEvent *event)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *device;

  sequence = gdk_event_get_event_sequence (event);
  device = gdk_event_get_device (event);
  priv = gtk_gesture_get_instance_private (gesture);

  if (priv->device != device)
    return;

  g_hash_table_remove (priv->points, sequence);
  _gtk_gesture_check_empty (gesture);
}

static void
_gtk_gesture_cancel_all (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GHashTableIter iter;

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &sequence, NULL))
    {
      g_signal_emit (gesture, signals[CANCEL], 0, sequence);
      g_hash_table_iter_remove (&iter);
      _gtk_gesture_check_recognized (gesture, sequence);
    }

  _gtk_gesture_check_empty (gesture);
}

static gboolean
gtk_gesture_handle_event (GtkEventController *controller,
                          const GdkEvent     *event)
{
  GtkGesture *gesture = GTK_GESTURE (controller);
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GdkDevice *source_device;
  gboolean was_recognized;

  source_device = gdk_event_get_source_device (event);

  if (!source_device)
    return FALSE;

  priv = gtk_gesture_get_instance_private (gesture);
  sequence = gdk_event_get_event_sequence (event);
  was_recognized = gtk_gesture_is_recognized (gesture);

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_DENIED)
    priv->last_sequence = sequence;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_TOUCH_BEGIN:
      if (_gtk_gesture_update_point (gesture, event, TRUE) &&
          _gtk_gesture_check_recognized (gesture, sequence))
        {
          PointData *data;

          data = g_hash_table_lookup (priv->points, sequence);
          data->press_handled = TRUE;
        }

      break;
    case GDK_BUTTON_RELEASE:
    case GDK_TOUCH_END:
      if (_gtk_gesture_update_point (gesture, event, FALSE))
        {
          if (was_recognized &&
              _gtk_gesture_check_recognized (gesture, sequence))
            g_signal_emit (gesture, signals[UPDATE], 0, sequence);

          _gtk_gesture_remove_point (gesture, event);
        }
      break;
    case GDK_MOTION_NOTIFY:
      if ((event->motion.state & BUTTONS_MASK) == 0)
        break;

      if (event->motion.is_hint)
        gdk_event_request_motions (&event->motion);

      /* Fall through */
    case GDK_TOUCH_UPDATE:
      if (_gtk_gesture_update_point (gesture, event, FALSE) &&
          _gtk_gesture_check_recognized (gesture, sequence))
        g_signal_emit (gesture, signals[UPDATE], 0, sequence);
      break;
    default:
      /* Unhandled event */
      return FALSE;
    }

  if (gtk_gesture_get_sequence_state (gesture, sequence) != GTK_EVENT_SEQUENCE_CLAIMED)
    return FALSE;

  return priv->recognized;
}

static void
gtk_gesture_reset (GtkEventController *controller)
{
  _gtk_gesture_cancel_all (GTK_GESTURE (controller));
}

static void
gtk_gesture_class_init (GtkGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_gesture_get_property;
  object_class->set_property = gtk_gesture_set_property;
  object_class->finalize = gtk_gesture_finalize;

  controller_class->handle_event = gtk_gesture_handle_event;
  controller_class->reset = gtk_gesture_reset;

  klass->check = gtk_gesture_check_impl;

  g_object_class_install_property (object_class,
                                   PROP_N_POINTS,
                                   g_param_spec_uint ("n-points",
                                                      P_("Number of points"),
                                                      P_("Number of points needed "
                                                         "to trigger the gesture"),
                                                      1, G_MAXUINT, 1,
                                                      GTK_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_boolean ("window",
                                                         P_("GdkWindow to receive events about"),
                                                         P_("GdkWindow to receive events about"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  signals[CHECK] =
    g_signal_new ("check",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, check),
                  g_signal_accumulator_true_handled,
                  NULL, NULL,
                  G_TYPE_BOOLEAN, 0);
  signals[BEGIN] =
    g_signal_new ("begin",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, begin),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
  signals[END] =
    g_signal_new ("end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, end),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
  signals[UPDATE] =
    g_signal_new ("update",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, update),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
  signals[CANCEL] =
    g_signal_new ("cancel",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, cancel),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
  signals[SEQUENCE_STATE_CHANGED] =
    g_signal_new ("sequence-state-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureClass, sequence_state_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_POINTER,
                  GTK_TYPE_EVENT_SEQUENCE_STATE);
}

static void
gtk_gesture_init (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  priv = gtk_gesture_get_instance_private (gesture);
  priv->points = g_hash_table_new_full (NULL, NULL, NULL,
                                        (GDestroyNotify) g_free);
  gtk_event_controller_set_event_mask (GTK_EVENT_CONTROLLER (gesture),
                                       GDK_TOUCH_MASK);
}

/**
 * gtk_gesture_get_device:
 * @gesture: a #GtkGesture
 *
 * Returns the master #GdkDevice that is currently operating
 * on @gesture, or %NULL if the gesture is not being interacted.
 *
 * Returns: a #GdkDevice, or %NULL.
 *
 * Since: 3.14
 **/
GdkDevice *
gtk_gesture_get_device (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->device;
}

/**
 * gtk_gesture_get_sequence_state:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns the @sequence state, as seen by @gesture.
 *
 * Returns: The sequence state in @gesture.
 *
 * Since: 3.14
 **/
GtkEventSequenceState
gtk_gesture_get_sequence_state (GtkGesture       *gesture,
                                GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture),
                        GTK_EVENT_SEQUENCE_NONE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return GTK_EVENT_SEQUENCE_NONE;

  return data->state;
}

/**
 * gtk_gesture_set_sequence_state:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 * @state: the sequence state
 *
 * Sets the state of @sequence in @gesture. Sequences start
 * in state #GTK_EVENT_SEQUENCE_NONE, and whenever they change
 * state, they can never go back to that state. Likewise,
 * sequences in state #GTK_EVENT_SEQUENCE_DENIED cannot turn
 * back to a not denied state. With these rules, the lifetime
 * of an event sequence is constrained to the next four:
 *
 * * None
 * * None → Denied
 * * None → Claimed
 * * None → Claimed → Denied
 *
 * Returns: #TRUE if @sequence is handled by @gesture,
 *          and the state is changed successfully.
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_set_sequence_state (GtkGesture            *gesture,
                                GdkEventSequence      *sequence,
                                GtkEventSequenceState  state)
{
  GtkGesturePrivate *priv;
  PointData *data;
  guint old_state;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (state >= GTK_EVENT_SEQUENCE_NONE &&
                    state <= GTK_EVENT_SEQUENCE_DENIED);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data || data->state == state)
    return FALSE;

  /* denied sequences remain denied */
  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  /* Sequences can't go from claimed/denied to none */
  if (state == GTK_EVENT_SEQUENCE_NONE &&
      data->state != GTK_EVENT_SEQUENCE_NONE)
    return FALSE;

  old_state = data->state;
  data->state = state;
  g_signal_emit (gesture, signals[SEQUENCE_STATE_CHANGED], 0,
                 sequence, old_state);

  if (state == GTK_EVENT_SEQUENCE_DENIED)
    _gtk_gesture_check_recognized (gesture, sequence);

  return TRUE;
}

/**
 * gtk_gesture_get_sequences:
 * @gesture: a #GtkGesture
 *
 * Returns the list of #GdkEventSequence<!-- -->s currently being interpreted
 * by @gesture
 *
 * Returns: (transfer container) (element-type:Gdk.EventSequence): A list
 *          of #GdkEventSequence<!-- -->s, the list elements are owned by GTK+
 *          and must not be freed or modified, the list itself must be deleted
 *          through g_list_free()
 **/
GList *
gtk_gesture_get_sequences (GtkGesture *gesture)
{
  GdkEventSequence *sequence;
  GtkGesturePrivate *priv;
  GList *sequences = NULL;
  GHashTableIter iter;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer *) &sequence, (gpointer *) &data))
    {
      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;
      if (data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      sequences = g_list_prepend (sequences, sequence);
    }

  return sequences;
}

/**
 * gtk_gesture_get_last_updated_sequence:
 * @gesture: a #GtkGesture
 *
 * Returns the #GdkEventSequence that was last updated on @gesture.
 *
 * Returns: The last updated sequence.
 *
 * Since: 3.14
 **/
GdkEventSequence *
gtk_gesture_get_last_updated_sequence (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->last_sequence;
}

const GdkEvent *
gtk_gesture_get_last_event (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return NULL;

  return data->event;
}

/**
 * gtk_gesture_get_point:
 * @gesture: a #GtkGesture
 * @sequence: (allow-none): a #GdkEventSequence, or %NULL for pointer events
 * @x: (out) (allow-none): return location for X axis of the sequence coordinates
 * @y: (out) (allow-none): return location for Y axis of the sequence coordinates
 *
 * If @sequence is currently being interpreted by @gesture, this
 * function returns %TRUE and fills in @x and @y with the last coordinates
 * stored for that event sequence.
 *
 * Returns: %TRUE if @sequence is currently interpreted
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_point (GtkGesture       *gesture,
                       GdkEventSequence *sequence,
                       gdouble          *x,
                       gdouble          *y)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (x)
    *x = data->widget_x;
  if (y)
    *y = data->widget_y;

  return TRUE;
}

/**
 * gtk_gesture_get_last_update_time:
 * @gesture: a #GtkGesture
 * @sequence: (allow-none): a #GdkEventSequence, or %NULL for pointer events
 * @evtime: (out) (allow-none): return location for last update time
 *
 * If @sequence is being interpreted by @gesture, this function
 * returns %TRUE and fills @evtime with the last event time it
 * received from that @sequence.
 *
 * Returns: %TRUE if @sequence is currently interpreted
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_last_update_time (GtkGesture       *gesture,
                                  GdkEventSequence *sequence,
                                  guint32          *evtime)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return FALSE;

  if (evtime)
    *evtime = gdk_event_get_time (data->event);

  return TRUE;
};

/**
 * gtk_gesture_get_last_event_type:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns the last event type that was processed for @sequence.
 *
 * Returns: the last event type.
 *
 * Since: 3.14
 **/
GdkEventType
gtk_gesture_get_last_event_type (GtkGesture       *gesture,
                                 GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), GDK_NOTHING);

  priv = gtk_gesture_get_instance_private (gesture);

  if (!g_hash_table_lookup_extended (priv->points, sequence,
                                     NULL, (gpointer *) &data))
    return GDK_NOTHING;

  return data->event->type;
}

/**
 * gtk_gesture_get_bounding_box:
 * @gesture: a #GtkGesture
 * @rect: (out): bounding box containing all active touches.
 *
 * If there are touch sequences being currently handled by @gesture,
 * this function returns #TRUE and fills in @rect with the bounding
 * box containing all active touches. Otherwise, #FALSE will be
 * returned.
 *
 * Returns: #TRUE if there are active touches, #FALSE otherwise
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_bounding_box (GtkGesture   *gesture,
                              GdkRectangle *rect)
{
  GtkGesturePrivate *priv;
  gdouble x1, y1, x2, y2;
  GHashTableIter iter;
  guint n_points = 0;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  x1 = y1 = G_MAXDOUBLE;
  x2 = y2 = -G_MAXDOUBLE;

  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      gdouble x, y;

      if (data->state == GTK_EVENT_SEQUENCE_DENIED)
        continue;
      if (data->event->type == GDK_TOUCH_END ||
          data->event->type == GDK_BUTTON_RELEASE)
        continue;

      gdk_event_get_coords (data->event, &x, &y);
      n_points++;
      x1 = MIN (x1, x);
      y1 = MIN (y1, y);
      x2 = MAX (x2, x);
      y2 = MAX (y2, y);
    }

  if (n_points == 0)
    return FALSE;

  rect->x = x1;
  rect->y = y1;
  rect->width = x2 - x1;
  rect->height = y2 - y1;

  return TRUE;
}


/**
 * gtk_gesture_get_bounding_box_center:
 * @gesture: a #GtkGesture
 * @x: (out): X coordinate for the bounding box center
 * @y: (out): Y coordinate for the bounding box center
 *
 * If there are touch sequences being currently handled by @gesture,
 * this function returns #TRUE and fills in @x and @y with the center
 * of the bounding box containing all active touches. Otherwise, #FALSE
 * will be returned.
 *
 * Returns: #FALSE if no active touches are present, #TRUE otherwise
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_get_bounding_box_center (GtkGesture *gesture,
                                     gdouble    *x,
                                     gdouble    *y)
{
  GdkRectangle rect;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);

  if (!gtk_gesture_get_bounding_box (gesture, &rect))
    return FALSE;

  *x = rect.x + rect.width / 2;
  *y = rect.y + rect.height / 2;
  return TRUE;
}

/**
 * gtk_gesture_is_active:
 * @gesture: a #GtkGesture
 *
 * Returns %TRUE if the gesture is currently active.
 * A gesture is active meanwhile there are touch sequences
 * interacting with it.
 *
 * Returns: %TRUE if gesture is active.
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_is_active (GtkGesture *gesture)
{
  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  return _gtk_gesture_effective_n_points (gesture) != 0;
}

/**
 * gtk_gesture_is_recognized:
 * @gesture: a #GtkGesture
 *
 * Returns %TRUE if the gesture is currently recognized.
 * A gesture is recognized if there are as many interacting
 * touch sequences as required by @gesture, and #GtkGesture::check
 * returned #TRUE for the sequences being currently interpreted.
 *
 * Returns: %TRUE if gesture is recognized.
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_is_recognized (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->recognized;
}

/**
 * gtk_gesture_check:
 * @gesture: a #GtkGesture
 *
 * Triggers a check on the @gesture, this should be rarely needed,
 * as the gesture will check the state after handling any event.
 *
 * Returns: #TRUE if the gesture is recognized.
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_check (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);

  return _gtk_gesture_check_recognized (gesture, priv->last_sequence);
}

/**
 * gtk_gesture_handles_sequence:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Returns #TRUE if @gesture is currently handling events corresponding to
 * @sequence.
 *
 * Returns: #TRUE if @gesture is handling @sequence.
 *
 * Since: 3.14
 **/
gboolean
gtk_gesture_handles_sequence (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  if (data->state == GTK_EVENT_SEQUENCE_DENIED)
    return FALSE;

  return TRUE;
}

/**
 * gtk_gesture_cancel_sequence:
 * @gesture: a #GtkGesture
 * @sequence: a #GdkEventSequence
 *
 * Cancels @sequence on @gesture, this emits #GtkGesture::cancel
 * and forgets the sequence altogether.
 *
 * Returns: #TRUE if the sequence was being handled by gesture
 **/
gboolean
gtk_gesture_cancel_sequence (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  g_signal_emit (gesture, signals[CANCEL], 0, sequence);
  _gtk_gesture_check_recognized (gesture, sequence);
  _gtk_gesture_remove_point (gesture, data->event);
  return TRUE;
}

/**
 * gtk_gesture_get_window:
 * @gesture: a #GtkGesture
 *
 * Returns the user-defined window that receives the events
 * handled by @gesture. See gtk_gesture_set_window() for more
 * information.
 *
 * Returns: the user defined window, or %NULL if none.
 *
 * Since: 3.14
 **/
GdkWindow *
gtk_gesture_get_window (GtkGesture *gesture)
{
  GtkGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), NULL);

  priv = gtk_gesture_get_instance_private (gesture);

  return priv->user_window;
}

/**
 * gtk_gesture_set_window:
 * @gesture: a #GtkGesture
 * @window: (allow-none): a #GdkWindow, or %NULL
 *
 * Sets a specific window to receive events about, so @gesture
 * will effectively handle only events targeting @window, or
 * a child of it. @window must pertain to gtk_event_controller_get_widget().
 *
 * Since: 3.14
 **/
void
gtk_gesture_set_window (GtkGesture *gesture,
                        GdkWindow  *window)
{
  GtkGesturePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE (gesture));
  g_return_if_fail (!window || GDK_IS_WINDOW (window));

  priv = gtk_gesture_get_instance_private (gesture);

  if (window)
    {
      GtkWidget *window_widget;

      gdk_window_get_user_data (window, (gpointer*) &window_widget);
      g_return_if_fail (window_widget !=
                        gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));
    }

  if (priv->user_window == window)
    return;

  priv->user_window = window;
  g_object_notify (G_OBJECT (gesture), "window");
}

gboolean
_gtk_gesture_handled_sequence_press (GtkGesture       *gesture,
                                     GdkEventSequence *sequence)
{
  GtkGesturePrivate *priv;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  data = g_hash_table_lookup (priv->points, sequence);

  if (!data)
    return FALSE;

  return data->press_handled;
}

gboolean
_gtk_gesture_get_pointer_emulating_sequence (GtkGesture        *gesture,
                                             GdkEventSequence **sequence)
{
  GtkGesturePrivate *priv;
  GdkEventSequence *seq;
  GHashTableIter iter;
  PointData *data;

  g_return_val_if_fail (GTK_IS_GESTURE (gesture), FALSE);

  priv = gtk_gesture_get_instance_private (gesture);
  g_hash_table_iter_init (&iter, priv->points);

  while (g_hash_table_iter_next (&iter, (gpointer*) &seq, (gpointer*) &data))
    {
      switch (data->event->type)
        {
        case GDK_TOUCH_BEGIN:
        case GDK_TOUCH_UPDATE:
        case GDK_TOUCH_END:
          if (!data->event->touch.emulating_pointer)
            continue;
          /* Fall through */
        case GDK_BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
        case GDK_MOTION_NOTIFY:
          *sequence = seq;
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}
