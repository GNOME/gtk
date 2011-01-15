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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 *
 * Author:
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * Based on similar code from Mx.
 */

/**
 * SECTION:gtkswitch
 * @Short_Description: A "light switch" style toggle
 * @Title: GtkSwitch
 * @See_Also: #GtkToggleButton
 *
 * #GtkSwitch is a widget that has two states: on or off. The user can control
 * which state should be active by clicking the empty area, or by dragging the
 * handle.
 */

#include "config.h"

#include "gtkswitch.h"

#include "gtkaccessible.h"
#include "gtkactivatable.h"
#include "gtkintl.h"
#include "gtkstyle.h"
#include "gtkprivate.h"
#include "gtktoggleaction.h"
#include "gtkwidget.h"

#define DEFAULT_SLIDER_WIDTH    (36)
#define DEFAULT_SLIDER_HEIGHT   (22)

struct _GtkSwitchPrivate
{
  GdkWindow *event_window;
  GtkAction *action;

  gint handle_x;
  gint offset;
  gint drag_start;
  gint drag_threshold;

  guint is_active             : 1;
  guint is_dragging           : 1;
  guint in_press              : 1;
  guint in_switch             : 1;
  guint use_action_appearance : 1;
};

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_RELATED_ACTION,
  PROP_USE_ACTION_APPEARANCE,
  LAST_PROP
};

static GParamSpec *switch_props[LAST_PROP] = { NULL, };

static GType gtk_switch_accessible_factory_get_type (void);

static void gtk_switch_activatable_interface_init (GtkActivatableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSwitch, gtk_switch, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_switch_activatable_interface_init));

static gboolean
gtk_switch_button_press (GtkWidget      *widget,
                         GdkEventButton *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->is_active)
    {
      /* if the event occurred in the "off" area, then this
       * is a direct toggle
       */
      if (event->x <= allocation.width / 2)
        {
          priv->in_press = TRUE;
          return FALSE;
        }

      priv->offset = event->x - allocation.width / 2;
    }
  else
    {
      /* if the event occurred in the "on" area, then this
       * is a direct toggle
       */
      if (event->x >= allocation.width / 2)
        {
          priv->in_press = TRUE;
          return FALSE;
        }

      priv->offset = event->x;
    }

  priv->drag_start = event->x;

  g_object_get (gtk_widget_get_settings (widget),
                "gtk-dnd-drag-threshold", &priv->drag_threshold,
                NULL);

  return FALSE;
}

