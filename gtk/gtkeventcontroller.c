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
#include "gtkwidgetprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef struct _GtkEventControllerPrivate GtkEventControllerPrivate;

enum {
  PROP_WIDGET = 1,
  PROP_PROPAGATION_PHASE
};

struct _GtkEventControllerPrivate
{
  GtkWidget *widget;
  guint evmask;
  GtkPropagationPhase phase;
};

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
    case PROP_PROPAGATION_PHASE:
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (object),
                                                  g_value_get_enum (value));
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
    case PROP_PROPAGATION_PHASE:
      g_value_set_enum (value, priv->phase);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_constructed (GObject *object)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);
  if (priv->widget)
    _gtk_widget_add_controller (priv->widget, controller);
}

static void
gtk_event_controller_dispose (GObject *object)
{
  GtkEventController *controller = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);
  if (priv->widget)
    _gtk_widget_remove_controller (priv->widget, controller);
}

static void
gtk_event_controller_class_init (GtkEventControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->handle_event = gtk_event_controller_handle_event_default;

  object_class->set_property = gtk_event_controller_set_property;
  object_class->get_property = gtk_event_controller_get_property;
  object_class->constructed = gtk_event_controller_constructed;
  object_class->dispose = gtk_event_controller_dispose;

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
   * GtkEventController:propagation-phase:
   *
   * The propagation phase at which this controller will handle events.
   *
   * Since: 3.14
   */
  g_object_class_install_property (object_class,
                                   PROP_PROPAGATION_PHASE,
                                   g_param_spec_enum ("propagation-phase",
                                                      P_("Propagation phase"),
                                                      P_("Propagation phase at which this controller is run"),
                                                      GTK_TYPE_PROPAGATION_PHASE,
                                                      GTK_PHASE_BUBBLE,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_event_controller_init (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);
  priv->phase = GTK_PHASE_BUBBLE;
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
 *          controller action
 *
 * Since: 3.14
 **/
gboolean
gtk_event_controller_handle_event (GtkEventController *controller,
                                   const GdkEvent     *event)
{
  GtkEventControllerClass *controller_class;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  if (controller_class->handle_event)
    {
      g_object_ref (controller);
      retval = controller_class->handle_event (controller, event);
      g_object_unref (controller);
    }

  return retval;
}

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
}

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
 * Returns: (transfer none): a #GtkWidget
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
  GtkEventControllerClass *controller_class;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  if (controller_class->reset)
    controller_class->reset (controller);
}

/**
 * gtk_event_controller_get_propagation_phase:
 * @controller: a #GtkEventController
 *
 * Gets the propagation phase at which @controller handles events.
 *
 * Returns: the propagation phase
 *
 * Since: 3.14
 **/
GtkPropagationPhase
gtk_event_controller_get_propagation_phase (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), GTK_PHASE_NONE);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->phase;
}

/**
 * gtk_event_controller_set_propagation_phase:
 * @controller: a #GtkEventController
 * @phase: a propagation phase
 *
 * Sets the propagation phase at which a controller handles events.
 *
 * If @phase is %GTK_PHASE_NONE, no automatic event handling will be
 * performed, but other additional gesture maintenance will. In that phase,
 * the events can be managed by calling gtk_event_controller_handle_event().
 *
 * Since: 3.14
 **/
void
gtk_event_controller_set_propagation_phase (GtkEventController  *controller,
                                            GtkPropagationPhase  phase)
{
  GtkEventControllerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (phase >= GTK_PHASE_NONE && phase <= GTK_PHASE_TARGET);

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->phase == phase)
    return;

  priv->phase = phase;

  if (phase == GTK_PHASE_NONE)
    gtk_event_controller_reset (controller);

  g_object_notify (G_OBJECT (controller), "propagation-phase");
}
