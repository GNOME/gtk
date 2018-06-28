/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010  Intel Corporation
 * Copyright (C) 2010  RedHat, Inc.
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
 * Author:
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * Based on similar code from Mx.
 */

/**
 * SECTION:gtkswitch
 * @Short_Description: A “light switch” style toggle
 * @Title: GtkSwitch
 * @See_Also: #GtkToggleButton
 *
 * #GtkSwitch is a widget that has two states: on or off. The user can control
 * which state should be active by clicking the empty area, or by dragging the
 * handle.
 *
 * GtkSwitch can also handle situations where the underlying state changes with
 * a delay. See #GtkSwitch::state-set for details.
 *
 * # CSS nodes
 *
 * |[<!-- language="plain" -->
 * switch
 * ├── label
 * ├── label
 * ╰── slider
 * ]|
 *
 * GtkSwitch has four css nodes, the main node with the name switch and subnodes
 * for the slider and the on and off labels. Neither of them is using any style classes.
 */

#include "config.h"

#include "gtkswitch.h"

#include "gtkactionable.h"
#include "gtkactionhelperprivate.h"
#include "gtkgesturemultipress.h"
#include "gtkgesturepan.h"
#include "gtkgesturesingle.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"
#include "gtkwidgetprivate.h"

#include "a11y/gtkswitchaccessible.h"

#include "fallback-c89.c"

struct _GtkSwitchPrivate
{
  GtkActionHelper *action_helper;

  GtkGesture *pan_gesture;
  GtkGesture *multipress_gesture;

  double handle_pos;
  guint tick_id;
  GtkProgressTracker tracker;

  guint state                 : 1;
  guint is_active             : 1;

  GtkWidget *on_label;
  GtkWidget *off_label;
  GtkWidget *slider;
};
typedef struct _GtkSwitchPrivate GtkSwitchPrivate;

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_STATE,
  LAST_PROP,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET
};

enum
{
  ACTIVATE,
  STATE_SET,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *switch_props[LAST_PROP] = { NULL, };

static void gtk_switch_actionable_iface_init (GtkActionableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSwitch, gtk_switch, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSwitch)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                                gtk_switch_actionable_iface_init))

static void
gtk_switch_end_toggle_animation (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (sw), priv->tick_id);
      priv->tick_id = 0;
    }
}

static gboolean
gtk_switch_on_frame_clock_update (GtkWidget     *widget,
                                  GdkFrameClock *clock,
                                  gpointer       user_data)
{
  GtkSwitch *sw = GTK_SWITCH (widget);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  gtk_progress_tracker_advance_frame (&priv->tracker,
                                      gdk_frame_clock_get_frame_time (clock));

  if (gtk_progress_tracker_get_state (&priv->tracker) != GTK_PROGRESS_STATE_AFTER)
    {
      if (priv->is_active)
        priv->handle_pos = 1.0 - gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE);
      else
        priv->handle_pos = gtk_progress_tracker_get_ease_out_cubic (&priv->tracker, FALSE);
    }
  else
    {
      gtk_switch_set_active (sw, !priv->is_active);
    }

  gtk_widget_queue_allocate (GTK_WIDGET (sw));

  return G_SOURCE_CONTINUE;
}

#define ANIMATION_DURATION 100

static void
gtk_switch_begin_toggle_animation (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  if (gtk_settings_get_enable_animations (gtk_widget_get_settings (GTK_WIDGET (sw))))
    {
      gtk_progress_tracker_start (&priv->tracker, 1000 * ANIMATION_DURATION, 0, 1.0);
      if (priv->tick_id == 0)
        priv->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (sw),
                                                      gtk_switch_on_frame_clock_update,
                                                      NULL, NULL);
    }
  else
    {
      gtk_switch_set_active (sw, !priv->is_active);
    }
}

static void
gtk_switch_multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                                       gint                  n_press,
                                       gdouble               x,
                                       gdouble               y,
                                       GtkSwitch            *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);
  graphene_rect_t switch_bounds;

  gtk_widget_compute_bounds (GTK_WIDGET (sw), GTK_WIDGET (sw), &switch_bounds);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  /* If the press didn't happen in the draggable handle,
   * cancel the pan gesture right away
   */
  if ((priv->is_active && x <= switch_bounds.size.width / 2.0) ||
      (!priv->is_active && x > switch_bounds.size.width / 2.0))
    gtk_gesture_set_state (priv->pan_gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_switch_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                        gint                  n_press,
                                        gdouble               x,
                                        gdouble               y,
                                        GtkSwitch            *sw)
{
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_widget_contains (GTK_WIDGET (sw), x, y) &&
      gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    gtk_switch_begin_toggle_animation (sw);
}

