/* GTK - The GIMP Toolkit
 * Copyright (C) 2020, Red Hat, Inc.
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
 * SECTION:gtkeventcontrollerfocus
 * @Short_description: Event controller for focus
 * @Title: GtkEventControllerFocus
 * @See_also: #GtkEventController
 *
 * #GtkEventControllerFocus is an event controller meant for situations
 * where you need to know where the focusboard focus is.
 **/

#include "config.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkbindings.h"
#include "gtkenums.h"
#include "gtkmain.h"
#include "gtktypebuiltins.h"

#include <gdk/gdk.h>

struct _GtkEventControllerFocus
{
  GtkEventController parent_instance;

  const GtkCrossingData *current_crossing;

  guint is_focus       : 1;
  guint contains_focus : 1;
};

struct _GtkEventControllerFocusClass
{
  GtkEventControllerClass parent_class;
};

enum {
  ENTER,
  LEAVE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

enum {
  PROP_IS_FOCUS = 1,
  PROP_CONTAINS_FOCUS,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GtkEventControllerFocus, gtk_event_controller_focus,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_event_controller_focus_finalize (GObject *object)
{
  //GtkEventControllerFocus *focus = GTK_EVENT_CONTROLLER_FOCUS (object);

  G_OBJECT_CLASS (gtk_event_controller_focus_parent_class)->finalize (object);
}

static void
update_focus (GtkEventController    *controller,
              const GtkCrossingData *crossing)
{
  GtkEventControllerFocus *focus = GTK_EVENT_CONTROLLER_FOCUS (controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gboolean is_focus = FALSE;
  gboolean contains_focus = FALSE;
  gboolean enter = FALSE;
  gboolean leave = FALSE;

  if (crossing->direction == GTK_CROSSING_IN)
    {
      if (crossing->new_target == widget)
        is_focus = TRUE;
      if (crossing->new_target != NULL)
        contains_focus = TRUE;
    }

  if (focus->contains_focus != contains_focus)
    {
      if (contains_focus)
        enter = TRUE;
      else
        leave = TRUE;
    }

  if (enter)
    g_signal_emit (controller, signals[ENTER], 0);

  g_object_freeze_notify (G_OBJECT (focus));
  if (focus->is_focus != is_focus)
    {
      focus->is_focus = is_focus;
      g_object_notify (G_OBJECT (focus), "is-focus");
    }

  if (focus->contains_focus != contains_focus)
    {
      focus->contains_focus = contains_focus;
      g_object_notify (G_OBJECT (focus), "contains-focus");
    }
  g_object_thaw_notify (G_OBJECT (focus));

  if (leave)
    g_signal_emit (controller, signals[LEAVE], 0);
}

static void
gtk_event_controller_focus_handle_crossing (GtkEventController    *controller,
                                            const GtkCrossingData *crossing,
                                            double                 x,
                                            double                 y)
{
  GtkEventControllerFocus *focus = GTK_EVENT_CONTROLLER_FOCUS (controller);

  if (crossing->type != GTK_CROSSING_FOCUS)
    return;

  focus->current_crossing = crossing;

  update_focus (controller, crossing);

  focus->current_crossing = NULL;
}

static void
gtk_event_controller_focus_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GtkEventControllerFocus *controller = GTK_EVENT_CONTROLLER_FOCUS (object);

  switch (prop_id)
    {
    case PROP_IS_FOCUS:
      g_value_set_boolean (value, controller->is_focus);
      break;

    case PROP_CONTAINS_FOCUS:
      g_value_set_boolean (value, controller->contains_focus);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_focus_class_init (GtkEventControllerFocusClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_event_controller_focus_finalize;
  object_class->get_property = gtk_event_controller_focus_get_property;
  controller_class->handle_crossing = gtk_event_controller_focus_handle_crossing;

  /**
   * GtkEventControllerFocus:is-focus:
   *
   * Whether focus is in the controllers widget itself,
   * opposed to in a descendent widget. See also
   * #GtkEventControllerFocus:contains-focus.
   *
   * When handling focus events, this property is updated
   * before #GtkEventControllerFocus::focus-in or
   * #GtkEventControllerFocus::focus-out are emitted.
   */
  props[PROP_IS_FOCUS] =
      g_param_spec_boolean ("is-focus",
                            P_("Is Focus"),
                            P_("Whether the focus is in the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  /**
   * GtkEventControllerFocus:contains-focus:
   *
   * Whether focus is contain in the controllers widget. See
   * See #GtkEventControllerFocus:is-focus for whether the focus is in the widget itself
   * or inside a descendent.
   *
   * When handling focus events, this property is updated
   * before #GtkEventControllerFocus::focus-in or
   * #GtkEventControllerFocus::focus-out are emitted.
   */
  props[PROP_CONTAINS_FOCUS] =
      g_param_spec_boolean ("contains-focus",
                            P_("Contains Focus"),
                            P_("Whether the focus is in a descendant of the controllers widget"),
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  /**
   * GtkEventControllerFocus::enter:
   * @controller: the object which received the signal
   *
   * This signal is emitted whenever the focus enters into the
   * widget or one of its descendents.
   */
  signals[ENTER] =
    g_signal_new (I_("enter"),
                  GTK_TYPE_EVENT_CONTROLLER_FOCUS,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEventControllerFocus::leave:
   * @controller: the object which received the signal
   *
   * This signal is emitted whenever the focus leaves from 
   * the widget or one of its descendents.
   */
  signals[LEAVE] =
    g_signal_new (I_("leave"),
                  GTK_TYPE_EVENT_CONTROLLER_FOCUS,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_event_controller_focus_init (GtkEventControllerFocus *controller)
{
}

/**
 * gtk_event_controller_focus_new:
 *
 * Creates a new event controller that will handle focus events.
 *
 * Returns: a new #GtkEventControllerFocus
 **/
GtkEventController *
gtk_event_controller_focus_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_FOCUS, NULL);
}

/**
 * gtk_event_controller_focus_get_focus_origin:
 * @controller: a #GtkEventControllerFocus
 *
 * Returns the widget that was holding focus before.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerFocus::focus-in and #GtkEventControllerFocus::focus-out signals.
 *
 * Returns: (transfer none): the previous focus
 */
GtkWidget *
gtk_event_controller_focus_get_focus_origin (GtkEventControllerFocus *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (controller), NULL);
  g_return_val_if_fail (controller->current_crossing != NULL, NULL);

  return controller->current_crossing->old_target;
}

/**
 * gtk_event_controller_focus_get_focus_target:
 * @controller: a #GtkEventControllerFocus
 *
 * Returns the widget that will be holding focus afterwards.
 *
 * This function can only be used in handlers for the
 * #GtkEventControllerFocus::focus-in and #GtkEventControllerFocus::focus-out signals.
 *
 * Returns: (transfer none): the next focus
 */
GtkWidget *
gtk_event_controller_focus_get_focus_target (GtkEventControllerFocus *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (controller), NULL);
  g_return_val_if_fail (controller->current_crossing != NULL, NULL);

  return controller->current_crossing->new_target;
}

GtkWidget *
gtk_event_controller_focus_get_old_focus_child (GtkEventControllerFocus *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (controller), NULL);
  g_return_val_if_fail (controller->current_crossing != NULL, NULL);

  return controller->current_crossing->old_descendent;
}

GtkWidget *
gtk_event_controller_focus_get_new_focus_child (GtkEventControllerFocus *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (controller), NULL);
  g_return_val_if_fail (controller->current_crossing != NULL, NULL);

  return controller->current_crossing->new_descendent;
}

/**
 * gtk_event_controller_focus_contains_focus:
 * @self: a #GtkEventControllerFocus
 *
 * Returns the value of the GtkEventControllerFocus:contains-focus property.
 *
 * Returns: %TRUE if focus is within @self or one of its children
 */
gboolean
gtk_event_controller_focus_contains_focus (GtkEventControllerFocus *self)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (self), FALSE);

  return self->contains_focus;
}

/**
 * gtk_event_controller_focus_is_focus:
 * @self: a #GtkEventControllerFocus
 *
 * Returns the value of the GtkEventControllerFocus:is-focus property.
 *
 * Returns: %TRUE if focus is within @self but not one of its children
 */
gboolean
gtk_event_controller_focus_is_focus (GtkEventControllerFocus *self)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_FOCUS (self), FALSE);

  return self->is_focus;
}
