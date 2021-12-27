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
 * ╰── slider
 * ]|
 *
 * GtkSwitch has two css nodes, the main node with the name switch and a subnode
 * named slider. Neither of them is using any style classes.
 */

#include "config.h"

#include "gtkswitch.h"

#include "deprecated/gtkactivatable.h"
#include "deprecated/gtktoggleaction.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkwidget.h"
#include "gtkmarshalers.h"
#include "gtkapplicationprivate.h"
#include "gtkactionable.h"
#include "a11y/gtkswitchaccessible.h"
#include "gtkactionhelper.h"
#include "gtkcsscustomgadgetprivate.h"
#include "gtkcssgadgetprivate.h"
#include "gtkiconhelperprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkprogresstrackerprivate.h"
#include "gtksettingsprivate.h"

#include "fallback-c89.c"

#define DEFAULT_SLIDER_WIDTH    (36)
#define DEFAULT_SLIDER_HEIGHT   (22)

struct _GtkSwitchPrivate
{
  GdkWindow *event_window;
  GtkAction *action;
  GtkActionHelper *action_helper;

  GtkGesture *pan_gesture;
  GtkGesture *multipress_gesture;

  GtkCssGadget *gadget;
  GtkCssGadget *slider_gadget;
  GtkCssGadget *on_gadget;
  GtkCssGadget *off_gadget;

  double handle_pos;
  guint tick_id;
  GtkProgressTracker tracker;

  guint state                 : 1;
  guint is_active             : 1;
  guint in_switch             : 1;
  guint use_action_appearance : 1;
};

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_STATE,
  PROP_RELATED_ACTION,
  PROP_USE_ACTION_APPEARANCE,
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
static void gtk_switch_activatable_interface_init (GtkActivatableIface *iface);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_CODE (GtkSwitch, gtk_switch, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSwitch)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                                gtk_switch_actionable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_switch_activatable_interface_init));
G_GNUC_END_IGNORE_DEPRECATIONS;

