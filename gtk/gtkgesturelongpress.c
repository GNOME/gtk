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
 * SECTION:gtkgesturelongpress
 * @Short_description: "Press and Hold" gesture
 * @Title: GtkGestureLongPress
 *
 * #GtkGestureLongPress is a #GtkGesture implementation able to recognize
 * long presses, triggering the #GtkGestureLongPress::pressed after the
 * timeout is exceeded.
 *
 * If the touchpoint is lifted before the timeout passes, or if it drifts
 * too far of the initial press point, the #GtkGestureLongPress::cancelled
 * signal will be emitted.
 */

#include "config.h"
#include "gtkgesturelongpress.h"
#include "gtkgesturelongpressprivate.h"
#include "gtkgestureprivate.h"
#include "gtkmarshalers.h"
#include "gtkdnd.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkGestureLongPressPrivate GtkGestureLongPressPrivate;

enum {
  PRESSED,
  CANCELLED,
  N_SIGNALS
};

enum {
  PROP_DELAY_FACTOR = 1
};

struct _GtkGestureLongPressPrivate
{
  gdouble initial_x;
  gdouble initial_y;

  gdouble delay_factor;
  guint timeout_id;
  guint delay;
  guint cancelled : 1;
  guint triggered : 1;
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureLongPress, gtk_gesture_long_press, GTK_TYPE_GESTURE_SINGLE)

static void
gtk_gesture_long_press_init (GtkGestureLongPress *gesture)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));
  priv->delay_factor = 1.0;
}

static gboolean
gtk_gesture_long_press_check (GtkGesture *gesture)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));

  if (priv->cancelled)
    return FALSE;

  return GTK_GESTURE_CLASS (gtk_gesture_long_press_parent_class)->check (gesture);
}

static gboolean
_gtk_gesture_long_press_timeout (gpointer user_data)
{
  GtkGestureLongPress *gesture = user_data;
  GtkGestureLongPressPrivate *priv;
  GdkEventSequence *sequence;
  gdouble x, y;

  priv = gtk_gesture_long_press_get_instance_private (gesture);
  sequence = gtk_gesture_get_last_updated_sequence (GTK_GESTURE (gesture));
  gtk_gesture_get_point (GTK_GESTURE (gesture), sequence, &x, &y);

  priv->timeout_id = 0;
  priv->triggered = TRUE;
  g_signal_emit (gesture, signals[PRESSED], 0, x, y);

  return FALSE;
}

static void
gtk_gesture_long_press_begin (GtkGesture       *gesture,
                              GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;
  const GdkEvent *event;
  GtkWidget *widget;
  gint delay;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (gesture, sequence);

  if (!event ||
      (event->type != GDK_BUTTON_PRESS &&
       event->type != GDK_TOUCH_BEGIN))
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  g_object_get (gtk_widget_get_settings (widget),
                "gtk-long-press-time", &delay,
                NULL);

  delay = (gint)(priv->delay_factor * delay);

  gtk_gesture_get_point (gesture, sequence,
                         &priv->initial_x, &priv->initial_y);
  priv->timeout_id =
    gdk_threads_add_timeout (delay,
                             _gtk_gesture_long_press_timeout,
                             gesture);
}

static void
gtk_gesture_long_press_update (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;
  GtkWidget *widget;
  gdouble x, y;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));
  gtk_gesture_get_point (gesture, sequence, &x, &y);

  if (gtk_drag_check_threshold (widget, priv->initial_x, priv->initial_y, x, y))
    {
      if (priv->timeout_id)
        {
          g_source_remove (priv->timeout_id);
          priv->timeout_id = 0;
          g_signal_emit (gesture, signals[CANCELLED], 0);
        }

      priv->cancelled = TRUE;
      _gtk_gesture_check (gesture);
    }
}

static void
gtk_gesture_long_press_end (GtkGesture       *gesture,
                            GdkEventSequence *sequence)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (gesture));

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
      g_signal_emit (gesture, signals[CANCELLED], 0);
    }

  priv->cancelled = priv->triggered = FALSE;
}

static void
gtk_gesture_long_press_cancel (GtkGesture       *gesture,
                               GdkEventSequence *sequence)
{
  gtk_gesture_long_press_end (gesture, sequence);
  GTK_GESTURE_CLASS (gtk_gesture_long_press_parent_class)->cancel (gesture, sequence);
}

static void
gtk_gesture_long_press_sequence_state_changed (GtkGesture            *gesture,
                                               GdkEventSequence      *sequence,
                                               GtkEventSequenceState  state)
{
  if (state == GTK_EVENT_SEQUENCE_DENIED)
    gtk_gesture_long_press_end (gesture, sequence);
}

static void
gtk_gesture_long_press_finalize (GObject *object)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (object));

  if (priv->timeout_id)
    g_source_remove (priv->timeout_id);

  G_OBJECT_CLASS (gtk_gesture_long_press_parent_class)->finalize (object);
}

static void
gtk_gesture_long_press_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (object));

  switch (property_id)
    {
    case PROP_DELAY_FACTOR:
      g_value_set_double (value, priv->delay_factor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_gesture_long_press_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkGestureLongPressPrivate *priv;

  priv = gtk_gesture_long_press_get_instance_private (GTK_GESTURE_LONG_PRESS (object));

  switch (property_id)
    {
    case PROP_DELAY_FACTOR:
      priv->delay_factor = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_gesture_long_press_class_init (GtkGestureLongPressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);

  object_class->finalize = gtk_gesture_long_press_finalize;
  object_class->get_property = gtk_gesture_long_press_get_property;
  object_class->set_property = gtk_gesture_long_press_set_property;

  gesture_class->check = gtk_gesture_long_press_check;
  gesture_class->begin = gtk_gesture_long_press_begin;
  gesture_class->update = gtk_gesture_long_press_update;
  gesture_class->end = gtk_gesture_long_press_end;
  gesture_class->cancel = gtk_gesture_long_press_cancel;
  gesture_class->sequence_state_changed = gtk_gesture_long_press_sequence_state_changed;

  g_object_class_install_property (object_class,
                                   PROP_DELAY_FACTOR,
                                   g_param_spec_double ("delay-factor",
                                                        P_("Delay factor"),
                                                        P_("Factor by which to modify the default timeout"),
                                                        0.5, 2.0, 1.0,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkGestureLongPress::pressed:
   * @gesture: the object which received the signal
   * @x: the X coordinate where the press happened, relative to the widget allocation
   * @y: the Y coordinate where the press happened, relative to the widget allocation
   *
   * This signal is emitted whenever a press goes unmoved/unreleased longer than
   * what the GTK+ defaults tell.
   *
   * Since: 3.14
   */
  signals[PRESSED] =
    g_signal_new ("pressed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureLongPressClass, pressed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  /**
   * GtkGestureLongPress::cancelled:
   * @gesture: the object which received the signal
   *
   * This signal is emitted whenever a press moved too far, or was released
   * before #GtkGestureLongPress:pressed happened.
   *
   * Since: 3.14
   */
  signals[CANCELLED] =
    g_signal_new ("cancelled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkGestureLongPressClass, cancelled),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

/**
 * gtk_gesture_long_press_new:
 * @widget: a #GtkWidget
 *
 * Returns a newly created #GtkGesture that recognizes long presses.
 *
 * Returns: a newly created #GtkGestureLongPress
 *
 * Since: 3.14
 **/
GtkGesture *
gtk_gesture_long_press_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_new (GTK_TYPE_GESTURE_LONG_PRESS,
                       "widget", widget,
                       NULL);
}
