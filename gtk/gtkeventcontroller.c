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
#include "gtknative.h"

typedef struct _GtkEventControllerPrivate GtkEventControllerPrivate;

enum {
  PROP_WIDGET = 1,
  PROP_PROPAGATION_PHASE,
  PROP_PROPAGATION_LIMIT,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

struct _GtkEventControllerPrivate
{
  GtkWidget *widget;
  GtkPropagationPhase phase;
  GtkPropagationLimit limit;
  char *name;
  GtkWidget *target;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkEventController, gtk_event_controller, G_TYPE_OBJECT)

static void
gtk_event_controller_set_widget (GtkEventController *self,
                                 GtkWidget          *widget)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  priv->widget = widget;
}

static void
gtk_event_controller_unset_widget (GtkEventController *self)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  priv->widget = NULL;
}

static gboolean
gtk_event_controller_filter_event_default (GtkEventController *self,
                                           const GdkEvent     *event)
{
  return FALSE;
}

static gboolean
gtk_event_controller_handle_event_default (GtkEventController *self,
                                           const GdkEvent     *event,
                                           double              x,
                                           double              y)
{
  return FALSE;
}

static void
gtk_event_controller_handle_crossing_default (GtkEventController    *self,
                                              const GtkCrossingData *crossing,
                                              double                 x,
                                              double                 y)
{
}

static void
gtk_event_controller_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PROPAGATION_PHASE:
      gtk_event_controller_set_propagation_phase (self,
                                                  g_value_get_enum (value));
      break;
    case PROP_PROPAGATION_LIMIT:
      gtk_event_controller_set_propagation_limit (self,
                                                  g_value_get_enum (value));
      break;
    case PROP_NAME:
      g_free (priv->name);
      priv->name = g_value_dup_string (value);
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
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    case PROP_PROPAGATION_PHASE:
      g_value_set_enum (value, priv->phase);
      break;
    case PROP_PROPAGATION_LIMIT:
      g_value_set_enum (value, priv->limit);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_finalize (GObject *object)
{
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);
  
  g_free (priv->name);

  G_OBJECT_CLASS (gtk_event_controller_parent_class)->finalize (object);
}

static void
gtk_event_controller_class_init (GtkEventControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->set_widget = gtk_event_controller_set_widget;
  klass->unset_widget = gtk_event_controller_unset_widget;
  klass->filter_event = gtk_event_controller_filter_event_default;
  klass->handle_event = gtk_event_controller_handle_event_default;
  klass->handle_crossing = gtk_event_controller_handle_crossing_default;

  object_class->finalize = gtk_event_controller_finalize;
  object_class->set_property = gtk_event_controller_set_property;
  object_class->get_property = gtk_event_controller_get_property;

  /**
   * GtkEventController:widget:
   *
   * The widget receiving the #GdkEvents that the controller will handle.
   */
  properties[PROP_WIDGET] =
      g_param_spec_object ("widget",
                           P_("Widget"),
                           P_("Widget the gesture relates to"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READABLE);

  /**
   * GtkEventController:propagation-phase:
   *
   * The propagation phase at which this controller will handle events.
   */
  properties[PROP_PROPAGATION_PHASE] =
      g_param_spec_enum ("propagation-phase",
                         P_("Propagation phase"),
                         P_("Propagation phase at which this controller is run"),
                         GTK_TYPE_PROPAGATION_PHASE,
                         GTK_PHASE_BUBBLE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEventController:propagation-limit:
   *
   * The limit for which events this controller will handle.
   */
  properties[PROP_PROPAGATION_LIMIT] =
      g_param_spec_enum ("propagation-limit",
                         P_("Propagation limit"),
                         P_("Propagation limit for events handled by this controller"),
                         GTK_TYPE_PROPAGATION_LIMIT,
                         GTK_LIMIT_SAME_NATIVE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_NAME] =
      g_param_spec_string ("name",
                           P_("Name"),
                           P_("Name for this controller"),
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_event_controller_init (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);
  priv->phase = GTK_PHASE_BUBBLE;
  priv->limit = GTK_LIMIT_SAME_NATIVE;
}

static gboolean
same_native (GtkWidget *widget,
             GtkWidget *target)
{
  GtkWidget *native;
  GtkWidget *native2;

  if (!widget || !target)
    return TRUE;

  native = GTK_WIDGET (gtk_widget_get_native (widget));
  native2 = GTK_WIDGET (gtk_widget_get_native (widget));

  return native == native2;
}

static gboolean
gtk_event_controller_filter_event (GtkEventController *controller,
                                   const GdkEvent     *event,
                                   GtkWidget          *target)
{
  GtkEventControllerPrivate *priv;
  GtkEventControllerClass *controller_class;

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->widget && !gtk_widget_is_sensitive (priv->widget))
    return TRUE;

  if (priv->limit == GTK_LIMIT_SAME_NATIVE &&
      !same_native (priv->widget, target))
    return TRUE;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  return controller_class->filter_event (controller, event);
}

static gboolean
gtk_event_controller_filter_crossing (GtkEventController    *controller,
                                      const GtkCrossingData *data)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->widget && !gtk_widget_is_sensitive (priv->widget))
    return TRUE;

  if (priv->limit == GTK_LIMIT_SAME_NATIVE &&
      (!same_native (priv->widget, data->old_target) ||
       !same_native (priv->widget, data->new_target)))
    return TRUE;

  return FALSE;
}

