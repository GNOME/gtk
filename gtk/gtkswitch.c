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
#include "gtkcssnodeprivate.h"
#include "gtkcssnodeutilsprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

#include "fallback-c89.c"

#define DEFAULT_SLIDER_WIDTH    (36)

struct _GtkSwitchPrivate
{
  GdkWindow *event_window;
  GtkAction *action;
  GtkActionHelper *action_helper;

  GtkGesture *pan_gesture;
  GtkGesture *multipress_gesture;

  GtkCssNode *slider_node;

  double handle_pos;
  gint64 start_time;
  gint64 end_time;
  guint tick_id;

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

/* From clutter-easing.c, based on Robert Penner's
 * infamous easing equations, MIT license.
 */
static gdouble
ease_out_cubic (gdouble t)
{
  gdouble p = t - 1;

  return p * p * p + 1;
}

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
  gint64 now;

  now = gdk_frame_clock_get_frame_time (clock);

  if (now < priv->end_time)
    {
      gdouble t;

      t = (now - priv->start_time) / (gdouble) (priv->end_time - priv->start_time);
      t = ease_out_cubic (t);
      if (priv->is_active)
        priv->handle_pos = 1.0 - t;
      else
        priv->handle_pos = t;
    }
  else
    {
      gtk_switch_set_active (sw, !priv->is_active);
    }

  gtk_widget_queue_draw (GTK_WIDGET (sw));

  return G_SOURCE_CONTINUE;
}

#define ANIMATION_DURATION 100

static void
gtk_switch_begin_toggle_animation (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = sw->priv;
  gboolean animate;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (sw)),
                "gtk-enable-animations", &animate,
                NULL);

  if (animate)
    {
      GdkFrameClock *clock = gtk_widget_get_frame_clock (GTK_WIDGET (sw));
      priv->start_time = gdk_frame_clock_get_frame_time (clock);
      priv->end_time = priv->start_time + 1000 * ANIMATION_DURATION;
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

  /* If the press didn't happen in the draggable handle,
   * cancel the pan gesture right away
   */
  if ((priv->is_active && x <= allocation.width / 2) ||
      (!priv->is_active && x > allocation.width / 2))
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
}

static void
gtk_switch_pan_gesture_pan (GtkGesturePan   *gesture,
                            GtkPanDirection  direction,
                            gdouble          offset,
                            GtkSwitch       *sw)
{
  GtkWidget *widget = GTK_WIDGET (sw);
  GtkSwitchPrivate *priv = sw->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint width;

  if (direction == GTK_PAN_DIRECTION_LEFT)
    offset = -offset;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save_to_node (context, priv->slider_node);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_restore (context);

  width = gtk_widget_get_allocated_width (widget);

  if (priv->is_active)
    offset += width / 2;
  
  offset /= width / 2;
  /* constrain the handle within the trough width */
  priv->handle_pos = CLAMP (offset, 0, 1.0);

  /* we need to redraw the handle */
  gtk_widget_queue_draw (widget);
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
  gtk_widget_queue_draw (GTK_WIDGET (sw));
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
      gtk_widget_queue_draw (widget);
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
      gtk_widget_queue_draw (widget);
    }

  return FALSE;
}

static void
gtk_switch_activate (GtkSwitch *sw)
{
  gtk_switch_begin_toggle_animation (sw);
}

static void
gtk_switch_get_slider_size (GtkCssNode     *cssnode,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *minimum,
                             gint           *natural,
                             gint           *minimum_baseline,
                             gint           *natural_baseline,
                             gpointer        data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  gint slider_width;

  gtk_widget_style_get (widget,
                        "slider-width", &slider_width,
                        NULL);
  
  if (orientation == GTK_ORIENTATION_VERTICAL)
    slider_width *= 0.6;

  *minimum = slider_width;
  *natural = slider_width;
}