static void
gtk_switch_end_toggle_animation (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = sw->priv;

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
  GtkSwitchPrivate *priv = sw->priv;

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
  GtkSwitchPrivate *priv = sw->priv;

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
  GtkSwitchPrivate *priv = sw->priv;
  GtkAllocation allocation;

  gtk_widget_get_allocation (GTK_WIDGET (sw), &allocation);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  priv->in_switch = TRUE;

  /* If the press didn't happen in the draggable handle,
   * cancel the pan gesture right away
   */
  if ((priv->is_active && x <= allocation.width / 2.0) ||
      (!priv->is_active && x > allocation.width / 2.0))
    gtk_gesture_set_state (priv->pan_gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
gtk_switch_multipress_gesture_released (GtkGestureMultiPress *gesture,
                                        gint                  n_press,
                                        gdouble               x,
                                        gdouble               y,
                                        GtkSwitch            *sw)
{
  GtkSwitchPrivate *priv = sw->priv;
  GdkEventSequence *sequence;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (priv->in_switch &&
      gtk_gesture_handles_sequence (GTK_GESTURE (gesture), sequence))
    gtk_switch_begin_toggle_animation (sw);

  priv->in_switch = FALSE;
}

static void
gtk_switch_pan_gesture_pan (GtkGesturePan   *gesture,
                            GtkPanDirection  direction,
                            gdouble          offset,
                            GtkSwitch       *sw)
{
  GtkWidget *widget = GTK_WIDGET (sw);
  GtkSwitchPrivate *priv = sw->priv;
  gint width;

  if (direction == GTK_PAN_DIRECTION_LEFT)
    offset = -offset;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  width = gtk_widget_get_allocated_width (widget);

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
  GtkSwitchPrivate *priv = sw->priv;
  GdkEventSequence *sequence;
  GtkAllocation allocation;
  gboolean active;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));

  if (gtk_gesture_get_sequence_state (GTK_GESTURE (gesture), sequence) == GTK_EVENT_SEQUENCE_CLAIMED)
    {
      gtk_widget_get_allocation (GTK_WIDGET (sw), &allocation);

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

static gboolean
gtk_switch_enter (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (event->window == priv->event_window)
    {
      priv->in_switch = TRUE;
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  return FALSE;
}

static gboolean
gtk_switch_leave (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (event->window == priv->event_window)
    {
      priv->in_switch = FALSE;
      gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
    }

  return FALSE;
}

static void
gtk_switch_activate (GtkSwitch *sw)
{
  gtk_switch_begin_toggle_animation (sw);
}

static void
gtk_switch_get_slider_size (GtkCssGadget   *gadget,
                            GtkOrientation  orientation,
                            gint            for_size,
                            gint           *minimum,
                            gint           *natural,
                            gint           *minimum_baseline,
                            gint           *natural_baseline,
                            gpointer        unused)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  gdouble min_size;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      min_size = _gtk_css_number_value_get (gtk_css_style_get_value (gtk_css_gadget_get_style (gadget), GTK_CSS_PROPERTY_MIN_WIDTH), 100);

      if (min_size > 0.0)
        *minimum = 0;
      else
        gtk_widget_style_get (widget, "slider-width", minimum, NULL);
    }
  else
    {
      min_size = _gtk_css_number_value_get (gtk_css_style_get_value (gtk_css_gadget_get_style (gadget), GTK_CSS_PROPERTY_MIN_HEIGHT), 100);

      if (min_size > 0.0)
        *minimum = 0;
      else
        gtk_widget_style_get (widget, "slider-height", minimum, NULL);
    }

  *natural = *minimum;
}

static void
gtk_switch_get_content_size (GtkCssGadget   *gadget,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *minimum,
                             gint           *natural,
                             gint           *minimum_baseline,
                             gint           *natural_baseline,
                             gpointer        unused)
{
  GtkWidget *widget;
  GtkSwitch *self;
  GtkSwitchPrivate *priv;
  gint slider_minimum, slider_natural;
  gint on_minimum, on_natural;
  gint off_minimum, off_natural;

  widget = gtk_css_gadget_get_owner (gadget);
  self = GTK_SWITCH (widget);
  priv = self->priv;

  gtk_css_gadget_get_preferred_size (priv->slider_gadget,
                                     orientation,
                                     -1,
                                     &slider_minimum, &slider_natural,
                                     NULL, NULL);

  gtk_css_gadget_get_preferred_size (priv->on_gadget,
                                     orientation,
                                     -1,
                                     &on_minimum, &on_natural,
                                     NULL, NULL);
  gtk_css_gadget_get_preferred_size (priv->off_gadget,
                                     orientation,
                                     -1,
                                     &off_minimum, &off_natural,
                                     NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = 2 * MAX (slider_minimum, MAX (on_minimum, off_minimum));
      *natural = 2 * MAX (slider_natural, MAX (on_natural, off_natural));
    }
  else
    {
      *minimum = MAX (slider_minimum, MAX (on_minimum, off_minimum));
      *natural = MAX (slider_natural, MAX (on_natural, off_natural));
    }
}

static void
gtk_switch_get_preferred_width (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SWITCH (widget)->priv->gadget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_switch_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  gtk_css_gadget_get_preferred_size (GTK_SWITCH (widget)->priv->gadget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     minimum, natural,
                                     NULL, NULL);
}

static void
gtk_switch_allocate_contents (GtkCssGadget        *gadget,
                              const GtkAllocation *allocation,
                              int                  baseline,
                              GtkAllocation       *out_clip,
                              gpointer             unused)
{
  GtkSwitch *self = GTK_SWITCH (gtk_css_gadget_get_owner (gadget));
  GtkSwitchPrivate *priv = self->priv;
  GtkAllocation child_alloc;
  GtkAllocation on_clip, off_clip;

  child_alloc.x = allocation->x + round (priv->handle_pos * (allocation->width - allocation->width / 2));
  child_alloc.y = allocation->y;
  child_alloc.width = allocation->width / 2;
  child_alloc.height = allocation->height;

  gtk_css_gadget_allocate (priv->slider_gadget,
                           &child_alloc,
                           baseline,
                           out_clip);

  child_alloc.x = allocation->x;

  gtk_css_gadget_allocate (priv->on_gadget,
                           &child_alloc,
                           baseline,
                           &on_clip);

  gdk_rectangle_union (out_clip, &on_clip, out_clip);

  child_alloc.x = allocation->x + allocation->width - child_alloc.width;
  gtk_css_gadget_allocate (priv->off_gadget,
                           &child_alloc,
                           baseline,
                           &off_clip);

  gdk_rectangle_union (out_clip, &off_clip, out_clip);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    {
      GtkAllocation border_allocation;
      gtk_css_gadget_get_border_allocation (gadget, &border_allocation, NULL);
      gdk_window_move_resize (priv->event_window,
                              border_allocation.x,
                              border_allocation.y,
                              border_allocation.width,
                              border_allocation.height);
    }
}

static void
gtk_switch_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);
  gtk_css_gadget_allocate (priv->gadget,
                           allocation,
                           gtk_widget_get_allocated_baseline (widget),
                           &clip);

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_switch_realize (GtkWidget *widget)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GdkWindow *parent_window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;

  gtk_widget_set_realized (widget, TRUE);
  parent_window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, parent_window);
  g_object_ref (parent_window);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON1_MOTION_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  priv->event_window = gdk_window_new (parent_window,
                                       &attributes,
                                       attributes_mask);
  gtk_widget_register_window (widget, priv->event_window);
}