static gboolean
gtk_switch_motion (GtkWidget      *widget,
                   GdkEventMotion *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  /* if this is a direct toggle we don't handle motion */
  if (priv->in_press)
    return FALSE;

  if (ABS (event->x - priv->drag_start) < priv->drag_threshold)
    return TRUE;

  if (event->state & GDK_BUTTON1_MASK)
    {
      gint position = event->x - priv->offset;
      GtkAllocation allocation;
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);
      gtk_widget_get_allocation (widget, &allocation);

      /* constrain the handle within the trough width */
      if (position > (allocation.width / 2 - padding.right))
        priv->handle_x = allocation.width / 2 - padding.right;
      else if (position < padding.left)
        priv->handle_x = padding.left;
      else
        priv->handle_x = position;

      priv->is_dragging = TRUE;

      /* we need to redraw the handle */
      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_switch_button_release (GtkWidget      *widget,
                           GdkEventButton *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  /* dragged toggles have a "soft" grab, so we don't care whether we
   * are in the switch or not when the button is released; we do care
   * for direct toggles, instead
   */
  if (!priv->is_dragging && !priv->in_switch)
    return FALSE;

  /* direct toggle */
  if (priv->in_press)
    {
      priv->in_press = FALSE;
      gtk_switch_set_active (GTK_SWITCH (widget), !priv->is_active);

      return TRUE;
    }

  /* toggle the switch if the handle was clicked but a drag had not been
   * initiated */
  if (!priv->is_dragging && !priv->in_press)
    {
      gtk_switch_set_active (GTK_SWITCH (widget), !priv->is_active);

      return TRUE;
    }

  /* dragged toggle */
  if (priv->is_dragging)
    {
      priv->is_dragging = FALSE;

      /* if half the handle passed the middle of the switch, then we
       * consider it to be on
       */
      if ((priv->handle_x + (allocation.width / 4)) >= (allocation.width / 2))
        {
          gtk_switch_set_active (GTK_SWITCH (widget), TRUE);
          priv->handle_x = allocation.width / 2;
        }
      else
        {
          gtk_switch_set_active (GTK_SWITCH (widget), FALSE);
          priv->handle_x = 0;
        }

      gtk_widget_queue_draw (widget);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_switch_enter (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (event->window == priv->event_window)
    priv->in_switch = TRUE;

  return FALSE;
}

static gboolean
gtk_switch_leave (GtkWidget        *widget,
                  GdkEventCrossing *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (event->window == priv->event_window)
    priv->in_switch = FALSE;

  return FALSE;
}

static gboolean
gtk_switch_key_release (GtkWidget   *widget,
                        GdkEventKey *event)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_ISO_Enter ||
      event->keyval == GDK_KEY_space ||
      event->keyval == GDK_KEY_KP_Space)
    {
      gtk_switch_set_active (GTK_SWITCH (widget), !priv->is_active);
    }

  return FALSE;
}

static void
gtk_switch_get_preferred_width (GtkWidget *widget,
                                gint      *minimum,
                                gint      *natural)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint width, slider_width, focus_width, focus_pad;
  PangoLayout *layout;
  PangoRectangle logical_rect;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  width = padding.left + padding.right;

  gtk_widget_style_get (widget,
                        "slider-width", &slider_width,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  width += 2 * (focus_width + focus_pad);

  /* Translators: if the "on" state label requires more than three
   * glyphs then use MEDIUM VERTICAL BAR (U+2759) as the text for
   * the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "ON"));
  pango_layout_get_extents (layout, NULL, &logical_rect);
  pango_extents_to_pixels (&logical_rect, NULL);
  width += MAX (logical_rect.width, slider_width);

  /* Translators: if the "off" state label requires more than three
   * glyphs then use WHITE CIRCLE (U+25CB) as the text for the state
   */
  pango_layout_set_text (layout, C_("switch", "OFF"), -1);
  pango_layout_get_extents (layout, NULL, &logical_rect);
  pango_extents_to_pixels (&logical_rect, NULL);
  width += MAX (logical_rect.width, slider_width);

  g_object_unref (layout);

  if (minimum)
    *minimum = width;

  if (natural)
    *natural = width;
}

static void
gtk_switch_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gint height, focus_width, focus_pad;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gchar *str;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  height = padding.top + padding.bottom;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  height += 2 * (focus_width + focus_pad);

  str = g_strdup_printf ("%s%s",
                         C_("switch", "ON"),
                         C_("switch", "OFF"));

  layout = gtk_widget_create_pango_layout (widget, str);
  pango_layout_get_extents (layout, NULL, &logical_rect);
  pango_extents_to_pixels (&logical_rect, NULL);
  height += MAX (DEFAULT_SLIDER_HEIGHT, logical_rect.height);

  g_object_unref (layout);
  g_free (str);

  if (minimum)
    *minimum = height;

  if (natural)
    *natural = height;
}

static void
gtk_switch_size_allocate (GtkWidget     *widget,
                          GtkAllocation *allocation)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);
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
  gdk_window_set_user_data (priv->event_window, widget);
}

static void
gtk_switch_unrealize (GtkWidget *widget)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;

  if (priv->event_window != NULL)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
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
  GtkStateFlags state;

  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SLIDER);

  gtk_render_slider (context, cr,
                     box->x, box->y,
                     box->width, box->height,
                     GTK_ORIENTATION_HORIZONTAL);

  gtk_style_context_restore (context);
}

