/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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
 * Author(s): Matthias Clasen <mclasen@redhat.com>
 */

/**
 * SECTION:gtkeventcontrollermotion
 * @Short_description: Event controller for motion events
 * @Title: GtkEventControllerMotion
 * @See_also: #GtkEventController
 *
 * #GtkEventControllerMotion is an event controller meant for situations
 * where you need to track the position of the pointer.
 **/
#include "config.h"

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"

struct _GtkEventControllerMotion
{
  GtkEventController parent_instance;

  const GdkEvent *current_event;

  guint is_pointer_focus       : 1;
  guint contains_pointer_focus : 1;
};

struct _GtkEventControllerMotionClass
{
  GtkEventControllerClass parent_class;
};

enum {
  ENTER,
  LEAVE,
  MOTION,
  N_SIGNALS
};

enum {
  PROP_IS_POINTER_FOCUS = 1,
  PROP_CONTAINS_POINTER_FOCUS,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkEventControllerMotion, gtk_event_controller_motion, GTK_TYPE_EVENT_CONTROLLER)

static void
update_pointer_focus (GtkEventControllerMotion *motion,
                      gboolean                  enter,
                      GdkNotifyType             detail)
{
  gboolean is_pointer;
  gboolean contains_pointer;

  switch (detail)
    {
    case GDK_NOTIFY_VIRTUAL:
    case GDK_NOTIFY_NONLINEAR_VIRTUAL:
      is_pointer = FALSE;
      contains_pointer = enter;
      break;
    case GDK_NOTIFY_ANCESTOR:
    case GDK_NOTIFY_NONLINEAR:
      is_pointer = enter;
      contains_pointer = FALSE;
      break;
    case GDK_NOTIFY_INFERIOR:
      is_pointer = enter;
      contains_pointer = !enter;
      break;
    case GDK_NOTIFY_UNKNOWN:
    default:
      g_warning ("Unknown crossing detail");
      return;
    }

  g_object_freeze_notify (G_OBJECT (motion));
  if (motion->is_pointer_focus != is_pointer)
    {
      motion->is_pointer_focus = is_pointer;
      g_object_notify (G_OBJECT (motion), "is-pointer-focus");
    }
  if (motion->contains_pointer_focus != contains_pointer)
    {
      motion->contains_pointer_focus = contains_pointer;
      g_object_notify (G_OBJECT (motion), "contains-pointer-focus");
    }
  g_object_thaw_notify (G_OBJECT (motion));
}

static gboolean
gtk_event_controller_motion_handle_event (GtkEventController *controller,
                                          const GdkEvent     *event)
{
  GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION (controller);
  GtkEventControllerClass *parent_class;
  GdkEventType type;

  type = gdk_event_get_event_type (event);
  if (type == GDK_ENTER_NOTIFY)
    {
      double x, y;
      GdkCrossingMode mode;
      GdkNotifyType detail;

      gdk_event_get_coords (event, &x, &y);
      gdk_event_get_crossing_mode (event, &mode);
      gdk_event_get_crossing_detail (event, &detail);

      update_pointer_focus (motion, TRUE, detail);

      motion->current_event = event;

      g_signal_emit (controller, signals[ENTER], 0, x, y, mode, detail);

      motion->current_event = NULL;
    }
  else if (type == GDK_LEAVE_NOTIFY)
    {
      GdkCrossingMode mode;
      GdkNotifyType detail;

      gdk_event_get_crossing_mode (event, &mode);
      gdk_event_get_crossing_detail (event, &detail);

      update_pointer_focus (motion, FALSE, detail);

      motion->current_event = event;

      g_signal_emit (controller, signals[LEAVE], 0, mode, detail);

      motion->current_event = NULL;
    }
  else if (type == GDK_MOTION_NOTIFY)
    {
      double x, y;

      gdk_event_get_coords (event, &x, &y);

      g_signal_emit (controller, signals[MOTION], 0, x, y);
    }

  parent_class = GTK_EVENT_CONTROLLER_CLASS (gtk_event_controller_motion_parent_class);

  return parent_class->handle_event (controller, event);
}

