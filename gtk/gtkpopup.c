/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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
 */

#include "config.h"

#include "gtkpopup.h"
#include "gtkroot.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkcssnodeprivate.h"
#include "gtkbindings.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkmnemonichash.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gdk/gdkeventsprivate.h"
#include "gtkpointerfocusprivate.h"

static GListStore *popup_list = NULL;

typedef struct {
  GdkDisplay *display;
  GskRenderer *renderer;
  GdkSurface *surface;
  GdkSurfaceState state;
  GtkWidget *relative_to;
  GdkGravity parent_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  int anchor_offset_x;
  int anchor_offset_y;

  GtkWidget *focus_widget;
  gboolean active;
  GtkWidget *default_widget;
  GtkMnemonicHash *mnemonic_hash;
  GList *foci;
} GtkPopupPrivate;

enum {
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  CLOSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_PARENT_ANCHOR = 1,
  PROP_SURFACE_ANCHOR,
  PROP_ANCHOR_HINTS,
  PROP_ANCHOR_OFFSET_X,
  PROP_ANCHOR_OFFSET_Y,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };
 
static void gtk_popup_root_interface_init (GtkRootInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPopup, gtk_popup, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkPopup)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ROOT,
                                                gtk_popup_root_interface_init))


static GdkDisplay *
gtk_popup_root_get_display (GtkRoot *root)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  return priv->display;
}

static GskRenderer *
gtk_popup_root_get_renderer (GtkRoot *root)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  return priv->renderer;
}

static void
gtk_popup_root_get_surface_transform (GtkRoot *root,
                                      int     *x,
                                      int     *y)
{
  GtkStyleContext *context;
  GtkBorder margin, border, padding;

  context = gtk_widget_get_style_context (GTK_WIDGET (root));
  gtk_style_context_get_margin (context, &margin);
  gtk_style_context_get_border (context, &border);
  gtk_style_context_get_padding (context, &padding);

  *x = margin.left + border.left + padding.left;
  *y = margin.top + border.top + padding.top;
}

static void
gtk_popup_move_resize (GtkPopup *popup)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GdkRectangle rect;
  GtkRequisition req;
 
  gtk_widget_get_preferred_size (GTK_WIDGET (popup), NULL, &req);
  gdk_surface_resize (priv->surface, req.width, req.height);
  gtk_widget_get_surface_allocation (priv->relative_to, &rect);
  gdk_surface_move_to_rect (priv->surface,
                            &rect,
                            priv->parent_anchor,
                            priv->surface_anchor,
                            priv->anchor_hints,
                            priv->anchor_offset_x,
                            priv->anchor_offset_y);
}

static void
gtk_popup_root_check_resize (GtkRoot *root)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *widget = GTK_WIDGET (popup);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    {
      gtk_popup_move_resize (popup);
      gtk_widget_allocate (GTK_WIDGET (popup),
                           gdk_surface_get_width (priv->surface),
                           gdk_surface_get_height (priv->surface),
                           -1, NULL);
    }
}

static void gtk_popup_set_is_active (GtkPopup *popup, gboolean active);

static void
gtk_popup_focus_in (GtkWidget *widget)
{
  gtk_popup_set_is_active (GTK_POPUP (widget), TRUE);
}

static void
gtk_popup_focus_out (GtkWidget *widget)
{
  gtk_popup_set_is_active (GTK_POPUP (widget), FALSE);
}

static void
ensure_state_flag_backdrop (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  if ((priv->state & GDK_SURFACE_STATE_FOCUSED) != 0)
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
  else
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_BACKDROP, FALSE);
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GdkSurfaceState new_surface_state;
  GdkSurfaceState changed_mask;

  new_surface_state = gdk_surface_get_state (_gtk_widget_get_surface (widget));
  changed_mask = new_surface_state ^ priv->state;
  priv->state = new_surface_state;

  if (changed_mask & GDK_SURFACE_STATE_FOCUSED)
    ensure_state_flag_backdrop (widget);
}

static void
surface_size_changed (GtkWindow *window,
                      guint      width,
                      guint      height)
{
}

