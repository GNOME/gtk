/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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
#include "gtkgesturemultipress.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkGestureMultiPressPrivate GtkGestureMultiPressPrivate;

struct _GtkGestureMultiPressPrivate
{
  GdkRectangle rect;
  gdouble initial_press_x;
  gdouble initial_press_y;
  guint double_click_timeout_id;
  guint n_presses;
  guint button;
  guint current_button;
  guint rect_is_set : 1;
};

enum {
  PROP_BUTTON = 1
};

enum {
  PRESSED,
  STOPPED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureMultiPress, gtk_gesture_multi_press, GTK_TYPE_GESTURE)

static void
gtk_gesture_multi_press_finalize (GObject *object)
{
  GtkGestureMultiPressPrivate *priv;
  GtkGestureMultiPress *gesture;

  gesture = GTK_GESTURE_MULTI_PRESS (object);
  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (priv->double_click_timeout_id)
    {
      g_source_remove (priv->double_click_timeout_id);
      priv->double_click_timeout_id = 0;
    }

  G_OBJECT_CLASS (gtk_gesture_multi_press_parent_class)->finalize (object);
}

static void
gtk_gesture_multi_press_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkGestureMultiPress *gesture;

  gesture = GTK_GESTURE_MULTI_PRESS (object);

  switch (prop_id)
    {
    case PROP_BUTTON:
      gtk_gesture_multi_press_set_button (gesture,
                                          g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_gesture_multi_press_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkGestureMultiPressPrivate *priv;
  GtkGestureMultiPress *gesture;

  gesture = GTK_GESTURE_MULTI_PRESS (object);
  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_value_set_uint (value, priv->button);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_gesture_multi_press_check (GtkGesture *gesture)
{
  GtkGestureMultiPress *multi_press;
  GtkGestureMultiPressPrivate *priv;
  GList *sequences;
  gboolean active;

  multi_press = GTK_GESTURE_MULTI_PRESS (gesture);
  priv = gtk_gesture_multi_press_get_instance_private (multi_press);
  sequences = gtk_gesture_get_sequences (gesture);

  active = sequences || priv->double_click_timeout_id;
  g_list_free (sequences);

  return active;
}

static gboolean
_double_click_timeout_cb (gpointer user_data)
{
  GtkGestureMultiPress *gesture = user_data;
  GtkGestureMultiPressPrivate *priv;

  priv = gtk_gesture_multi_press_get_instance_private (gesture);
  priv->double_click_timeout_id = 0;

  gtk_gesture_multi_press_reset (gesture);

  return FALSE;
}

static void
_gtk_gesture_multi_press_update_timeout (GtkGestureMultiPress *gesture)
{
  GtkGestureMultiPressPrivate *priv;
  guint double_click_time;
  GtkSettings *settings;
  GtkWidget *widget;

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (priv->double_click_timeout_id)
    g_source_remove (priv->double_click_timeout_id);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  settings = gtk_widget_get_settings (widget);
  g_object_get (settings, "gtk-double-click-time", &double_click_time, NULL);

  priv->double_click_timeout_id =
    gdk_threads_add_timeout (double_click_time,
                             _double_click_timeout_cb,
                             gesture);
}

static gboolean
_gtk_gesture_multi_press_check_within_threshold (GtkGestureMultiPress *gesture,
                                                 gdouble               x,
                                                 gdouble               y)
{
  GtkGestureMultiPressPrivate *priv;
  guint double_click_distance;
  GtkSettings *settings;
  GtkWidget *widget;

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (priv->n_presses == 0)
    return TRUE;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  settings = gtk_widget_get_settings (widget);
  g_object_get (settings,
                "gtk-double-click-distance", &double_click_distance,
                NULL);

  if (ABS (priv->initial_press_x - x) < double_click_distance &&
      ABS (priv->initial_press_y - y) < double_click_distance)
    {
      if (!priv->rect_is_set ||
          (x >= priv->rect.x && x < priv->rect.x + priv->rect.width &&
           y >= priv->rect.y && y < priv->rect.y + priv->rect.height))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_gesture_multi_press_update (GtkGesture       *gesture,
                                GdkEventSequence *sequence)
{
  GtkGestureMultiPress *multi_press;
  GtkGestureMultiPressPrivate *priv;
  guint n_presses, button = 1;
  const GdkEvent *event;
  gdouble x, y;

  multi_press = GTK_GESTURE_MULTI_PRESS (gesture);
  priv = gtk_gesture_multi_press_get_instance_private (multi_press);
  event = gtk_gesture_get_last_event (gesture, sequence);

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      button = event->button.button;
      /* Fall through */
    case GDK_TOUCH_BEGIN:
      /* Ignore buttons we don't care about */
      if (priv->button != 0 && button != priv->button)
        break;

      /* Reset the gesture if the button number changes mid-recognition */
      if (priv->n_presses > 0 &&
          priv->current_button != button)
        gtk_gesture_multi_press_reset (multi_press);

      priv->current_button = button;
      _gtk_gesture_multi_press_update_timeout (multi_press);
      gtk_gesture_get_point (gesture, sequence, &x, &y);

      if (!_gtk_gesture_multi_press_check_within_threshold (multi_press, x, y))
        gtk_gesture_multi_press_reset (multi_press);

      /* Increment later the real counter, just if the gesture is
       * reset on the pressed handler */
      n_presses = priv->n_presses + 1;

      g_signal_emit (gesture, signals[PRESSED], 0, n_presses, TRUE, x, y);

      if (priv->n_presses == 0)
        {
          priv->initial_press_x = x;
          priv->initial_press_y = y;
        }

      priv->n_presses++;
      break;
    default:
      break;
    }
}

static void
gtk_gesture_multi_press_class_init (GtkGestureMultiPressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->finalize = gtk_gesture_multi_press_finalize;
  object_class->set_property = gtk_gesture_multi_press_set_property;
  object_class->get_property = gtk_gesture_multi_press_get_property;

  gesture_class->check = gtk_gesture_multi_press_check;
  gesture_class->begin = gtk_gesture_multi_press_update;
  gesture_class->update = gtk_gesture_multi_press_update;

  g_object_class_install_property (object_class,
                                   PROP_BUTTON,
                                   g_param_spec_uint ("button",
                                                      P_("Button number"),
                                                      P_("Button number to listen to"),
                                                      0, G_MAXUINT, 0,
                                                      GTK_PARAM_READWRITE));
  signals[PRESSED] =
    g_signal_new ("pressed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureMultiPressClass, pressed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_BOOLEAN,
                  G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  signals[STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureMultiPressClass, stopped),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_gesture_multi_press_init (GtkGestureMultiPress *gesture)
{
}

/**
 * gtk_gesture_multi_press_new:
 * @widget: a #GtkWidget
 *
 * Returns a newly created #GtkGesture that recognizes single and multiple
 * presses.
 *
 * Returns: a newly created #GtkGestureMultiPress
 *
 * Since: 3.14
 **/
GtkGesture *
gtk_gesture_multi_press_new (GtkWidget *widget)
{
  return g_object_new (GTK_TYPE_GESTURE_MULTI_PRESS,
                       "widget", widget,
                       NULL);
}

/**
 * gtk_gesture_multi_press_reset:
 * @gesture: a #GtkGestureMultiPress
 *
 * Resets the gesture, so the press count is reset to 0.
 *
 * Note: If this happens within a #GtkGestureMultiPress::pressed handler,
 * the button press being currently handled will still increment the press
 * counter, so a subsequent press would count as the second press.
 *
 * Since: 3.14
 **/
void
gtk_gesture_multi_press_reset (GtkGestureMultiPress *gesture)
{
  GtkGestureMultiPressPrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE (gesture));

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (priv->n_presses != 0)
    g_signal_emit (gesture, signals[STOPPED], 0);

  priv->current_button = 0;
  priv->n_presses = 0;
  gtk_gesture_check (GTK_GESTURE (gesture));
}

/**
 * gtk_gesture_multi_press_set_button:
 * @gesture: a #GtkGestureMultiPress
 * @button: button number to listen to, or 0 for any button
 *
 * Sets the button number @gesture listens to. If non-0, every
 * button press from a different button number will be ignored.
 * Touch events implicitly match with button 1.
 *
 * Since: 3.14
 **/
void
gtk_gesture_multi_press_set_button (GtkGestureMultiPress *gesture,
                                    guint                 button)
{
  GtkGestureMultiPressPrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (priv->button == button)
    return;

  priv->button = button;
  g_object_notify (G_OBJECT (gesture), "button");
}

/**
 * gtk_gesture_multi_press_get_button:
 * @gesture: a #GtkgestureMultiPress
 *
 * Returns the button number @gesture listens for, or 0 if @gesture
 * reacts to any button press.
 *
 * Returns: The button number, or 0 for any button.
 *
 * Since: 3.14
 **/
guint
gtk_gesture_multi_press_get_button (GtkGestureMultiPress *gesture)
{
  GtkGestureMultiPressPrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_MULTI_PRESS (gesture), 0);

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  return priv->button;
}

/**
 * gtk_gesture_multi_press_set_area:
 * @gesture: a #GtkGesture
 * @rect: (allow-none): rectangle to receive coordinates on.
 *
 * If @rect is non-#NULL, the press area will be checked to be
 * confined within the rectangle, otherwise the button count
 * will be reset so the press is seen as being the first one.
 * If @rect is #NULL, the area will be reset to an unrestricted
 * state.
 *
 * Note: The rectangle is only used to determine whether any
 * non-first click falls within the expected area. This is not
 * akin to an input shape.
 *
 * Since: 3.14
 **/
void
gtk_gesture_multi_press_set_area (GtkGestureMultiPress *gesture,
                                  const GdkRectangle   *rect)
{
  GtkGestureMultiPressPrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_MULTI_PRESS (gesture));
  g_return_if_fail (rect != NULL);

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (!rect)
    priv->rect_is_set = FALSE;
  else
    {
      priv->rect_is_set = TRUE;
      priv->rect = *rect;
    }
}

/**
 * gtk_gesture_multi_press_get_area:
 * @gesture: a #GtkGestureMultiPress
 * @rect: (out): return location for the press area
 *
 * If an area was set through gtk_gesture_multi_press_set_area(),
 * this function will return #TRUE and fill in @rect with the
 * press area. See gtk_gesture_multi_press_set_area() for more
 * details on what the press area represents.
 *
 * Returns: 3.14
 **/
gboolean
gtk_gesture_multi_press_get_area (GtkGestureMultiPress *gesture,
                                  GdkRectangle         *rect)
{
  GtkGestureMultiPressPrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_MULTI_PRESS (gesture), FALSE);

  priv = gtk_gesture_multi_press_get_instance_private (gesture);

  if (rect)
    {
      if (priv->rect_is_set)
        *rect = priv->rect;
      else
        {
          rect->x = rect->y = G_MININT;
          rect->width = rect->height = G_MAXINT;
        }
    }

  return priv->rect_is_set;
}