static void
gtk_event_controller_motion_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkEventControllerMotion *controller = GTK_EVENT_CONTROLLER_MOTION (object);

  switch (prop_id)
    {
    case PROP_IS_POINTER_FOCUS:
      g_value_set_boolean (value, controller->is_pointer_focus);
      break;

    case PROP_CONTAINS_POINTER_FOCUS:
      g_value_set_boolean (value, controller->contains_pointer_focus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_motion_class_init (GtkEventControllerMotionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_event_controller_motion_get_property;

  controller_class->handle_event = gtk_event_controller_motion_handle_event;

  /**
   * GtkEventControllerMotion:is-pointer-focus:
   *
   * Whether the pointer is in the controllers widget itself,
   * as opposed to in a descendent widget. See
   * #GtkEventControllerMotion:contains-pointer-focus.
   *
   * When handling crossing events, this property is updated
   * before #GtkEventControllerMotion::enter or
   * #GtkEventControllerMotion::leave are emitted.
   */
  props[PROP_IS_POINTER_FOCUS] =
      g_param_spec_boolean ("is-pointer-focus",
                            P_("Is Pointer Focus"),
                            P_("Whether the pointer is in the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  /**
   * GtkEventControllerMotion:contains-pointer-focus:
   *
   * Whether the pointer is in a descendant of the controllers widget.
   * See #GtkEventControllerMotion:is-pointer-focus.
   *
   * When handling crossing events, this property is updated
   * before #GtkEventControllerMotion::enter or
   * #GtkEventControllerMotion::leave are emitted.
   */
  props[PROP_CONTAINS_POINTER_FOCUS] =
      g_param_spec_boolean ("contains-pointer-focus",
                            P_("Contains Pointer Focus"),
                            P_("Whether the pointer is in a descendant of the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  /**
   * GtkEventControllerMotion::enter:
   * @controller: The object that received the signal
   * @x: the x coordinate
   * @y: the y coordinate
   * @crossing_mode: the crossing mode of this event
   * @notify_type: the kind of crossing event
   *
   * Signals that the pointer has entered the widget.
   */
  signals[ENTER] =
    g_signal_new (I_("enter"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE_ENUM_ENUM,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  GDK_TYPE_CROSSING_MODE,
                  GDK_TYPE_NOTIFY_TYPE);
  g_signal_set_va_marshaller (signals[ENTER],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLE_ENUM_ENUMv);

  /**
   * GtkEventControllerMotion::leave:
   * @controller: The object that received the signal
   * @crossing_mode: the crossing mode of this event
   * @notify_type: the kind of crossing event
   *
   * Signals that pointer has left the widget.
   */
  signals[LEAVE] =
    g_signal_new (I_("leave"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__ENUM_ENUM,
                  G_TYPE_NONE,
                  2,
                  GDK_TYPE_CROSSING_MODE,
                  GDK_TYPE_NOTIFY_TYPE);
  g_signal_set_va_marshaller (signals[LEAVE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__ENUM_ENUMv);

  /**
   * GtkEventControllerMotion::motion:
   * @controller: The object that received the signal
   * @x: the x coordinate
   * @y: the y coordinate
   *
   * Emitted when the pointer moves inside the widget.
   */
  signals[MOTION] =
    g_signal_new (I_("motion"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[MOTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_event_controller_motion_init (GtkEventControllerMotion *motion)
{
}

/**
 * gtk_event_controller_motion_new:
 *
 * Creates a new event controller that will handle motion events.
 *
 * Returns: a new #GtkEventControllerMotion
 **/
GtkEventController *
gtk_event_controller_motion_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_MOTION,
                       NULL);
}

/**
 * gtk_event_controller_motion_get_pointer_origin:
 * @controller: a #GtkEventControllerMotion
 *
 * Returns the widget that contained the pointer before.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerMotion::enter and
 * #GtkEventControllerMotion::leave signals.
 *
 * Returns: (transfer none): the previous pointer focus
 */
GtkWidget *
gtk_event_controller_motion_get_pointer_origin (GtkEventControllerMotion *controller)
{
  GtkWidget *origin;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_MOTION (controller), NULL);
  g_return_val_if_fail (controller->current_event != NULL, NULL);

  if (gdk_event_get_event_type (controller->current_event) == GDK_ENTER_NOTIFY)
    origin = (GtkWidget *)gdk_event_get_related_target (controller->current_event);
  else
    origin = (GtkWidget *)gdk_event_get_target (controller->current_event);

  return origin;
}

/**
 * gtk_event_controller_motion_get_pointer_target:
 * @controller: a #GtkEventControllerMotion
 *
 * Returns the widget that will contain the pointer afterwards.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerMotion::enter and
 * #GtkEventControllerMotion::leave signals.
 *
 * Returns: (transfer none): the next pointer focus
 */
GtkWidget *
gtk_event_controller_motion_get_pointer_target (GtkEventControllerMotion *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_MOTION (controller), NULL);
  g_return_val_if_fail (controller->current_event != NULL, NULL);

  if (gdk_event_get_event_type (controller->current_event) == GDK_ENTER_NOTIFY)
    return (GtkWidget *)gdk_event_get_target (controller->current_event);
  else
    return (GtkWidget *)gdk_event_get_related_target (controller->current_event);
}

