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
 * SECTION:gtkeventcontroller
 * @Short_description: Self-contained handler of series of events
 * @Title: GtkEventController
 * @See_also: #GtkGesture
 *
 * #GtkEventController is a base, low-level implementation for event
 * controllers. Those react to a series of #GdkEvents, and possibly trigger
 * actions as a consequence of those.
 */

#include "config.h"
#include "gtkeventcontroller.h"
#include "gtkeventcontrollerprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkEventControllerPrivate GtkEventControllerPrivate;

enum {
  PROP_WIDGET = 1,
  PROP_EVENT_MASK
};

enum {
  HANDLE_EVENT,
  RESET,
  N_SIGNALS
};

struct _GtkEventControllerPrivate
{
  GtkWidget *widget;
  guint evmask;
};

guint signals[N_SIGNALS] = { 0 };

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkEventController, gtk_event_controller, G_TYPE_OBJECT)

static gboolean
gtk_event_controller_handle_event_default (GtkEventController *controller,
                                           const GdkEvent     *event)
{
  return FALSE;
}

static void
gtk_event_controller_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (GTK_EVENT_CONTROLLER (object));

  switch (prop_id)
    {
    case PROP_WIDGET:
      priv->widget = g_value_get_object (value);
      break;
    case PROP_EVENT_MASK:
      priv->evmask = g_value_get_flags (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (GTK_EVENT_CONTROLLER (object));

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    case PROP_EVENT_MASK:
      g_value_set_flags (value, priv->evmask);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
gtk_event_controller_class_init (GtkEventControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->handle_event = gtk_event_controller_handle_event_default;

  object_class->set_property = gtk_event_controller_set_property;
  object_class->get_property = gtk_event_controller_get_property;

  /**
   * GtkEventController:widget:
   *
   * The widget receiving the #GdkEvents that the controller will handle.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_WIDGET,
                                   g_param_spec_object ("widget",
                                                        P_("Widget"),
                                                        P_("Widget the gesture relates to"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  /**
   * GtkEventController:event-mask:
   *
   * Set of events that the controller handles.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_EVENT_MASK,
                                   g_param_spec_flags ("event-mask",
                                                       P_("Event mask"),
                                                       P_("Event mask the controller handles"),
                                                       GDK_TYPE_EVENT_MASK, 0,
                                                       GTK_PARAM_READWRITE));
  /**
   * GtkEventController::handle-event:
   * @controller: the object which receives the signal
   * @event: the event to handle
   *
   * This signal is emitted on @controller whenever an event is to be handled.
   *
   * Return value: %TRUE to propagate further emission if the event was handled,
   *   %FALSE otherwise
   *
   * Since: 3.14
   */
  signals[HANDLE_EVENT] =
    g_signal_new ("handle-event",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkEventControllerClass, handle_event),
                  g_signal_accumulator_true_handled, NULL, NULL,
                  G_TYPE_BOOLEAN, 1,
                  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  /**
   * GtkEventController::reset:
   * @controller: the object which receives the signal
   *
   * This signal is emitted on @controller whenever it needs to be reset. When
   * this happens controllers must forget any recorded state.
   *
   * Since: 3.14
   */
  signals[RESET] =
    g_signal_new ("reset",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkEventControllerClass, reset),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_event_controller_init (GtkEventController *controller)
{
}

/**
 * gtk_event_controller_handle_event:
 * @controller: a #GtkEventController
 * @event: a #GdkEvent
 *
 * Feeds an events into @controller, so it can be interpreted
 * and the controller actions triggered.
 *
 * Returns: %TRUE if the event was potentially useful to trigger the
 *          controller action.
 *
 * Since: 3.14
 **/
gboolean
gtk_event_controller_handle_event (GtkEventController *controller,
                                   const GdkEvent     *event)
{
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  g_signal_emit (controller, signals[HANDLE_EVENT], 0, event, &retval);

  return retval;
}

/**
 * gtk_event_controller_set_event_mask:
 * @controller: a #GtkEventController
 * @event_mask: mask for the events the controller handles
 *
 * Sets the event mask that the controller handles. This is only
 * meant for #GtkEventController implementations and should not be
 * called in applications.
 *
 * Since: 3.14
 **/
void
gtk_event_controller_set_event_mask (GtkEventController *controller,
                                     GdkEventMask        event_mask)
{
  GtkEventControllerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->evmask == event_mask)
    return;

  priv->evmask = event_mask;
  g_object_notify (G_OBJECT (controller), "event-mask");
}

/**
 * gtk_event_controller_get_event_mask:
 * @controller: a #GtkEventController
 *
 * Returns the event mask necessary for the events handled by @controller.
 *
 * Returns: the controller event mask.
 *
 * Since: 3.14
 **/
GdkEventMask
gtk_event_controller_get_event_mask (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), 0);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->evmask;
}

/**
 * gtk_event_controller_get_widget:
 * @controller: a #GtkEventController
 *
 * Returns the #GtkWidget this controller relates to.
 *
 * Returns: a #GtkWidget
 *
 * Since: 3.14
 **/
GtkWidget *
gtk_event_controller_get_widget (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), 0);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->widget;
}

/**
 * gtk_event_controller_reset:
 * @controller: a #GtkEventController
 *
 * Resets the @controller to a clean state. Every interaction
 * the controller did through #GtkEventController::handle-event
 * will be dropped at this point.
 *
 * Since: 3.14
 **/
void
gtk_event_controller_reset (GtkEventController *controller)
{
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  g_signal_emit (controller, signals[RESET], 0);
}