static void
gtk_popup_init (GtkPopup *popup)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkEventController *controller;

  gtk_widget_set_has_surface (GTK_WIDGET (popup), TRUE);

  priv->parent_anchor = GDK_GRAVITY_SOUTH;
  priv->surface_anchor = GDK_GRAVITY_NORTH;
  priv->anchor_hints = GDK_ANCHOR_FLIP_Y;
  priv->anchor_offset_x = 0;
  priv->anchor_offset_y = 0;

  controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (controller, "focus-in", G_CALLBACK (gtk_popup_focus_in), popup);
  g_signal_connect_swapped (controller, "focus-out", G_CALLBACK (gtk_popup_focus_out), popup);
  gtk_widget_add_controller (GTK_WIDGET (popup), controller);
}

static void
gtk_popup_realize (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GdkRectangle parent_rect;

  if (_gtk_widget_get_alloc_needed (widget))
    {
      GdkRectangle allocation;

      allocation.x = 0;
      allocation.y = 0;
      allocation.width = 20; // FIXME
      allocation.height = 20;
      gtk_widget_size_allocate (widget, &allocation, -1);
      gtk_widget_queue_resize (widget);
    }

  gtk_widget_get_surface_allocation (priv->relative_to, &parent_rect);

  priv->surface = gdk_surface_new_popup (priv->display, &parent_rect);
  gdk_surface_set_transient_for (priv->surface, gtk_widget_get_surface (priv->relative_to));
  gdk_surface_set_type_hint (priv->surface, GDK_SURFACE_TYPE_HINT_POPUP_MENU);

  gtk_widget_set_surface (widget, priv->surface);
  g_signal_connect_swapped (priv->surface, "notify::state", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect_swapped (priv->surface, "size-changed", G_CALLBACK (surface_size_changed), widget);

  gtk_widget_register_surface (widget, priv->surface);

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->realize (widget);

  priv->renderer = gsk_renderer_new_for_surface (priv->surface);
}

static void
gtk_popup_unrealize (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (priv->renderer);
  g_clear_object (&priv->renderer);

  g_signal_handlers_disconnect_by_func (priv->surface, surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_size_changed, widget);

  g_clear_object (&priv->surface);
}

static void
gtk_popup_move_focus (GtkWidget         *widget,
                      GtkDirectionType   dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!gtk_widget_get_focus_child (widget))
    gtk_root_set_focus (GTK_ROOT (widget), NULL);
}

static void
gtk_popup_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_css_node_validate (gtk_widget_get_css_node (widget));
  gtk_widget_realize (widget);
  gtk_popup_root_check_resize (GTK_ROOT (widget));
  gtk_widget_map (widget);

  if (!gtk_widget_get_focus_child (widget))
    gtk_widget_child_focus (widget, GTK_DIR_TAB_FORWARD);
}

static void
gtk_popup_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void
grab_prepare_func (GdkSeat    *seat,
                   GdkSurface *surface,
                   gpointer    data)
{
  gdk_surface_show (surface);
}

static void
gtk_popup_map (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *child;
  GdkRectangle rect;

  gdk_seat_grab (gdk_display_get_default_seat (priv->display),
                 priv->surface,
                 GDK_SEAT_CAPABILITY_ALL,
                 TRUE,
                 NULL, NULL, grab_prepare_func, NULL);

  gtk_widget_get_surface_allocation (priv->relative_to, &rect);
  gdk_surface_move_to_rect (priv->surface,
                            &rect,
                            priv->parent_anchor,
                            priv->surface_anchor,
                            priv->anchor_hints,
                            priv->anchor_offset_x,
                            priv->anchor_offset_y);

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->map (widget);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);

  gdk_surface_focus (priv->surface, gtk_get_current_event_time ());
}

static void
gtk_popup_unmap (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *child;

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->unmap (widget);

  gdk_surface_hide (priv->surface);
  gdk_seat_ungrab (gdk_display_get_default_seat (priv->display));

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL)
    gtk_widget_unmap (child);
}