static void
gtk_switch_pan_gesture_pan (GtkGesturePan   *gesture,
                            GtkPanDirection  direction,
                            gdouble          offset,
                            GtkSwitch       *sw)
{
  GtkWidget *widget = GTK_WIDGET (sw);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);
  int width;

  width = gtk_widget_get_width (widget);

  if (direction == GTK_PAN_DIRECTION_LEFT)
    offset = -offset;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  if (priv->is_active)
    offset += width / 2;
  
  offset /= width / 2;
  /* constrain the handle within the trough width */
  priv->handle_pos = CLAMP (offset, 0, 1.0);

  /* we need to redraw the handle */
  gtk_widget_queue_allocate (widget);
}

static void
gtk_switch_pan_gesture_drag_end (GtkGestureDrag *gesture,
                                 gdouble         x,
                                 gdouble         y,
                                 GtkSwitch      *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);
  GdkEventSequence *sequence;
  gboolean active;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_gesture_get_sequence_state (GTK_GESTURE (gesture), sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
    {
      /* if half the handle passed the middle of the switch, then we
       * consider it to be on
       */
      active = priv->handle_pos >= 0.5;
    }
  else if (!gtk_gesture_handles_sequence (priv->multipress_gesture, sequence))
    active = priv->is_active;
  else
    return;

  priv->handle_pos = active ? 1.0 : 0.0;
  gtk_switch_set_active (sw, active);
  gtk_widget_queue_allocate (GTK_WIDGET (sw));
}

static void
gtk_switch_activate (GtkSwitch *sw)
{
  gtk_switch_begin_toggle_animation (sw);
}

static void
gtk_switch_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GtkSwitch *self = GTK_SWITCH (widget);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (self);
  gint slider_minimum, slider_natural;
  int on_nat, off_nat;

  gtk_widget_measure (priv->slider, orientation, -1,
                      &slider_minimum, &slider_natural,
                      NULL, NULL);

  gtk_widget_measure (priv->on_label, orientation, for_size, NULL, &on_nat, NULL, NULL);
  gtk_widget_measure (priv->off_label, orientation, for_size, NULL, &off_nat, NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int text_width = MAX (on_nat, off_nat);
      *minimum = 2 * MAX (slider_minimum, text_width);
      *natural = 2 * MAX (slider_natural, text_width);
    }
  else
    {
      int text_height = MAX (on_nat, off_nat);
      *minimum = MAX (slider_minimum, text_height);
      *natural = MAX (slider_natural, text_height);
    }
}

static void
gtk_switch_size_allocate (GtkWidget           *widget,
                          const GtkAllocation *allocation,
                          int                  baseline)
{
  GtkSwitch *self = GTK_SWITCH (widget);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (self);
  GtkAllocation child_alloc;
  GtkAllocation slider_alloc;
  int min;

  slider_alloc.x = round (priv->handle_pos * (allocation->width / 2));
  slider_alloc.y = 0;
  slider_alloc.width = allocation->width / 2;
  slider_alloc.height = allocation->height;

  gtk_widget_size_allocate (priv->slider, &slider_alloc, -1);


  /* Center ON label in left half */
  gtk_widget_measure (priv->on_label, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  child_alloc.x = ((allocation->width / 2) - min) / 2;
  child_alloc.width = min;
  gtk_widget_measure (priv->on_label, GTK_ORIENTATION_VERTICAL, min, &min, NULL, NULL, NULL);
  child_alloc.y = (allocation->height - min) / 2;
  child_alloc.height = min;
  gtk_widget_size_allocate (priv->on_label, &child_alloc, -1);

  /* Center OFF label in right half */
  gtk_widget_measure (priv->off_label, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);
  child_alloc.x = (allocation->width / 2) + ((allocation->width / 2) - min) / 2;
  child_alloc.width = min;
  gtk_widget_measure (priv->off_label, GTK_ORIENTATION_VERTICAL, min, &min, NULL, NULL, NULL);
  child_alloc.y = (allocation->height - min) / 2;
  child_alloc.height = min;
  gtk_widget_size_allocate (priv->off_label, &child_alloc, -1);
}

static void
gtk_switch_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (priv->action_helper, action_name);
}

static void
gtk_switch_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  if (!priv->action_helper)
    priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (priv->action_helper, action_target);
}

static const gchar *
gtk_switch_get_action_name (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  return gtk_action_helper_get_action_name (priv->action_helper);
}

