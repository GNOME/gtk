/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
 * Copyright (C) 2019, Yariv Barkan
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
 */

/**
 * SECTION:gtkgesturetranslate
 * @Short_description: Translate gesture
 * @Title: GtkGestureTranslate
 * @See_also: #GtkGestureRotate, #GtkGestureZoom
 *
 * #GtkGestureTranslate is a #GtkGesture implementation able to recognize
 * pinch/translate gestures, whenever the distance between both tracked
 * sequences changes, the #GtkGestureTranslate::offset-changed signal is
 * emitted to report the current translation.
 */

#include "config.h"
#include "gtkgesturetranslate.h"
#include "gtkgesturetranslateprivate.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"

typedef struct _GtkGestureTranslatePrivate GtkGestureTranslatePrivate;

enum {
  OFFSET_CHANGED,
  LAST_SIGNAL
};

struct _GtkGestureTranslatePrivate
{
  gdouble start_x;
  gdouble start_y;

  gdouble accum_x;
  gdouble accum_y;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureTranslate, gtk_gesture_translate, GTK_TYPE_GESTURE)

static void
gtk_gesture_translate_init (GtkGestureTranslate *gesture)
{
}

static GObject *
gtk_gesture_translate_constructor (GType                  type,
                                   guint                  n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
  GObject *object;

  object = G_OBJECT_CLASS (gtk_gesture_translate_parent_class)->constructor (type,
                                                                             n_construct_properties,
                                                                             construct_properties);
  g_object_set (object, "n-points", 2, NULL);

  return object;
}

static gboolean
_gtk_gesture_translate_get_offset (GtkGestureTranslate *translate,
                                   gdouble             *x_offset,
                                   gdouble             *y_offset)
{
  GtkGestureTranslatePrivate *priv;
  const GdkEvent *last_event;
  gdouble x1, y1, x2, y2, center_x, center_y;
  GtkGesture *gesture;
  GList *sequences = NULL;
  GdkTouchpadGesturePhase phase;
  gboolean retval = FALSE;

  gesture = GTK_GESTURE (translate);
  priv = gtk_gesture_translate_get_instance_private (translate);

  if (!gtk_gesture_is_recognized (gesture))
    return FALSE;

  sequences = gtk_gesture_get_sequences (gesture);
  if (!sequences)
    return FALSE;

  last_event = gtk_gesture_get_last_event (gesture, sequences->data);
  gdk_event_get_touchpad_gesture_phase (last_event, &phase);

  if (gdk_event_get_event_type (last_event) == GDK_TOUCHPAD_PINCH &&
      (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN ||
       phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE ||
       phase == GDK_TOUCHPAD_GESTURE_PHASE_END))
    {
      /* Touchpad pinch */
      *x_offset = priv->accum_x;
      *y_offset = priv->accum_y;
    }
  else
    {
      if (!sequences->next)
        goto out;

      gtk_gesture_get_point (gesture, sequences->data, &x1, &y1);
      gtk_gesture_get_point (gesture, sequences->next->data, &x2, &y2);

      center_x = (x1 + x2) / 2;
      center_y = (y1 + y2) / 2;

      *x_offset = center_x - priv->start_x;
      *y_offset = center_y - priv->start_y;
    }

  retval = TRUE;

 out:
  g_list_free (sequences);
  return retval;
}

static gboolean
_gtk_gesture_translate_check_emit (GtkGestureTranslate *gesture)
{
  gdouble x_offset, y_offset;

  if (!_gtk_gesture_translate_get_offset (gesture, &x_offset, &y_offset))
    return FALSE;

  g_signal_emit (gesture, signals[OFFSET_CHANGED], 0, x_offset, y_offset);

  return TRUE;
}

static gboolean
gtk_gesture_translate_filter_event (GtkEventController *controller,
                                    const GdkEvent     *event)
{
  /* Let 2-finger touchpad pinch events go through */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH)
    {
      guint n_fingers;

      gdk_event_get_touchpad_gesture_n_fingers (event, &n_fingers);

      if (n_fingers == 2)
        return FALSE;
      else
        return TRUE;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_translate_parent_class)->filter_event (controller, event);
}

static void
gtk_gesture_translate_begin (GtkGesture       *gesture,
                             GdkEventSequence *sequence)
{
  GtkGestureTranslate *translate = GTK_GESTURE_TRANSLATE (gesture);
  GtkGestureTranslatePrivate *priv;

  priv = gtk_gesture_translate_get_instance_private (translate);
  gtk_gesture_get_point (gesture, sequence, &priv->start_x, &priv->start_y);
}