static void
gtk_switch_unrealize (GtkWidget *widget)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (priv->event_window != NULL)
    {
      gtk_widget_unregister_window (widget, priv->event_window);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_switch_parent_class)->unrealize (widget);
}

static void
gtk_switch_map (GtkWidget *widget)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  GTK_WIDGET_CLASS (gtk_switch_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gtk_switch_unmap (GtkWidget *widget)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_switch_parent_class)->unmap (widget);
}

static gboolean
gtk_switch_render_slider (GtkCssGadget *gadget,
                          cairo_t      *cr,
                          int           x,
                          int           y,
                          int           width,
                          int           height,
                          gpointer      data)
{
  return gtk_widget_has_visible_focus (gtk_css_gadget_get_owner (gadget));
}

static gboolean
gtk_switch_render_trough (GtkCssGadget *gadget,
                          cairo_t      *cr,
                          int           x,
                          int           y,
                          int           width,
                          int           height,
                          gpointer      data)
{
  GtkWidget *widget = gtk_css_gadget_get_owner (gadget);
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  gtk_css_gadget_draw (priv->on_gadget, cr);
  gtk_css_gadget_draw (priv->off_gadget, cr);
  gtk_css_gadget_draw (priv->slider_gadget, cr);

  return FALSE;
}

static gboolean
gtk_switch_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  gtk_css_gadget_draw (GTK_SWITCH (widget)->priv->gadget, cr);

  return FALSE;
}

static void
gtk_switch_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  previous_state_flags)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkStateFlags state = gtk_widget_get_state_flags (widget);

  gtk_css_gadget_set_state (priv->gadget, state);
  gtk_css_gadget_set_state (priv->slider_gadget, state);
  gtk_css_gadget_set_state (priv->on_gadget, state);
  gtk_css_gadget_set_state (priv->off_gadget, state);

  GTK_WIDGET_CLASS (gtk_switch_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
gtk_switch_set_related_action (GtkSwitch *sw,
                               GtkAction *action)
{
  GtkSwitchPrivate *priv = sw->priv;

  if (priv->action == action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (sw), action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  priv->action = action;
}

static void
gtk_switch_set_use_action_appearance (GtkSwitch *sw,
                                      gboolean   use_appearance)
{
  GtkSwitchPrivate *priv = sw->priv;

  if (priv->use_action_appearance != use_appearance)
    {
      priv->use_action_appearance = use_appearance;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (sw), priv->action);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gtk_switch_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  if (!sw->priv->action_helper)
    sw->priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_name (sw->priv->action_helper, action_name);
}

static void
gtk_switch_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  if (!sw->priv->action_helper)
    sw->priv->action_helper = gtk_action_helper_new (actionable);

  gtk_action_helper_set_action_target_value (sw->priv->action_helper, action_target);
}

static const gchar *
gtk_switch_get_action_name (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_action_helper_get_action_name (sw->priv->action_helper);
}

static GVariant *
gtk_switch_get_action_target_value (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_action_helper_get_action_target_value (sw->priv->action_helper);
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

    case PROP_RELATED_ACTION:
      gtk_switch_set_related_action (sw, g_value_get_object (value));
      break;

    case PROP_USE_ACTION_APPEARANCE:
      gtk_switch_set_use_action_appearance (sw, g_value_get_boolean (value));
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
  GtkSwitchPrivate *priv = GTK_SWITCH (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->is_active);
      break;

    case PROP_STATE:
      g_value_set_boolean (value, priv->state);
      break;

    case PROP_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;

    case PROP_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
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
  GtkSwitchPrivate *priv = GTK_SWITCH (object)->priv;

  g_clear_object (&priv->action_helper);

  if (priv->action)
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (object), NULL);
      G_GNUC_END_IGNORE_DEPRECATIONS;
      priv->action = NULL;
    }

  g_clear_object (&priv->pan_gesture);
  g_clear_object (&priv->multipress_gesture);

  G_OBJECT_CLASS (gtk_switch_parent_class)->dispose (object);
}