static gboolean
gtk_switch_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (widget)->priv;
  GtkStyleContext *context;
  GdkRectangle handle;
  PangoLayout *layout;
  PangoRectangle rect;
  gint label_x, label_y;
  GtkStateFlags state;
  GtkBorder padding;
  gint focus_width, focus_pad;
  gint x, y, width, height;

  gtk_widget_style_get (widget,
                        "focus-line-width", &focus_width,
                        "focus-padding", &focus_pad,
                        NULL);

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  if (priv->is_active)
    state |= GTK_STATE_FLAG_ACTIVE;

  gtk_style_context_get_padding (context, state, &padding);

  x = 0;
  y = 0;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  if (gtk_widget_has_focus (widget))
    gtk_render_focus (context, cr, x, y, width, height);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  x += focus_width + focus_pad;
  y += focus_width + focus_pad;
  width -= 2 * (focus_width + focus_pad);
  height -= 2 * (focus_width + focus_pad);

  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

  gtk_render_background (context, cr, x, y, width, height);
  gtk_render_frame (context, cr, x, y, width, height);

  /* XXX the +1/-1 it's pixel wriggling after checking with the default
   * theme and xmag
   */
  handle.y = y + padding.top + 1;
  handle.width = (width - padding.left - padding.right) / 2;
  handle.height = (height - padding.top - padding.bottom) - 1;

  /* Translators: if the "on" state label requires more than three
   * glyphs then use MEDIUM VERTICAL BAR (U+2759) as the text for
   * the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "ON"));
  pango_layout_get_extents (layout, NULL, &rect);
  pango_extents_to_pixels (&rect, NULL);

  label_x = x + padding.left
          + ((width / 2) - rect.width - padding.left - padding.right) / 2;
  label_y = y + padding.top
          + (height - rect.height - padding.top - padding.bottom) / 2;

  gtk_render_layout (context, cr, label_x, label_y, layout);

  g_object_unref (layout);

  /* Translators: if the "off" state label requires more than three
   * glyphs then use WHITE CIRCLE (U+25CB) as the text for the state
   */
  layout = gtk_widget_create_pango_layout (widget, C_("switch", "OFF"));
  pango_layout_get_extents (layout, NULL, &rect);
  pango_extents_to_pixels (&rect, NULL);

  label_x = x + padding.left
          + (width / 2)
          + ((width / 2) - rect.width - padding.left - padding.right) / 2;
  label_y = y + padding.top
          + (height - rect.height - padding.top - padding.bottom) / 2;

  gtk_render_layout (context, cr, label_x, label_y, layout);

  g_object_unref (layout);

  if (priv->is_dragging)
    handle.x = x + priv->handle_x;
  else if (priv->is_active)
    handle.x = x + width - handle.width - padding.right;
  else
    handle.x = x + padding.left;

  gtk_style_context_restore (context);

  gtk_switch_paint_handle (widget, cr, &handle);

  return FALSE;
}

static AtkObject *
gtk_switch_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (G_UNLIKELY (first_time))
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /* Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for the
       * parent type of GtkSwitch
       */
      derived_type = g_type_parent (GTK_TYPE_SWITCH);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry, derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
        atk_registry_set_factory_type (registry,
                                       GTK_TYPE_SWITCH,
                                       gtk_switch_accessible_factory_get_type ());

      first_time = FALSE;
    }

  return GTK_WIDGET_CLASS (gtk_switch_parent_class)->get_accessible (widget);
}

static void
gtk_switch_set_related_action (GtkSwitch *sw,
                               GtkAction *action)
{
  GtkSwitchPrivate *priv = sw->priv;

  if (priv->action == action)
    return;

  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (sw), action);

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

      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (sw), priv->action);
    }
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

    case PROP_RELATED_ACTION:
      gtk_switch_set_related_action (sw, g_value_get_object (value));
      break;

    case PROP_USE_ACTION_APPEARANCE:
      gtk_switch_set_use_action_appearance (sw, g_value_get_boolean (value));
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

    case PROP_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;

    case PROP_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_switch_dispose (GObject *object)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (object)->priv;

  if (priv->action)
    {
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (object), NULL);
      priv->action = NULL;
    }

  G_OBJECT_CLASS (gtk_switch_parent_class)->dispose (object);
}

static void
gtk_switch_class_init (GtkSwitchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer activatable_iface;

  g_type_class_add_private (klass, sizeof (GtkSwitchPrivate));

  activatable_iface = g_type_default_interface_peek (GTK_TYPE_ACTIVATABLE);
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
                          GTK_PARAM_READWRITE);

  gobject_class->set_property = gtk_switch_set_property;
  gobject_class->get_property = gtk_switch_get_property;
  gobject_class->dispose = gtk_switch_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, switch_props);

  widget_class->get_preferred_width = gtk_switch_get_preferred_width;
  widget_class->get_preferred_height = gtk_switch_get_preferred_height;
  widget_class->size_allocate = gtk_switch_size_allocate;
  widget_class->realize = gtk_switch_realize;
  widget_class->unrealize = gtk_switch_unrealize;
  widget_class->map = gtk_switch_map;
  widget_class->unmap = gtk_switch_unmap;
  widget_class->draw = gtk_switch_draw;
  widget_class->button_press_event = gtk_switch_button_press;
  widget_class->button_release_event = gtk_switch_button_release;
  widget_class->motion_notify_event = gtk_switch_motion;
  widget_class->enter_notify_event = gtk_switch_enter;
  widget_class->leave_notify_event = gtk_switch_leave;
  widget_class->key_release_event = gtk_switch_key_release;
  widget_class->get_accessible = gtk_switch_get_accessible;

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
}