static void
gtk_gesture_translate_update (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  _gtk_gesture_translate_check_emit (GTK_GESTURE_TRANSLATE (gesture));
}

static gboolean
gtk_gesture_translate_handle_event (GtkEventController *controller,
                                    const GdkEvent     *event)
{
  GtkGestureTranslate *translate = GTK_GESTURE_TRANSLATE (controller);
  GtkGestureTranslatePrivate *priv;
  GdkTouchpadGesturePhase phase;
  double dx;
  double dy;

  priv = gtk_gesture_translate_get_instance_private (translate);

  gdk_event_get_touchpad_gesture_phase (event, &phase);
  gdk_event_get_touchpad_deltas (event, &dx, &dy);

  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_PINCH)
    {
      if (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN ||
          phase == GDK_TOUCHPAD_GESTURE_PHASE_END)
        {
          priv->accum_x = 0;
          priv->accum_y = 0;
        }
      else if (phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE)
        {
          priv->accum_x += dx;
          priv->accum_y += dy;
        }
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_translate_parent_class)->handle_event (controller, event);
}

static void
gtk_gesture_translate_class_init (GtkGestureTranslateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *event_controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->constructor = gtk_gesture_translate_constructor;

  event_controller_class->filter_event = gtk_gesture_translate_filter_event;
  event_controller_class->handle_event = gtk_gesture_translate_handle_event;

  gesture_class->begin = gtk_gesture_translate_begin;
  gesture_class->update = gtk_gesture_translate_update;

  /**
   * GtkGestureTranslate::offset-changed:
   * @controller: the object on which the signal is emitted
   * @x_offset: offset from the initial position on the x axis
   * @y_offset: offset from the initial position on the y axis
   *
   * This signal is emitted whenever the center position between both tracked
   * sequences changes.
   */
  signals[OFFSET_CHANGED] =
    g_signal_new (I_("offset-changed"),
                  GTK_TYPE_GESTURE_TRANSLATE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkGestureTranslateClass, offset_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[OFFSET_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

/**
 * gtk_gesture_translate_new:
 *
 * Returns a newly created #GtkGesture that recognizes translate
 * in/out gestures (usually known as pinch/translate).
 *
 * Returns: a newly created #GtkGestureTranslate
 **/
GtkGesture *
gtk_gesture_translate_new (void)
{
  return g_object_new (GTK_TYPE_GESTURE_TRANSLATE,
                       NULL);
}

/**
 * gtk_gesture_translate_get_start:
 * @gesture: a #GtkGesture
 * @x: (out) (nullable): X coordinate for the pinch translate start point
 * @y: (out) (nullable): Y coordinate for the pinch translate start point
 *
 * If the @gesture is active, this function returns %TRUE
 * and fills in @x and @y with the pinch translate start coordinates,
 * in window-relative coordinates.
 *
 * Returns: %TRUE if the gesture is active
 **/
gboolean
gtk_gesture_translate_get_start (GtkGestureTranslate *gesture,
                                 gdouble             *x,
                                 gdouble             *y)
{
  GtkGestureTranslatePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_TRANSLATE (gesture), FALSE);

  priv = gtk_gesture_translate_get_instance_private (gesture);

  if (x)
    *x = priv->start_x;

  if (y)
    *y = priv->start_y;

  return TRUE;
}

/**
 * gtk_gesture_translate_get_offset:
 * @gesture: a #GtkGestureTranslate
 * @x_offset: (out) (nullable): X offset from pinch translate start point
 * @y_offset: (out) (nullable): Y offset from pinch translate start point
 *
 * If @gesture is active, this function fills in @x_offset and @y_offset
 * with the translation since the gesture was recognized (hence the starting
 * point is considered 0,0).
 *
 * Returns: %TRUE if the gesture is active
 **/
gboolean
gtk_gesture_translate_get_offset (GtkGestureTranslate *gesture,
                                  gdouble             *x_offset,
                                  gdouble             *y_offset)
{
  gboolean retval;
  gdouble _x_offset;
  gdouble _y_offset;

  g_return_val_if_fail (GTK_IS_GESTURE_TRANSLATE (gesture), FALSE);

  retval = _gtk_gesture_translate_get_offset (gesture, &_x_offset, &_y_offset);

  if (x_offset)
    *x_offset = _x_offset;

  if (y_offset)
    *y_offset = _y_offset;

  return retval;
}
