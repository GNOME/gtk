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

#include "config.h"

#include "gtkpinchpangesture.h"

#include "gtksequencetrackerprivate.h"
#include "gtkpinchpanrecognizer.h"

#include <math.h>

/**
 * SECTION:gtkpinchpangesture
 * @Short_description: Gesture for pinch, pan and rotation
 * @Title: GtkPinchPanGesture
 * @See_also: #GtkPinchPanRecognizer
 *
 * The #GtkPinchPanGesture object is used to to track pinch, pan and rotation
 * gestures. These are usually used to implement scrolling, zooming and rotation
 * respectively on scrollable widgets.
 *
 * #GtkPinchPanGesture was added in GTK 3.6.
 */


enum {
  PROP_0,
};

struct _GtkPinchPanGesturePrivate {
  GtkSequenceTracker *sequence[2];
  double              initial_distance;
  double              initial_angle;
};

G_DEFINE_TYPE (GtkPinchPanGesture, gtk_pinch_pan_gesture, GTK_TYPE_GESTURE)

static void
gtk_pinch_pan_gesture_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  //GtkPinchPanGesture *gesture = GTK_PINCH_PAN_GESTURE (object);
  //GtkPinchPanGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_pinch_pan_gesture_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  //GtkPinchPanGesture *gesture = GTK_PINCH_PAN_GESTURE (object);
  //GtkPinchPanGesturePrivate *priv = gesture->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_pinch_pan_gesture_dispose (GObject *object)
{
  //GtkPinchPanGesture *gesture = GTK_PINCH_PAN_GESTURE (object);

  G_OBJECT_CLASS (gtk_pinch_pan_gesture_parent_class)->dispose (object);
}

static void
gtk_pinch_pan_gesture_class_init (GtkPinchPanGestureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_pinch_pan_gesture_dispose;
  object_class->set_property = gtk_pinch_pan_gesture_set_property;
  object_class->get_property = gtk_pinch_pan_gesture_get_property;

  g_type_class_add_private (object_class, sizeof (GtkPinchPanGesturePrivate));
}

static void
gtk_pinch_pan_gesture_init (GtkPinchPanGesture *gesture)
{
  gesture->priv = G_TYPE_INSTANCE_GET_PRIVATE (gesture,
                                               GTK_TYPE_PINCH_PAN_GESTURE,
                                               GtkPinchPanGesturePrivate);
}

gboolean
_gtk_pinch_pan_gesture_begin (GtkPinchPanGesture *gesture,
                              GdkEvent           *event)
{
  GtkPinchPanGesturePrivate *priv = gesture->priv;

  if (priv->sequence[1] != NULL)
    return FALSE;

  if (priv->sequence[0] == NULL)
    {
      priv->sequence[0] = _gtk_sequence_tracker_new (event);
      gtk_gesture_add_sequence (GTK_GESTURE (gesture), event->touch.sequence);
    }
  else
    {
      priv->sequence[1] = _gtk_sequence_tracker_new (event);
      gtk_gesture_add_sequence (GTK_GESTURE (gesture), event->touch.sequence);
    }

  if (priv->sequence[1])
    {
      double x, y;
      _gtk_sequence_tracker_compute_distance (priv->sequence[0],
                                              priv->sequence[1],
                                              &x, &y);
      priv->initial_distance = sqrt (x * x + y * y);
      priv->initial_angle = atan2 (y, x);
      gtk_event_tracker_start (GTK_EVENT_TRACKER (gesture));
    }

  return FALSE;
}

gboolean
gtk_pinch_pan_gesture_update_for_event (GtkPinchPanGesture *gesture,
                                        GdkEvent           *event)
{
  GtkPinchPanGesturePrivate *priv = gesture->priv;
  gboolean result = FALSE;
  guint i;

  for (i = 0; i < 2; i++)
    {
      if (priv->sequence[i])
        result |= _gtk_sequence_tracker_update (priv->sequence[i], event);
    }
  return result;
}