static void
gtk_popup_dispose (GObject *object)
{
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (popup_list)); i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (popup_list), i);
      if (item == object)
        {
          g_list_store_remove (popup_list, i);
          break;
        }
      else
        g_object_unref (item);
    }

  G_OBJECT_CLASS (gtk_popup_parent_class)->dispose (object);
}

static void
gtk_popup_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popup_parent_class)->finalize (object);
}

static void
gtk_popup_constructed (GObject *object)
{
  g_list_store_append (popup_list, object);
  g_object_unref (object);
}

static void
gtk_popup_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));
  gtk_widget_measure (child, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_popup_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkWidget *child;
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  if (priv->surface)
    {
      // FIXME why is this needed ?
      gdk_surface_move_resize (priv->surface, 0, 0, width, height);
      gtk_popup_move_resize (popup);
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  gtk_widget_size_allocate (child, &(GtkAllocation) { 0, 0, width, height }, baseline);
}

static void gtk_popup_set_focus (GtkPopup  *popup,
                                 GtkWidget *widget);

static void gtk_popup_set_default (GtkPopup  *popup,
                                   GtkWidget *widget);

static void
gtk_popup_set_property (GObject       *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkPopup *popup = GTK_POPUP (object);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  switch (prop_id)
    {
    case PROP_PARENT_ANCHOR:
      if (priv->parent_anchor != g_value_get_enum (value))
        {
          priv->parent_anchor = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SURFACE_ANCHOR:
      if (priv->surface_anchor != g_value_get_enum (value))
        {
          priv->surface_anchor = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_ANCHOR_HINTS:
      if (priv->anchor_hints != g_value_get_flags (value))
        {
          priv->anchor_hints = g_value_get_flags (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_ANCHOR_OFFSET_X:
      if (priv->anchor_offset_x != g_value_get_int (value))
        {
          priv->anchor_offset_x = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_ANCHOR_OFFSET_Y:
      if (priv->anchor_offset_y != g_value_get_int (value))
        {
          priv->anchor_offset_y = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_FOCUS_WIDGET:
      gtk_popup_set_focus (popup, g_value_get_object (value));
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_DEFAULT_WIDGET:
      gtk_popup_set_default (popup, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_popup_get_property (GObject      *object,
                        guint         prop_id,
                        GValue       *value,
                        GParamSpec   *pspec)
{
  GtkPopup *popup = GTK_POPUP (object);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  switch (prop_id)
    {
    case PROP_PARENT_ANCHOR:
      g_value_set_enum (value, priv->parent_anchor);
      break;

    case PROP_SURFACE_ANCHOR:
      g_value_set_enum (value, priv->surface_anchor);
      break;

    case PROP_ANCHOR_HINTS:
      g_value_set_flags (value, priv->anchor_hints);
      break;

    case PROP_ANCHOR_OFFSET_X:
      g_value_set_enum (value, priv->anchor_offset_x);
      break;

    case PROP_ANCHOR_OFFSET_Y:
      g_value_set_enum (value, priv->anchor_offset_y);
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_FOCUS_WIDGET:
      g_value_set_object (value, priv->focus_widget);
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_DEFAULT_WIDGET:
      g_value_set_object (value, priv->default_widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_popup_activate_default (GtkPopup *popup)
{
  gtk_root_activate_default (GTK_ROOT (popup));
}

static void
gtk_popup_activate_focus (GtkPopup *popup)
{
  gtk_root_activate_focus (GTK_ROOT (popup));
}

static void
gtk_popup_close (GtkPopup *popup)
{
  gtk_widget_hide (GTK_WIDGET (popup));
}

static void
add_tab_bindings (GtkBindingSet    *binding_set,
                  GdkModifierType   modifiers,
                  GtkDirectionType  direction)
{
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Tab, modifiers,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Tab, modifiers,
                                "move-focus", 1,
                                GTK_TYPE_DIRECTION_TYPE, direction);
}

static void
gtk_popup_class_init (GtkPopupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  if (popup_list == NULL)
    popup_list = g_list_store_new (GTK_TYPE_WIDGET);

  object_class->constructed = gtk_popup_constructed;
  object_class->dispose = gtk_popup_dispose;
  object_class->finalize = gtk_popup_finalize;
  object_class->set_property = gtk_popup_set_property;
  object_class->get_property = gtk_popup_get_property;

  widget_class->realize = gtk_popup_realize;
  widget_class->unrealize = gtk_popup_unrealize;
  widget_class->map = gtk_popup_map;
  widget_class->unmap = gtk_popup_unmap;
  widget_class->show = gtk_popup_show;
  widget_class->hide = gtk_popup_hide;
  widget_class->measure = gtk_popup_measure;
  widget_class->size_allocate = gtk_popup_size_allocate;
  widget_class->move_focus = gtk_popup_move_focus;

  klass->activate_default = gtk_popup_activate_default;
  klass->activate_focus = gtk_popup_activate_focus;
  klass->close = gtk_popup_close;

  props[PROP_PARENT_ANCHOR] =
    g_param_spec_enum ("parent-anchor",
                       "Parent Anchor",
                       "Where the reference point in the parent widget is located",
                       GDK_TYPE_GRAVITY,
                       GDK_GRAVITY_SOUTH,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_SURFACE_ANCHOR] =
    g_param_spec_enum ("surface-anchor",
                       "Surface Anchor",
                       "Where the reference point of the surface is located",
                       GDK_TYPE_GRAVITY,
                       GDK_GRAVITY_NORTH,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_ANCHOR_HINTS] =
    g_param_spec_flags ("anchor-hints",
                       "Anchor Hints",
                       "Hints that influence the placement of the surface",
                       GDK_TYPE_ANCHOR_HINTS,
                       GDK_ANCHOR_FLIP_Y,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_ANCHOR_OFFSET_X] =
    g_param_spec_int ("anchor-offset-x",
                      "Anchor Offset X",
                      "X offset of the anchor point",
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_ANCHOR_OFFSET_Y] =
    g_param_spec_int ("anchor-offset-y",
                      "Anchor Offset Y",
                      "Y offset of the anchor point",
                      G_MININT, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
  gtk_root_install_properties (object_class, NUM_PROPERTIES);

  signals[ACTIVATE_FOCUS] =
    g_signal_new (I_("activate-focus"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopupClass, activate_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[ACTIVATE_DEFAULT] =
    g_signal_new (I_("activate-default"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopupClass, activate_default),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[CLOSE] =
    g_signal_new (I_("close"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopupClass, close),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  binding_set = gtk_binding_set_by_class (klass);

  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0, "activate-focus", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0, "activate-focus", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0, "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0, "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0, "activate-default", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  gtk_widget_class_set_css_name (widget_class, "popover");
}

GtkWidget *
gtk_popup_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_POPUP, NULL));
}

static void
size_changed (GtkWidget *widget,
              int        width,
              int        height,
              int        baseline,
              GtkPopup  *popup)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  if (priv->surface)
    gtk_popup_move_resize (popup);
}

void
gtk_popup_set_relative_to (GtkPopup  *popup,
                           GtkWidget *relative_to)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  
  priv->relative_to = relative_to;
  g_signal_connect (priv->relative_to, "size-allocate", G_CALLBACK (size_changed), popup);
  priv->display = gtk_widget_get_display (relative_to);
  gtk_widget_set_parent (GTK_WIDGET (popup), relative_to);
}

static void
gtk_popup_set_focus (GtkPopup  *popup,
                     GtkWidget *focus)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *old_focus = NULL;
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;

  if (focus && !gtk_widget_is_sensitive (focus))
    return;

  if (priv->focus_widget)
    old_focus = g_object_ref (priv->focus_widget);
  g_set_object (&priv->focus_widget, NULL);

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (popup)));
  device = gdk_seat_get_keyboard (seat);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  gdk_event_set_display (event, gtk_widget_get_display (GTK_WIDGET (popup)));
  gdk_event_set_device (event, device);
  event->any.surface = _gtk_widget_get_surface (GTK_WIDGET (popup));
  if (event->any.surface)
    g_object_ref (event->any.surface);

  gtk_synthesize_crossing_events (GTK_ROOT (popup), old_focus, focus, event, GDK_CROSSING_NORMAL);

  g_object_unref (event);

  g_set_object (&priv->focus_widget, focus);

  g_clear_object (&old_focus);

  g_object_notify (G_OBJECT (popup), "focus-widget");
}

static void
do_focus_change (GtkWidget *widget,
                 gboolean   in)
{
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
  device = gdk_seat_get_keyboard (seat);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  gdk_event_set_display (event, gtk_widget_get_display (widget));
  gdk_event_set_device (event, device);

  event->any.type = GDK_FOCUS_CHANGE;
  event->any.surface = _gtk_widget_get_surface (widget);
  if (event->any.surface)
    g_object_ref (event->any.surface);
  event->focus_change.in = in;
  event->focus_change.mode = GDK_CROSSING_STATE_CHANGED;
  event->focus_change.detail = GDK_NOTIFY_ANCESTOR;

  gtk_widget_set_has_focus (widget, in);
  gtk_widget_event (widget, event);

  g_object_unref (event);
}

static void
gtk_popup_set_is_active (GtkPopup *popup,
                         gboolean  active)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  if (priv->active == active)
    return;

  priv->active = active;

  if (priv->focus_widget &&
      priv->focus_widget != GTK_WIDGET (popup) &&
      gtk_widget_has_focus (priv->focus_widget) != active)
    do_focus_change (priv->focus_widget, active);
}

GListModel *
gtk_popup_get_popups (void)
{
  if (popup_list == NULL)
    popup_list = g_list_store_new (GTK_TYPE_WIDGET);

  return G_LIST_MODEL (popup_list);
}

static void
gtk_popup_set_default (GtkPopup  *popup,
                       GtkWidget *widget)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  g_return_if_fail (GTK_IS_POPUP (popup));

  if (widget && !gtk_widget_get_can_default (widget))
    return;

  if (priv->default_widget == widget)
    return;

  if (priv->default_widget)
    {
      if (priv->focus_widget != priv->default_widget ||
          !gtk_widget_get_receives_default (priv->default_widget))
        _gtk_widget_set_has_default (priv->default_widget, FALSE);

      gtk_widget_queue_draw (priv->default_widget);
      g_object_notify (G_OBJECT (priv->default_widget), "has-default");
    }

  g_set_object (&priv->default_widget, widget);

  if (priv->default_widget)
    {
      if (priv->focus_widget == NULL ||
          !gtk_widget_get_receives_default (priv->focus_widget))
        _gtk_widget_set_has_default (priv->default_widget, TRUE);

      gtk_widget_queue_draw (priv->default_widget);
      g_object_notify (G_OBJECT (priv->default_widget), "has-default");
    }

  g_object_notify (G_OBJECT (popup), "default-widget");
}

static GtkMnemonicHash *
gtk_popup_get_mnemonic_hash (GtkPopup *popup,
                             gboolean  create)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  if (!priv->mnemonic_hash && create)
    priv->mnemonic_hash = _gtk_mnemonic_hash_new ();

  return priv->mnemonic_hash;
}

static void
gtk_popup_root_add_mnemonic (GtkRoot   *root,
                             guint      keyval,
                             GtkWidget *target)
{
  _gtk_mnemonic_hash_add (gtk_popup_get_mnemonic_hash (GTK_POPUP (root), TRUE), keyval, target);
}

static void
gtk_popup_root_remove_mnemonic (GtkRoot   *root,
                                guint      keyval,
                                GtkWidget *target)
{
  _gtk_mnemonic_hash_remove (gtk_popup_get_mnemonic_hash (GTK_POPUP (root), TRUE), keyval, target);
}

static gboolean
gtk_popup_root_activate_key (GtkRoot     *root,
                             GdkEventKey *event)
{
  GdkModifierType modifier = event->state;
  guint keyval = event->keyval;

  if ((modifier & gtk_accelerator_get_default_mod_mask ()) == GDK_MOD1_MASK)
    {
      GtkMnemonicHash *hash = gtk_popup_get_mnemonic_hash (GTK_POPUP (root), FALSE);
      if (hash)
        return _gtk_mnemonic_hash_activate (hash, keyval);      
    }

  return FALSE;
}

static GtkPointerFocus *
gtk_popup_lookup_pointer_focus (GtkPopup         *popup,
                                GdkDevice        *device,
                                GdkEventSequence *sequence)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;

      if (focus->device == device && focus->sequence == sequence)
        return focus;
    }

  return NULL;
}

static void
gtk_popup_add_pointer_focus (GtkPopup        *popup,
                             GtkPointerFocus *focus)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);

  priv->foci = g_list_prepend (priv->foci, gtk_pointer_focus_ref (focus));
}

static void
gtk_popup_remove_pointer_focus (GtkPopup        *popup,
                                GtkPointerFocus *focus)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GList *pos;

  pos = g_list_find (priv->foci, focus);
  if (!pos)
    return;

  priv->foci = g_list_remove (priv->foci, focus);
  gtk_pointer_focus_unref (focus);
}

static void
gtk_popup_root_update_pointer_focus (GtkRoot          *root,
                                     GdkDevice        *device,
                                     GdkEventSequence *sequence,
                                     GtkWidget        *target,
                                     double            x,
                                     double            y)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPointerFocus *focus;

  focus = gtk_popup_lookup_pointer_focus (popup, device, sequence);
  if (focus)
    {
      gtk_pointer_focus_ref (focus);

      if (target)
        {
          gtk_pointer_focus_set_target (focus, target);
          gtk_pointer_focus_set_coordinates (focus, x, y);
        }
      else
        {
          gtk_popup_remove_pointer_focus (popup, focus);
        }

      gtk_pointer_focus_unref (focus);
    }
  else if (target)
    {
      focus = gtk_pointer_focus_new (root, target, device, sequence, x, y);
      gtk_popup_add_pointer_focus (popup, focus);
      gtk_pointer_focus_unref (focus);
    }
}