static void
gtk_switch_get_content_size (GtkCssNode     *cssnode,
                             GtkOrientation  orientation,
                             gint            for_size,
                             gint           *minimum,
                             gint           *natural,
                             gint           *minimum_baseline,
                             gint           *natural_baseline,
                             gpointer        data)
{
  GtkWidget *widget;
  GtkSwitch *self;
  GtkSwitchPrivate *priv;
  gint slider_minimum, slider_natural;
  PangoLayout *layout;
  PangoRectangle on_rect, off_rect;

  widget = GTK_WIDGET (data);
  self = GTK_SWITCH (data);
  priv = self->priv;

  gtk_css_node_get_preferred_size (priv->slider_node,
                                   orientation,
                                   -1,
                                   &slider_minimum, &slider_natural,
                                   NULL, NULL,
                                   gtk_switch_get_slider_size,
                                   self);

  /* Translators: if the "on" state label requires more than three
   * glyphs then use MEDIUM VERTICAL BAR (U+2759) as the text for
   * the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "ON"));
  pango_layout_get_pixel_extents (layout, NULL, &on_rect);

  /* Translators: if the "off" state label requires more than three
   * glyphs then use WHITE CIRCLE (U+25CB) as the text for the state
   */
  pango_layout_set_text (layout, C_("switch", "OFF"), -1);
  pango_layout_get_pixel_extents (layout, NULL, &off_rect);

  g_object_unref (layout);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      int text_width = MAX (on_rect.width, off_rect.width);
      *minimum = 2 * MAX (slider_minimum, text_width);
      *natural = 2 * MAX (slider_natural, text_width);
    }
  else
    {
      int text_height = MAX (on_rect.height, off_rect.height);
      *minimum = MAX (slider_minimum, text_height);
      *natural = MAX (slider_natural, text_height);
    }
}

static void
gtk_switch_get_preferred_width (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  gtk_css_node_get_preferred_size (gtk_widget_get_css_node (widget),
                                   GTK_ORIENTATION_HORIZONTAL,
                                   -1,
                                   minimum, natural,
                                   NULL, NULL,
                                   gtk_switch_get_content_size,
                                   widget);
}

static void
gtk_switch_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  gtk_css_node_get_preferred_size (gtk_widget_get_css_node (widget),
                                   GTK_ORIENTATION_VERTICAL,
                                   -1,
                                   minimum, natural,
                                   NULL, NULL,
                                   gtk_switch_get_content_size,
                                   widget);
}

static void
gtk_switch_allocate_contents (GtkCssNode          *cssnode,
                              const GtkAllocation *allocation,
                              int                  baseline,
                              GtkAllocation       *out_clip,
                              gpointer             data)
{
  GtkSwitch *self = data;
  GtkSwitchPrivate *priv = self->priv;

  /* We pretend to allocate the full area to the slider. That way both
   * potential left and right clip overlap gets correctly computed.
   */
  gtk_css_node_allocate (priv->slider_node,
                         allocation,
                         baseline,
                         out_clip,
                         NULL,
                         NULL);
}

static void
gtk_switch_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkAllocation clip;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  gtk_css_node_allocate (gtk_widget_get_css_node (widget),
                         allocation,
                         gtk_widget_get_allocated_baseline (widget),
                         &clip,
                         gtk_switch_allocate_contents,
                         widget);
  
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
                            GDK_POINTER_MOTION_HINT_MASK |
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

static inline void
gtk_switch_paint_handle (GtkWidget    *widget,
                         cairo_t      *cr,
                         GdkRectangle *box)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);

  gtk_style_context_save_to_node (context, GTK_SWITCH (widget)->priv->slider_node);

  gtk_render_slider (context, cr,
                     box->x, box->y,
                     box->width, box->height,
                     GTK_ORIENTATION_HORIZONTAL);

  gtk_style_context_restore (context);
}

static gboolean
gtk_switch_render_slider (GtkCssNode *cssnode,
                          cairo_t    *cr,
                          int         width,
                          int         height,
                          gpointer    data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  
  return gtk_widget_has_visible_focus (widget);
}