static GVariant *
gtk_switch_get_action_target_value (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  return gtk_action_helper_get_action_target_value (priv->action_helper);
}

static void
gtk_switch_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_switch_get_action_name;
  iface->set_action_name = gtk_switch_set_action_name;
  iface->get_action_target_value = gtk_switch_get_action_target_value;
  iface->set_action_target_value = gtk_switch_set_action_target_value;
}

static void
gtk_switch_set_property (GObject      *gobject,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkSwitch *sw = GTK_SWITCH (gobject);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_switch_set_active (sw, g_value_get_boolean (value));
      break;

    case PROP_STATE:
      gtk_switch_set_state (sw, g_value_get_boolean (value));
      break;

    case PROP_ACTION_NAME:
      gtk_switch_set_action_name (GTK_ACTIONABLE (sw), g_value_get_string (value));
      break;

    case PROP_ACTION_TARGET:
      gtk_switch_set_action_target_value (GTK_ACTIONABLE (sw), g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_switch_get_property (GObject    *gobject,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (GTK_SWITCH (gobject));

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->is_active);
      break;

    case PROP_STATE:
      g_value_set_boolean (value, priv->state);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (priv->action_helper));
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (priv->action_helper));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_switch_dispose (GObject *object)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (GTK_SWITCH (object));

  g_clear_object (&priv->action_helper);

  G_OBJECT_CLASS (gtk_switch_parent_class)->dispose (object);
}

static void
gtk_switch_finalize (GObject *object)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (GTK_SWITCH (object));

  gtk_switch_end_toggle_animation (GTK_SWITCH (object));

  gtk_widget_unparent (priv->on_label);
  gtk_widget_unparent (priv->off_label);
  gtk_widget_unparent (priv->slider);

  G_OBJECT_CLASS (gtk_switch_parent_class)->finalize (object);
}

static gboolean
state_set (GtkSwitch *sw, gboolean state)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);

  gtk_switch_set_state (sw, state);

  return TRUE;
}