/**
 * gtk_event_controller_handle_event:
 * @controller: a #GtkEventController
 * @event: a #GdkEvent
 * @target: the target widget
 * @x: event position in widget coordinates, or 0 if not a pointer event
 * @y: event position in widget coordinates, or 0 if not a pointer event
 *
 * Feeds an event into @controller, so it can be interpreted
 * and the controller actions triggered.
 *
 * Returns: %TRUE if the event was potentially useful to trigger the
 *          controller action
 **/
gboolean
gtk_event_controller_handle_event (GtkEventController *controller,
                                   const GdkEvent     *event,
                                   GtkWidget          *target,
                                   double              x,
                                   double              y)
{
  GtkEventControllerClass *controller_class;
  GtkEventControllerPrivate *priv;
  gboolean retval = FALSE;

  priv = gtk_event_controller_get_instance_private (controller);

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_event_controller_filter_event (controller, event, target))
    return retval;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  priv->target = target;

  g_object_ref (controller);
  retval = controller_class->handle_event (controller, event, x, y);
  g_object_unref (controller);

  priv->target = NULL;

  return retval;
}

/**
 * gtk_event_controller_handle_crossing:
 * @controller: a #GtkEventController
 * @crossing: a #GtkCrossingData
 * @x: translated event coordinates
 * @y: translated event coordinates
 *
 * Feeds a crossing event into @controller, so it can be interpreted
 * and the controller actions triggered.
 **/
void
gtk_event_controller_handle_crossing (GtkEventController    *controller,
                                      const GtkCrossingData *crossing,
                                      double                 x,
                                      double                 y)
{
  GtkEventControllerClass *controller_class;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (crossing != NULL);

  if (gtk_event_controller_filter_crossing (controller, crossing))
    return;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  g_object_ref (controller);
  controller_class->handle_crossing (controller, crossing, x, y);
  g_object_unref (controller);
}

/**
 * gtk_event_controller_get_widget:
 * @controller: a #GtkEventController
 *
 * Returns the #GtkWidget this controller relates to.
 *
 * Returns: (transfer none): a #GtkWidget
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
 * the controller did through gtk_event_controll_handle_event()
 * will be dropped at this point.
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

  g_object_notify_by_pspec (G_OBJECT (controller), properties[PROP_PROPAGATION_PHASE]);
}

GtkPropagationLimit
gtk_event_controller_get_propagation_limit (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), GTK_LIMIT_SAME_NATIVE);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->limit;
}
void
gtk_event_controller_set_propagation_limit (GtkEventController  *controller,
                                            GtkPropagationLimit  limit)
{
  GtkEventControllerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->limit == limit)
    return;

  priv->limit = limit;

  g_object_notify_by_pspec (G_OBJECT (controller), properties[PROP_PROPAGATION_LIMIT]);
}

const char *
gtk_event_controller_get_name (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), NULL);

  return priv->name;
}

void
gtk_event_controller_set_name (GtkEventController *controller,
                               const char         *name)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  g_free (priv->name);
  priv->name = g_strdup (name);
}

GtkWidget *
gtk_event_controller_get_target (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  return priv->target;
}

static GtkCrossingData *
gtk_crossing_data_copy (GtkCrossingData *crossing)
{
  GtkCrossingData *copy;

  copy = g_new (GtkCrossingData, 1);

  copy->type = crossing->type;
  copy->direction = crossing->direction;

  if (crossing->old_target)
    copy->old_target = g_object_ref (crossing->old_target);
  if (crossing->new_target)
    copy->new_target = g_object_ref (crossing->new_target);

  return copy;
}

static void
gtk_crossing_data_free (GtkCrossingData *crossing)
{
  g_clear_object (&crossing->old_target);
  g_clear_object (&crossing->new_target);

  g_free (crossing);
}

G_DEFINE_BOXED_TYPE (GtkCrossingData, gtk_crossing_data,
                     gtk_crossing_data_copy, gtk_crossing_data_free)