static gboolean
gtk_switch_render_trough (GtkCssNode *cssnode,
                          cairo_t    *cr,
                          int         width,
                          int         height,
                          gpointer    data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  PangoLayout *layout;
  PangoRectangle rect;
  gint label_x, label_y;
  gint slider_offset;

  /* Translators: if the "on" state label requires more than three
   * glyphs then use MEDIUM VERTICAL BAR (U+2759) as the text for
   * the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "ON"));

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  label_x = ((width / 2) - rect.width) / 2;
  label_y = (height - rect.height) / 2;

  gtk_render_layout (context, cr, label_x, label_y, layout);

  g_object_unref (layout);

  /* Translators: if the "off" state label requires more than three
   * glyphs then use WHITE CIRCLE (U+25CB) as the text for the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "OFF"));

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  label_x = (width / 2) + ((width / 2) - rect.width) / 2;
  label_y = (height - rect.height) / 2;

  gtk_render_layout (context, cr, label_x, label_y, layout);

  g_object_unref (layout);

  slider_offset = round (priv->handle_pos * (width - width / 2));
  cairo_translate (cr, slider_offset, 0);

  gtk_css_node_draw (priv->slider_node,
                     cr,
                     width / 2,
                     height,
                     gtk_switch_render_slider,
                     widget);

  cairo_translate (cr, -slider_offset, 0);

  return FALSE;
}

static gboolean
gtk_switch_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  gtk_css_node_draw (gtk_widget_get_css_node (widget),
                     cr,
                     gtk_widget_get_allocated_width (widget),
                     gtk_widget_get_allocated_height (widget),
                     gtk_switch_render_trough,
                     widget);

  return FALSE;
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
  gtk_switch_end_toggle_animation (GTK_SWITCH (object));

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

  klass->activate = gtk_switch_activate;
  klass->state_set = state_set;

  /**
   * GtkSwitch:slider-width:
   *
   * The minimum width of the #GtkSwitch handle, in pixels.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("slider-width",
                                                             P_("Slider Width"),
                                                             P_("The minimum width of the handle"),
                                                             DEFAULT_SLIDER_WIDTH, G_MAXINT,
                                                             DEFAULT_SLIDER_WIDTH,
                                                             GTK_PARAM_READABLE));

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
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  widget_class->activate_signal = signals[ACTIVATE];

  /**
   * GtkSwitch::state-set:
   * @widget: the object on which the signal was emitted
   * @state: the new state of the switch
   *
   * The ::state-set signal on GtkSwitch is emitted to change the underlying
   * state. It is emitted when the user changes the switch position. The
   * default handler keeps the state in sync with the #GtkState:active
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

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_SWITCH_ACCESSIBLE);
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOGGLE_BUTTON);
}

static void
gtk_switch_init (GtkSwitch *self)
{
  GtkSwitchPrivate *priv;
  GtkStyleContext *context;
  GtkGesture *gesture;
  GtkCssNode *widget_node;

  priv = self->priv = gtk_switch_get_instance_private (self);

  priv->use_action_appearance = TRUE;
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);

  widget_node = gtk_widget_get_css_node (GTK_WIDGET (self));
  priv->slider_node = gtk_css_node_new ();
  gtk_css_node_set_widget_type (priv->slider_node, GTK_TYPE_SWITCH);
  gtk_css_node_add_class (priv->slider_node, g_quark_from_string (GTK_STYLE_CLASS_SLIDER));
  gtk_css_node_set_parent (priv->slider_node, widget_node);
  gtk_css_node_set_state (priv->slider_node, gtk_css_node_get_state (widget_node));
  g_signal_connect_object (priv->slider_node, "style-changed", G_CALLBACK (gtk_css_node_style_changed_for_widget), self, 0);
  g_object_unref (priv->slider_node);

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

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);
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

      g_object_notify_by_pspec (G_OBJECT (sw), switch_props[PROP_ACTIVE]);

      g_signal_emit (sw, signals[STATE_SET], 0, is_active, &handled);

      accessible = gtk_widget_get_accessible (GTK_WIDGET (sw));
      atk_object_notify_state_change (accessible, ATK_STATE_CHECKED, priv->is_active);

      gtk_widget_queue_draw (GTK_WIDGET (sw));
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
    gtk_widget_set_state_flags (GTK_WIDGET (sw), GTK_STATE_FLAG_ACTIVE, FALSE);
  else
    gtk_widget_unset_state_flags (GTK_WIDGET (sw), GTK_STATE_FLAG_ACTIVE);

  g_object_notify (G_OBJECT (sw), "state");

  gtk_widget_queue_draw (GTK_WIDGET (sw));
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