static void
gtk_switch_init (GtkSwitch *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_SWITCH, GtkSwitchPrivate);
  self->priv->use_action_appearance = TRUE;
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
}

/**
 * gtk_switch_new:
 *
 * Creates a new #GtkSwitch widget.
 *
 * Return value: the newly created #GtkSwitch instance
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

  is_active = !!is_active;

  priv = sw->priv;

  if (priv->is_active != is_active)
    {
      AtkObject *accessible;
      GtkWidget *widget;
      GtkStyleContext *context;

      widget = GTK_WIDGET (sw);
      priv->is_active = is_active;

      g_object_notify_by_pspec (G_OBJECT (sw), switch_props[PROP_ACTIVE]);

      if (priv->action)
        gtk_action_activate (priv->action);

      accessible = gtk_widget_get_accessible (GTK_WIDGET (sw));
      atk_object_notify_state_change (accessible, ATK_STATE_CHECKED, priv->is_active);

      if (gtk_widget_get_realized (widget))
        {
          context = gtk_widget_get_style_context (widget);
          gtk_style_context_notify_state_change (context,
                                                 gtk_widget_get_window (widget),
                                                 NULL, GTK_STATE_ACTIVE, is_active);
        }

      gtk_widget_queue_draw (GTK_WIDGET (sw));
    }
}

/**
 * gtk_switch_get_active:
 * @sw: a #GtkSwitch
 *
 * Gets whether the #GtkSwitch is in its "on" or "off" state.
 *
 * Return value: %TRUE if the #GtkSwitch is active, and %FALSE otherwise
 *
 * Since: 3.0
 */
gboolean
gtk_switch_get_active (GtkSwitch *sw)
{
  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return sw->priv->is_active;
}

static void
gtk_switch_update (GtkActivatable *activatable,
                   GtkAction      *action,
                   const gchar    *property_name)
{
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
}

static void
gtk_switch_sync_action_properties (GtkActivatable *activatable,
                                   GtkAction      *action)
{
  if (!action)
    return;

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));

  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));

  gtk_action_block_activate (action);
  gtk_switch_set_active (GTK_SWITCH (activatable), gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);
}

static void
gtk_switch_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_switch_update;
  iface->sync_action_properties = gtk_switch_sync_action_properties;
}

/* accessibility: object */

/* dummy typedefs */
typedef struct _GtkSwitchAccessible             GtkSwitchAccessible;
typedef struct _GtkSwitchAccessibleClass        GtkSwitchAccessibleClass;

ATK_DEFINE_TYPE (GtkSwitchAccessible, _gtk_switch_accessible, GTK_TYPE_WIDGET);

static AtkStateSet *
gtk_switch_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (_gtk_switch_accessible_parent_class)->ref_state_set (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return state_set;

  if (gtk_switch_get_active (GTK_SWITCH (widget)))
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

  return state_set;
}

static void
_gtk_switch_accessible_initialize (AtkObject *accessible,
                                   gpointer   widget)
{
  ATK_OBJECT_CLASS (_gtk_switch_accessible_parent_class)->initialize (accessible, widget);

  atk_object_set_role (accessible, ATK_ROLE_TOGGLE_BUTTON);
  atk_object_set_name (accessible, C_("light switch widget", "Switch"));
  atk_object_set_description (accessible, _("Switches between on and off states"));
}

static void
_gtk_switch_accessible_class_init (GtkSwitchAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = _gtk_switch_accessible_initialize;
  atk_class->ref_state_set = gtk_switch_accessible_ref_state_set;
}

static void
_gtk_switch_accessible_init (GtkSwitchAccessible *self)
{
}

/* accessibility: factory */

typedef AtkObjectFactoryClass   GtkSwitchAccessibleFactoryClass;
typedef AtkObjectFactory        GtkSwitchAccessibleFactory;

G_DEFINE_TYPE (GtkSwitchAccessibleFactory,
               gtk_switch_accessible_factory,
               ATK_TYPE_OBJECT_FACTORY);

static GType
gtk_switch_accessible_factory_get_accessible_type (void)
{
  return _gtk_switch_accessible_get_type ();
}

static AtkObject *
gtk_switch_accessible_factory_create_accessible (GObject *obj)
{
  AtkObject *accessible;

  accessible = g_object_new (_gtk_switch_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static void
gtk_switch_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
  klass->create_accessible = gtk_switch_accessible_factory_create_accessible;
  klass->get_accessible_type = gtk_switch_accessible_factory_get_accessible_type;
}

static void
gtk_switch_accessible_factory_init (AtkObjectFactory *factory)
{
}