gboolean
_gtk_pinch_pan_gesture_update (GtkPinchPanGesture *gesture,
                               GdkEvent           *event)
{
  if (gtk_pinch_pan_gesture_update_for_event (gesture, event) &&
      gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)))
    gtk_event_tracker_update (GTK_EVENT_TRACKER (gesture));

  return FALSE;
}

gboolean
_gtk_pinch_pan_gesture_end (GtkPinchPanGesture *gesture,
                            GdkEvent           *event)
{
  if (gtk_pinch_pan_gesture_update_for_event (gesture, event))
    {
      if (gesture->priv->sequence[1])
        gtk_event_tracker_finish (GTK_EVENT_TRACKER (gesture));
      else
        gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));
    }

  return FALSE;
}

gboolean
_gtk_pinch_pan_gesture_cancel (GtkPinchPanGesture *gesture,
                               GdkEvent           *event)
{
  if (gtk_pinch_pan_gesture_update_for_event (gesture, event))
    gtk_event_tracker_cancel (GTK_EVENT_TRACKER (gesture));

  return FALSE;
}

void
gtk_pinch_pan_gesture_get_offset (GtkPinchPanGesture *gesture,
                              double          *x,
                              double          *y)
{
  GtkPinchPanGesturePrivate *priv;

  g_return_if_fail (GTK_IS_PINCH_PAN_GESTURE (gesture));
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    {
      if (x)
        *x = 0;
      if (y)
        *y = 0;

      return;
    }

  if (x)
    *x = (_gtk_sequence_tracker_get_x_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_x_offset (priv->sequence[1])) / 2;
  if (y)
    *y = (_gtk_sequence_tracker_get_y_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_y_offset (priv->sequence[1])) / 2;
}

double
gtk_pinch_pan_gesture_get_x_offset (GtkPinchPanGesture *gesture)
{
  GtkPinchPanGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_PINCH_PAN_GESTURE (gesture), 0.0);
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    return 0;

  return (_gtk_sequence_tracker_get_x_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_x_offset (priv->sequence[1])) / 2;
}

double
gtk_pinch_pan_gesture_get_y_offset (GtkPinchPanGesture *gesture)
{
  GtkPinchPanGesturePrivate *priv;

  g_return_val_if_fail (GTK_IS_PINCH_PAN_GESTURE (gesture), 0.0);
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    return 0;

  return (_gtk_sequence_tracker_get_y_offset (priv->sequence[0])
          + _gtk_sequence_tracker_get_y_offset (priv->sequence[1])) / 2;
}

double
gtk_pinch_pan_gesture_get_rotation (GtkPinchPanGesture *gesture)
{
  GtkPinchPanGesturePrivate *priv;
  double x, y, angle;

  g_return_val_if_fail (GTK_IS_PINCH_PAN_GESTURE (gesture), 0.0);
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    return 0;

  _gtk_sequence_tracker_compute_distance (priv->sequence[0],
                                          priv->sequence[1],
                                          &x, &y);
  angle = atan2 (y, x);
  angle -= priv->initial_angle;
  angle *= 180 / G_PI;
  angle = fmod (angle + 360, 360);
  
  return angle;
}

double
gtk_pinch_pan_gesture_get_zoom (GtkPinchPanGesture *gesture)
{
  GtkPinchPanGesturePrivate *priv;
  double x, y;

  g_return_val_if_fail (GTK_IS_PINCH_PAN_GESTURE (gesture), 1.0);
  
  priv = gesture->priv;

  if (!gtk_event_tracker_is_started (GTK_EVENT_TRACKER (gesture)) ||
      gtk_event_tracker_is_cancelled (GTK_EVENT_TRACKER (gesture)))
    return 1;

  _gtk_sequence_tracker_compute_distance (priv->sequence[0],
                                          priv->sequence[1],
                                          &x, &y);
  
  return sqrt (x * x + y * y) / priv->initial_distance;
}