static void
gtk_switch_class_init (GtkSwitchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  /**
   * GtkSwitch:active:
   *
   * Whether the #GtkSwitch widget is in its on or off state.
   */
  switch_props[PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          P_("Active"),
                          P_("Whether the switch is on or off"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSwitch:state:
   *
   * The backend state that is controlled by the switch. 
   * See #GtkSwitch::state-set for details.
   */
  switch_props[PROP_STATE] =
    g_param_spec_boolean ("state",
                          P_("State"),
                          P_("The backend state"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  gobject_class->set_property = gtk_switch_set_property;
  gobject_class->get_property = gtk_switch_get_property;
  gobject_class->dispose = gtk_switch_dispose;
  gobject_class->finalize = gtk_switch_finalize;

  g_object_class_install_properties (gobject_class, LAST_PROP, switch_props);

  widget_class->measure = gtk_switch_measure;
  widget_class->size_allocate = gtk_switch_size_allocate;

  klass->activate = gtk_switch_activate;
  klass->state_set = state_set;

  /**
   * GtkSwitch::activate:
   * @widget: the object which received the signal.
   *
   * The ::activate signal on GtkSwitch is an action signal and
   * emitting it causes the switch to animate.
   * Applications should never connect to this signal, but use the
   * notify::active signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSwitchClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
  widget_class->activate_signal = signals[ACTIVATE];

  /**
   * GtkSwitch::state-set:
   * @widget: the object on which the signal was emitted
   * @state: the new state of the switch
   *
   * The ::state-set signal on GtkSwitch is emitted to change the underlying
   * state. It is emitted when the user changes the switch position. The
   * default handler keeps the state in sync with the #GtkSwitch:active
   * property.
   *
   * To implement delayed state change, applications can connect to this signal,
   * initiate the change of the underlying state, and call gtk_switch_set_state()
   * when the underlying state change is complete. The signal handler should
   * return %TRUE to prevent the default handler from running.
   *
   * Visually, the underlying state is represented by the trough color of
   * the switch, while the #GtkSwitch:active property is represented by the
   * position of the switch.
   *
   * Returns: %TRUE to stop the signal emission
   */
  signals[STATE_SET] =
    g_signal_new (I_("state-set"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSwitchClass, state_set),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_BOOLEAN);

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SWITCH_ACCESSIBLE);
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOGGLE_BUTTON);

  gtk_widget_class_set_css_name (widget_class, I_("switch"));
}

static void
gtk_switch_init (GtkSwitch *self)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (self);
  GtkGesture *gesture;

  gtk_widget_set_has_surface (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

  gesture = gtk_gesture_multi_press_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_switch_multipress_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_switch_multipress_gesture_released), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
  priv->multipress_gesture = gesture;

  gesture = gtk_gesture_pan_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pan",
                    G_CALLBACK (gtk_switch_pan_gesture_pan), self);
  g_signal_connect (gesture, "drag-end",
                    G_CALLBACK (gtk_switch_pan_gesture_drag_end), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
  priv->pan_gesture = gesture;

  /* Translators: if the "on" state label requires more than three
   * glyphs then use MEDIUM VERTICAL BAR (U+2759) as the text for
   * the state
   */
  priv->on_label = gtk_label_new (C_("switch", "ON"));
  gtk_widget_set_parent (priv->on_label, GTK_WIDGET (self));

  /* Translators: if the "off" state label requires more than three
   * glyphs then use WHITE CIRCLE (U+25CB) as the text for the state
   */
  priv->off_label = gtk_label_new (C_("switch", "OFF"));
  gtk_widget_set_parent (priv->off_label, GTK_WIDGET (self));

  priv->slider = gtk_gizmo_new ("slider", NULL, NULL, NULL);
  gtk_widget_set_parent (priv->slider, GTK_WIDGET (self));
}

/**
 * gtk_switch_new:
 *
 * Creates a new #GtkSwitch widget.
 *
 * Returns: the newly created #GtkSwitch instance
 */
GtkWidget *
gtk_switch_new (void)
{
  return g_object_new (GTK_TYPE_SWITCH, NULL);
}

/**
 * gtk_switch_set_active:
 * @sw: a #GtkSwitch
 * @is_active: %TRUE if @sw should be active, and %FALSE otherwise
 *
 * Changes the state of @sw to the desired one.
 */
void
gtk_switch_set_active (GtkSwitch *sw,
                       gboolean   is_active)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  g_return_if_fail (GTK_IS_SWITCH (sw));

  gtk_switch_end_toggle_animation (sw);

  is_active = !!is_active;

  if (priv->is_active != is_active)
    {
      AtkObject *accessible;
      gboolean handled;

      priv->is_active = is_active;

      if (priv->is_active)
        priv->handle_pos = 1.0;
      else
        priv->handle_pos = 0.0;

      g_signal_emit (sw, signals[STATE_SET], 0, is_active, &handled);

      g_object_notify_by_pspec (G_OBJECT (sw), switch_props[PROP_ACTIVE]);

      accessible = gtk_widget_get_accessible (GTK_WIDGET (sw));
      atk_object_notify_state_change (accessible, ATK_STATE_CHECKED, priv->is_active);

      gtk_widget_queue_allocate (GTK_WIDGET (sw));
    }
}

/**
 * gtk_switch_get_active:
 * @sw: a #GtkSwitch
 *
 * Gets whether the #GtkSwitch is in its “on” or “off” state.
 *
 * Returns: %TRUE if the #GtkSwitch is active, and %FALSE otherwise
 */
gboolean
gtk_switch_get_active (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return priv->is_active;
}

/**
 * gtk_switch_set_state:
 * @sw: a #GtkSwitch
 * @state: the new state
 *
 * Sets the underlying state of the #GtkSwitch.
 *
 * Normally, this is the same as #GtkSwitch:active, unless the switch
 * is set up for delayed state changes. This function is typically
 * called from a #GtkSwitch::state-set signal handler.
 *
 * See #GtkSwitch::state-set for details.
 */
void
gtk_switch_set_state (GtkSwitch *sw,
                      gboolean   state)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);
  g_return_if_fail (GTK_IS_SWITCH (sw));

  state = state != FALSE;

  if (priv->state == state)
    return;

  priv->state = state;

  /* This will be a no-op if we're switching the state in response
   * to a UI change. We're setting active anyway, to catch 'spontaneous'
   * state changes.
   */
  gtk_switch_set_active (sw, state);

  if (state)
    gtk_widget_set_state_flags (GTK_WIDGET (sw), GTK_STATE_FLAG_CHECKED, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (sw), GTK_STATE_FLAG_CHECKED);

  g_object_notify (G_OBJECT (sw), "state");
}

/**
 * gtk_switch_get_state:
 * @sw: a #GtkSwitch
 *
 * Gets the underlying state of the #GtkSwitch.
 *
 * Returns: the underlying state
 */
gboolean
gtk_switch_get_state (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = gtk_switch_get_instance_private (sw);

  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return priv->state;
}