static void
gtk_popup_root_update_pointer_focus_on_state_change (GtkRoot   *root,
                                                     GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GList *l = priv->foci, *cur;

  while (l)
    {
      GtkPointerFocus *focus = l->data;

      cur = l;
      focus = cur->data;
      l = cur->next;

      gtk_pointer_focus_ref (focus);

      if (focus->grab_widget &&
          (focus->grab_widget == widget ||
           gtk_widget_is_ancestor (focus->grab_widget, widget)))
        gtk_pointer_focus_set_implicit_grab (focus, NULL);

      if (GTK_WIDGET (focus->toplevel) == widget)
        {
          /* Unmapping the toplevel, remove pointer focus */
          priv->foci = g_list_remove_link (priv->foci, cur);
          gtk_pointer_focus_unref (focus);
        }
      else if (focus->target == widget ||
               gtk_widget_is_ancestor (focus->target, widget))
        {
          gtk_pointer_focus_repick_target (focus);
        }

      gtk_pointer_focus_unref (focus);
    }
}

static GtkWidget *
gtk_popup_root_lookup_pointer_focus (GtkRoot          *root,
                                     GdkDevice        *device,
                                     GdkEventSequence *sequence)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPointerFocus *focus;

  focus = gtk_popup_lookup_pointer_focus (popup, device, sequence);
  return focus ? gtk_pointer_focus_get_target (focus) : NULL;
}

