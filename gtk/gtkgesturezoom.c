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

/**
 * SECTION:gtkgesturezoom
 * @Short_description: Zoom gesture
 * @Title: GtkGestureZoom
 * @See_also: #GtkGestureRotate
 *
 * #GtkGestureZoom is a #GtkGesture implementation able to recognize
 * pinch/zoom gestures, whenever the distance between both tracked
 * sequences changes, the #GtkGestureZoom::scale-changed signal is
 * emitted to report the scale factor.
 */

#include "config.h"
#include <math.h>
#include "gtkgesturezoom.h"
#include "gtkgesturezoomprivate.h"

typedef struct _GtkGestureZoomPrivate GtkGestureZoomPrivate;

enum {
  SCALE_CHANGED,
  LAST_SIGNAL
};

struct _GtkGestureZoomPrivate
{
  gdouble initial_distance;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureZoom, gtk_gesture_zoom, GTK_TYPE_GESTURE)

static void
gtk_gesture_zoom_init (GtkGestureZoom *gesture)
{
}

static GObject *
gtk_gesture_zoom_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
  GObject *object;

  object = G_OBJECT_CLASS (gtk_gesture_zoom_parent_class)->constructor (type,
                                                                        n_construct_properties,
                                                                        construct_properties);
  g_object_set (object, "n-points", 2, NULL);

  return object;
}

static gboolean
_gtk_gesture_zoom_get_distance (GtkGestureZoom *zoom,
                                gdouble        *distance)
{
  gdouble x1, y1, x2, y2;
  GtkGesture *gesture;
  GList *sequences;
  gdouble dx, dy;

  gesture = GTK_GESTURE (zoom);

  if (!gtk_gesture_is_recognized (gesture))
    return FALSE;

  sequences = gtk_gesture_get_sequences (gesture);
  g_assert (sequences && sequences->next);

  gtk_gesture_get_point (gesture, sequences->data, &x1, &y1);
  gtk_gesture_get_point (gesture, sequences->next->data, &x2, &y2);
  g_list_free (sequences);

  dx = x1 - x2;
  dy = y1 - y2;;
  *distance = sqrt ((dx * dx) + (dy * dy));

  return TRUE;
}

static gboolean
_gtk_gesture_zoom_check_emit (GtkGestureZoom *gesture)
{
  GtkGestureZoomPrivate *priv;
  gdouble distance, zoom;

  if (!_gtk_gesture_zoom_get_distance (gesture, &distance))
    return FALSE;

  priv = gtk_gesture_zoom_get_instance_private (gesture);

  if (distance == 0 || priv->initial_distance == 0)
    return FALSE;

  zoom = distance / priv->initial_distance;
  g_signal_emit (gesture, signals[SCALE_CHANGED], 0, zoom);

  return TRUE;
}

static void
gtk_gesture_zoom_begin (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  GtkGestureZoom *zoom = GTK_GESTURE_ZOOM (gesture);
  GtkGestureZoomPrivate *priv;

  priv = gtk_gesture_zoom_get_instance_private (zoom);
  _gtk_gesture_zoom_get_distance (zoom, &priv->initial_distance);
}

static void
gtk_gesture_zoom_update (GtkGesture       *gesture,
                         GdkEventSequence *sequence)
{
  _gtk_gesture_zoom_check_emit (GTK_GESTURE_ZOOM (gesture));
}

static void
gtk_gesture_zoom_class_init (GtkGestureZoomClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->constructor = gtk_gesture_zoom_constructor;

  gesture_class->begin = gtk_gesture_zoom_begin;
  gesture_class->update = gtk_gesture_zoom_update;

  /**
   * GtkGestureZoom::scale-changed:
   * @controller: the object on which the signal is emitted
   * @scale: Scale delta, taking the initial state as 1:1
   *
   * This signal is emitted whenever the distance between both tracked
   * sequences changes.
   *
   * Since: 3.14
   */
  signals[SCALE_CHANGED] =
    g_signal_new ("scale-changed",
                  GTK_TYPE_GESTURE_ZOOM,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGestureZoomClass, scale_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

/**
 * gtk_gesture_zoom_new:
 * @widget: a #GtkWidget
 *
 * Returns a newly created #GtkGesture that recognizes zoom
 * in/out gestures (usually known as pinch/zoom).
 *
 * Returns: a newly created #GtkGestureZoom
 *
 * Since: 3.14
 **/
GtkGesture *
gtk_gesture_zoom_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_GESTURE_ZOOM,
                       "widget", widget,
                       NULL);
}

/**
 * gtk_gesture_zoom_get_scale_delta:
 * @gesture: a #GtkGestureZoom
 *
 * If @gesture is active, this function returns the zooming difference
 * since the gesture was recognized (hence the starting point is
 * considered 1:1). If @gesture is not active, 1 is returned.
 *
 * Returns: the scale delta
 *
 * Since: 3.14
 **/
gdouble
gtk_gesture_zoom_get_scale_delta (GtkGestureZoom *gesture)
{
  GtkGestureZoomPrivate *priv;
  gdouble distance;

  g_return_val_if_fail (GTK_IS_GESTURE_ZOOM (gesture), 1.0);

  if (!_gtk_gesture_zoom_get_distance (gesture, &distance))
    return 1.0;

  priv = gtk_gesture_zoom_get_instance_private (gesture);

  return distance / priv->initial_distance;
}