static void
gtk_switch_finalize (GObject *object)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (object)->priv;
  gtk_switch_end_toggle_animation (GTK_SWITCH (object));

  g_clear_object (&priv->gadget);
  g_clear_object (&priv->slider_gadget);
  g_clear_object (&priv->on_gadget);
  g_clear_object (&priv->off_gadget);

  G_OBJECT_CLASS (gtk_switch_parent_class)->finalize (object);
}

static gboolean
state_set (GtkSwitch *sw, gboolean state)
{
  if (sw->priv->action_helper)
    gtk_action_helper_activate (sw->priv->action_helper);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (sw->priv->action)
    gtk_action_activate (sw->priv->action);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_switch_set_state (sw, state);

  return TRUE;
}

static void
gtk_switch_class_init (GtkSwitchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer activatable_iface;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  activatable_iface = g_type_default_interface_peek (GTK_TYPE_ACTIVATABLE);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  switch_props[PROP_RELATED_ACTION] =
    g_param_spec_override ("related-action",
                           g_object_interface_find_property (activatable_iface,
                                                             "related-action"));

  switch_props[PROP_USE_ACTION_APPEARANCE] =
    g_param_spec_override ("use-action-appearance",
                           g_object_interface_find_property (activatable_iface,
                                                             "use-action-appearance"));

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
   *
   * Since: 3.14
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

  widget_class->get_preferred_width = gtk_switch_get_preferred_width;
  widget_class->get_preferred_height = gtk_switch_get_preferred_height;
  widget_class->size_allocate = gtk_switch_size_allocate;
  widget_class->realize = gtk_switch_realize;
  widget_class->unrealize = gtk_switch_unrealize;
  widget_class->map = gtk_switch_map;
  widget_class->unmap = gtk_switch_unmap;
  widget_class->draw = gtk_switch_draw;
  widget_class->enter_notify_event = gtk_switch_enter;
  widget_class->leave_notify_event = gtk_switch_leave;
  widget_class->state_flags_changed = gtk_switch_state_flags_changed;

  klass->activate = gtk_switch_activate;
  klass->state_set = state_set;

  /**
   * GtkSwitch:slider-width:
   *
   * The minimum width of the #GtkSwitch handle, in pixels.
   *
   * Deprecated: 3.20: Use the CSS min-width property instead.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("slider-width",
                                                             P_("Slider Width"),
                                                             P_("The minimum width of the handle"),
                                                             DEFAULT_SLIDER_WIDTH, G_MAXINT,
                                                             DEFAULT_SLIDER_WIDTH,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

  /**
   * GtkSwitch:slider-height:
   *
   * The minimum height of the #GtkSwitch handle, in pixels.
   *
   * Since: 3.18
   *
   * Deprecated: 3.20: Use the CSS min-height property instead.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("slider-height",
                                                             P_("Slider Height"),
                                                             P_("The minimum height of the handle"),
                                                             DEFAULT_SLIDER_HEIGHT, G_MAXINT,
                                                             DEFAULT_SLIDER_HEIGHT,
                                                             GTK_PARAM_READABLE|G_PARAM_DEPRECATED));

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
   *
   * Since: 3.14
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
  g_signal_set_va_marshaller (signals[STATE_SET],
                              G_TYPE_FROM_CLASS (gobject_class),
                              _gtk_marshal_BOOLEAN__BOOLEANv);

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SWITCH_ACCESSIBLE);
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOGGLE_BUTTON);

  gtk_widget_class_set_css_name (widget_class, "switch");
}

static void
gtk_switch_init (GtkSwitch *self)
{
  GtkSwitchPrivate *priv;
  GtkGesture *gesture;
  GtkCssNode *widget_node;

  priv = self->priv = gtk_switch_get_instance_private (self);

  priv->use_action_appearance = TRUE;
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  priv->gadget = gtk_css_custom_gadget_new_for_node (widget_node,
                                                     GTK_WIDGET (self),
                                                     gtk_switch_get_content_size,
                                                     gtk_switch_allocate_contents,
                                                     gtk_switch_render_trough,
                                                     NULL,
                                                     NULL);

  priv->slider_gadget = gtk_css_custom_gadget_new ("slider",
                                                   GTK_WIDGET (self),
                                                   priv->gadget,
                                                   NULL,
                                                   gtk_switch_get_slider_size,
                                                   NULL,
                                                   gtk_switch_render_slider,
                                                   NULL,
                                                   NULL);

  priv->on_gadget = gtk_icon_helper_new_named ("image", GTK_WIDGET (self));
  _gtk_icon_helper_set_icon_name (GTK_ICON_HELPER (priv->on_gadget), "switch-on-symbolic", GTK_ICON_SIZE_MENU);
  gtk_css_node_set_parent (gtk_css_gadget_get_node (priv->on_gadget), widget_node);
  gtk_css_node_set_state (gtk_css_gadget_get_node (priv->on_gadget), gtk_css_node_get_state (widget_node));

  priv->off_gadget = gtk_icon_helper_new_named ("image", GTK_WIDGET (self));
  _gtk_icon_helper_set_icon_name (GTK_ICON_HELPER (priv->off_gadget), "switch-off-symbolic", GTK_ICON_SIZE_MENU);
  gtk_css_node_set_parent (gtk_css_gadget_get_node (priv->off_gadget), widget_node);
  gtk_css_node_set_state (gtk_css_gadget_get_node (priv->off_gadget), gtk_css_node_get_state (widget_node));

  gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_switch_multipress_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_switch_multipress_gesture_released), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  priv->multipress_gesture = gesture;

  gesture = gtk_gesture_pan_new (GTK_WIDGET (self),
                                 GTK_ORIENTATION_HORIZONTAL);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  g_signal_connect (gesture, "pan",
                    G_CALLBACK (gtk_switch_pan_gesture_pan), self);
  g_signal_connect (gesture, "drag-end",
                    G_CALLBACK (gtk_switch_pan_gesture_drag_end), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  priv->pan_gesture = gesture;
}

/**
 * gtk_switch_new:
 *
 * Creates a new #GtkSwitch widget.
 *
 * Returns: the newly created #GtkSwitch instance
 *
 * Since: 3.0
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
 *
 * Since: 3.0
 */
void
gtk_switch_set_active (GtkSwitch *sw,
                       gboolean   is_active)
{
  GtkSwitchPrivate *priv;

  g_return_if_fail (GTK_IS_SWITCH (sw));

  gtk_switch_end_toggle_animation (sw);

  is_active = !!is_active;

  priv = sw->priv;

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
 *
 * Since: 3.0
 */
gboolean
gtk_switch_get_active (GtkSwitch *sw)
{
  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return sw->priv->is_active;
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
 *
 * Since: 3.14
 */
void
gtk_switch_set_state (GtkSwitch *sw,
                      gboolean   state)
{
  g_return_if_fail (GTK_IS_SWITCH (sw));

  state = state != FALSE;

  if (sw->priv->state == state)
    return;

  sw->priv->state = state;

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
 *
 * Since: 3.14
 */
gboolean
gtk_switch_get_state (GtkSwitch *sw)
{
  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return sw->priv->state;
}

static void
gtk_switch_update (GtkActivatable *activatable,
                   GtkAction      *action,
                   const gchar    *property_name)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (strcmp (property_name, "visible") == 0)
    {
      if (gtk_action_is_visible (action))
        gtk_widget_show (GTK_WIDGET (activatable));
      else
        gtk_widget_hide (GTK_WIDGET (activatable));
    }
  else if (strcmp (property_name, "sensitive") == 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
    }
  else if (strcmp (property_name, "active") == 0)
    {
      gtk_action_block_activate (action);
      gtk_switch_set_active (GTK_SWITCH (activatable), gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
      gtk_action_unblock_activate (action);
    }

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
gtk_switch_sync_action_properties (GtkActivatable *activatable,
                                   GtkAction      *action)
{
  if (!action)
    return;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));

  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));

  gtk_action_block_activate (action);
  gtk_switch_set_active (GTK_SWITCH (activatable), gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);

  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
gtk_switch_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_switch_update;
  iface->sync_action_properties = gtk_switch_sync_action_properties;
}