static GtkWidget *
gtk_popup_root_lookup_effective_pointer_focus (GtkRoot          *root,
                                               GdkDevice        *device,
                                               GdkEventSequence *sequence)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPointerFocus *focus;

  focus = gtk_popup_lookup_pointer_focus (popup, device, sequence);
  return focus ? gtk_pointer_focus_get_effective_target (focus) : NULL;
}

static GtkWidget *
gtk_popup_root_lookup_pointer_focus_implicit_grab (GtkRoot          *root,
                                                   GdkDevice        *device,
                                                    GdkEventSequence *sequence)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPointerFocus *focus;

  focus = gtk_popup_lookup_pointer_focus (popup, device, sequence);
  return focus ? gtk_pointer_focus_get_implicit_grab (focus) : NULL;
}

static void
gtk_popup_root_set_pointer_focus_grab (GtkRoot          *root,
                                       GdkDevice        *device,
                                       GdkEventSequence *sequence,
                                       GtkWidget        *grab_widget)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPointerFocus *focus;

  focus = gtk_popup_lookup_pointer_focus (popup, device, sequence);
  if (!focus && !grab_widget)
    return;
  g_assert (focus != NULL);
  gtk_pointer_focus_set_implicit_grab (focus, grab_widget);
}

static void
update_cursor (GtkRoot   *root,
               GdkDevice *device,
               GtkWidget *grab_widget,
               GtkWidget *target)
{
  GdkCursor *cursor = NULL;

  if (grab_widget && !gtk_widget_is_ancestor (target, grab_widget))
    {
      /* Outside the grab widget, cursor stays to whatever the grab
       * widget says.
       */
      cursor = gtk_widget_get_cursor (grab_widget);
    }
  else
    {
      /* Inside the grab widget or in absence of grabs, allow walking
       * up the hierarchy to find out the cursor.
       */
      while (target)
        {
          if (grab_widget && target == grab_widget)
            break;

          cursor = gtk_widget_get_cursor (target);

          if (cursor)
            break;

          target = _gtk_widget_get_parent (target);
        }
    }

  gdk_surface_set_device_cursor (gtk_widget_get_surface (GTK_WIDGET (root)), device, cursor);
}

static void
gtk_popup_root_maybe_update_cursor (GtkRoot   *root,
                                    GtkWidget *widget,
                                    GdkDevice *device)
{
  GtkPopup *popup = GTK_POPUP (root);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GList *l;

  for (l = priv->foci; l; l = l->next)
    {
      GtkPointerFocus *focus = l->data;
      GtkWidget *grab_widget = NULL;
      GtkWidget  *target;

      if (focus->sequence)
        continue;
      if (device && device != focus->device)
        continue;

#if 0
        {
          GtkWindowGroup *group = gtk_window_get_group (root);
          grab_widget = gtk_window_group_get_current_device_grab (group, focus->device);
          if (!grab_widget)
            grab_widget = gtk_window_group_get_current_grab (group);
        }
#endif

      if (!grab_widget)
        grab_widget = gtk_pointer_focus_get_implicit_grab (focus);

      target = gtk_pointer_focus_get_target (focus);

      if (widget)
        {
          /* Check whether the changed widget affects the current cursor
           * lookups.
           */
          if (grab_widget && grab_widget != widget &&
              !gtk_widget_is_ancestor (widget, grab_widget))
            continue;
          if (target != widget &&
              !gtk_widget_is_ancestor (target, widget))
            continue;
        }

      update_cursor (focus->toplevel, focus->device, grab_widget, target);

      if (device)
        break;
    }
}

static void
gtk_popup_root_interface_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_popup_root_get_display;
  iface->get_renderer = gtk_popup_root_get_renderer;
  iface->get_surface_transform = gtk_popup_root_get_surface_transform;
  iface->check_resize = gtk_popup_root_check_resize;
  iface->add_mnemonic = gtk_popup_root_add_mnemonic;
  iface->remove_mnemonic = gtk_popup_root_remove_mnemonic;
  iface->activate_key = gtk_popup_root_activate_key;
  iface->update_pointer_focus = gtk_popup_root_update_pointer_focus;
  iface->update_pointer_focus_on_state_change = gtk_popup_root_update_pointer_focus_on_state_change;
  iface->lookup_pointer_focus = gtk_popup_root_lookup_pointer_focus;
  iface->lookup_pointer_focus_implicit_grab = gtk_popup_root_lookup_pointer_focus_implicit_grab;
  iface->lookup_effective_pointer_focus = gtk_popup_root_lookup_effective_pointer_focus;
  iface->set_pointer_focus_grab = gtk_popup_root_set_pointer_focus_grab;
  iface->maybe_update_cursor = gtk_popup_root_maybe_update_cursor;
}

