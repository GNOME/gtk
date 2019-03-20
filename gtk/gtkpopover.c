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

#include "gtkpopoverprivate.h"
#include "gtkroot.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkcssnodeprivate.h"
#include "gtkbindings.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkmnemonichash.h"
#include "gtkgizmoprivate.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmain.h"
#include "gtkstack.h"
#include "gtkmenusectionbox.h"
#include "gdk/gdkeventsprivate.h"
#include "gtkpointerfocusprivate.h"
#include "gtkcssnodeprivate.h"

static GListStore *popover_list = NULL;

typedef struct {
  GdkDisplay *display;
  GskRenderer *renderer;
  GdkSurface *surface;
  GtkWidget *focus_widget;
  gboolean active;
  GtkWidget *default_widget;
  GtkMnemonicHash *mnemonic_hash;
  GList *foci;

  GdkSurfaceState state;
  GtkWidget *relative_to;
  GdkRectangle pointing_to;
  gboolean has_pointing_to;
  GtkPositionType position;
  gboolean modal;
  gboolean has_grab;

  GtkWidget *contents_widget;
} GtkPopoverPrivate;

enum {
  ACTIVATE_FOCUS,
  ACTIVATE_DEFAULT,
  CLOSE,
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_RELATIVE_TO = 1,
  PROP_POINTING_TO,
  PROP_POSITION,
  PROP_MODAL,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

static void gtk_popover_root_interface_init (GtkRootInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPopover, gtk_popover, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkPopover)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ROOT,
                                                gtk_popover_root_interface_init))


static GdkDisplay *
gtk_popover_root_get_display (GtkRoot *root)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->display;
}

static GskRenderer *
gtk_popover_root_get_renderer (GtkRoot *root)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->renderer;
}

static void
gtk_popover_root_get_surface_transform (GtkRoot *root,
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
move_to_rect (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle rect;
  GdkGravity parent_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;

  gtk_widget_get_surface_allocation (priv->relative_to, &rect);
  if (priv->has_pointing_to)
    {
      rect.x += priv->pointing_to.x;
      rect.y += priv->pointing_to.y;
      rect.width = priv->pointing_to.width;
      rect.height = priv->pointing_to.height;
    }

  switch (priv->position)
    {
    case GTK_POS_LEFT:
      parent_anchor = GDK_GRAVITY_WEST;
      surface_anchor = GDK_GRAVITY_EAST;
      anchor_hints = GDK_ANCHOR_FLIP_X;
      break;

    case GTK_POS_RIGHT:
      parent_anchor = GDK_GRAVITY_EAST;
      surface_anchor = GDK_GRAVITY_WEST;
      anchor_hints = GDK_ANCHOR_FLIP_X;
      break;

    case GTK_POS_TOP:
      parent_anchor = GDK_GRAVITY_NORTH;
      surface_anchor = GDK_GRAVITY_SOUTH;
      anchor_hints = GDK_ANCHOR_FLIP_Y;
      break;

    case GTK_POS_BOTTOM:
      parent_anchor = GDK_GRAVITY_SOUTH;
      surface_anchor = GDK_GRAVITY_NORTH;
      anchor_hints = GDK_ANCHOR_FLIP_Y;
      break;

    default:
      g_assert_not_reached ();
    }

  gdk_surface_move_to_rect (priv->surface,
                            &rect,
                            parent_anchor,
                            surface_anchor,
                            anchor_hints,
                            0, 0);
}

static void
gtk_popover_move_resize (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkRequisition req;
 
  gtk_widget_get_preferred_size (GTK_WIDGET (popover), NULL, &req);
  gdk_surface_resize (priv->surface, req.width, req.height);
  move_to_rect (popover);
}

static void
gtk_popover_root_check_resize (GtkRoot *root)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *widget = GTK_WIDGET (popover);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    {
      gtk_popover_move_resize (popover);
      gtk_widget_allocate (GTK_WIDGET (popover),
                           gdk_surface_get_width (priv->surface),
                           gdk_surface_get_height (priv->surface),
                           -1, NULL);
    }
}

static void gtk_popover_set_is_active (GtkPopover *popover, gboolean active);

static void
gtk_popover_focus_in (GtkWidget *widget)
{
  gtk_popover_set_is_active (GTK_POPOVER (widget), TRUE);
}

static void
gtk_popover_focus_out (GtkWidget *widget)
{
  gtk_popover_set_is_active (GTK_POPOVER (widget), FALSE);
}

static void
ensure_state_flag_backdrop (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if ((priv->state & GDK_SURFACE_STATE_FOCUSED) != 0)
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);
  else
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_BACKDROP, FALSE);
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkSurfaceState new_surface_state;
  GdkSurfaceState changed_mask;

  new_surface_state = gdk_surface_get_state (_gtk_widget_get_surface (widget));
  changed_mask = new_surface_state ^ priv->state;
  priv->state = new_surface_state;

  if (changed_mask & GDK_SURFACE_STATE_FOCUSED)
    ensure_state_flag_backdrop (widget);

  if (changed_mask & GDK_SURFACE_STATE_WITHDRAWN)
    {
      if (priv->state & GDK_SURFACE_STATE_WITHDRAWN)
        gtk_widget_hide (widget);
    }
}

static void
surface_size_changed (GtkWindow *window,
                      guint      width,
                      guint      height)
{
}

static void
measure_contents (GtkGizmo       *gizmo,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline)
{
  GtkPopover *popover = GTK_POPOVER (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    gtk_widget_measure (child, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
allocate_contents (GtkGizmo *gizmo,
                   int       width,
                   int       height,
                   int       baseline)
{
  GtkPopover *popover = GTK_POPOVER (gtk_widget_get_parent (GTK_WIDGET (gizmo)));
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    gtk_widget_size_allocate (child,
                              &(GtkAllocation) { 0, 0, width, height
                              }, -1);
}

static void
gtk_popover_init (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkEventController *controller;
  GtkStyleContext *context;

  gtk_widget_set_has_surface (GTK_WIDGET (popover), TRUE);

  priv->position = GTK_POS_TOP;
  priv->modal = TRUE;

  priv->display = gdk_display_get_default ();

  controller = gtk_event_controller_key_new ();
  g_signal_connect_swapped (controller, "focus-in", G_CALLBACK (gtk_popover_focus_in), popover);
  g_signal_connect_swapped (controller, "focus-out", G_CALLBACK (gtk_popover_focus_out), popover);
  gtk_widget_add_controller (GTK_WIDGET (popover), controller);

  priv->contents_widget = gtk_gizmo_new ("contents",
                                         measure_contents,
                                         allocate_contents,
                                         NULL,
                                         NULL);
  gtk_widget_set_parent (priv->contents_widget, GTK_WIDGET (popover));

  context = gtk_widget_get_style_context (GTK_WIDGET (popover));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BACKGROUND);
}

static void
gtk_popover_realize (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GdkRectangle parent_rect;

  gtk_widget_get_surface_allocation (priv->relative_to, &parent_rect);

  priv->surface = gdk_surface_new_popup_full (priv->display, gtk_widget_get_surface (priv->relative_to));

  gtk_widget_set_surface (widget, priv->surface);
  g_signal_connect_swapped (priv->surface, "notify::state", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect_swapped (priv->surface, "size-changed", G_CALLBACK (surface_size_changed), widget);

  gtk_widget_register_surface (widget, priv->surface);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->realize (widget);

  priv->renderer = gsk_renderer_new_for_surface (priv->surface);
}

static void
gtk_popover_unrealize (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (priv->renderer);
  g_clear_object (&priv->renderer);

  g_signal_handlers_disconnect_by_func (priv->surface, surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (priv->surface, surface_size_changed, widget);

  g_clear_object (&priv->surface);
}

static void
gtk_popover_move_focus (GtkWidget         *widget,
                      GtkDirectionType   dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!gtk_widget_get_focus_child (widget))
    gtk_root_set_focus (GTK_ROOT (widget), NULL);
}

static void
gtk_popover_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_css_node_validate (gtk_widget_get_css_node (widget));
  gtk_widget_realize (widget);
  gtk_popover_root_check_resize (GTK_ROOT (widget));
  gtk_widget_map (widget);

  if (!gtk_widget_get_focus_child (widget))
    gtk_widget_child_focus (widget, GTK_DIR_TAB_FORWARD);
}

static void
gtk_popover_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
  g_signal_emit (widget, signals[CLOSED], 0);
}

static void
grab_prepare_func (GdkSeat    *seat,
                   GdkSurface *surface,
                   gpointer    data)
{
  gdk_surface_show (surface);
}

static void
gtk_popover_map (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *child;
  GdkRectangle parent_rect;

  if (priv->modal)
    {
      gdk_seat_grab (gdk_display_get_default_seat (priv->display),
                     priv->surface,
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL, grab_prepare_func, NULL);
      priv->has_grab = TRUE;
    }

  gtk_widget_get_surface_allocation (priv->relative_to, &parent_rect);
  move_to_rect (popover);

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->map (widget);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);
}

static void
gtk_popover_unmap (GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *child;

  GTK_WIDGET_CLASS (gtk_popover_parent_class)->unmap (widget);

  gdk_surface_hide (priv->surface);
  if (priv->has_grab)
    {
      gdk_seat_ungrab (gdk_display_get_default_seat (priv->display));
      priv->has_grab = FALSE;
    }

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL)
    gtk_widget_unmap (child);
}

static void
gtk_popover_dispose (GObject *object)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  guint i;
  GtkWidget *child;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (popover_list)); i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (popover_list), i);
      if (item == object)
        {
          g_list_store_remove (popover_list, i);
          break;
        }
      else
        g_object_unref (item);
    }

  child = gtk_bin_get_child (GTK_BIN (popover));

  if (child)
    {
      gtk_widget_unparent (child);
      _gtk_bin_set_child (GTK_BIN (popover), NULL);
    }

  g_clear_pointer (&priv->contents_widget, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_parent_class)->dispose (object);
}

static void
gtk_popover_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_parent_class)->finalize (object);
}

static void
gtk_popover_constructed (GObject *object)
{
  g_list_store_append (popover_list, object);
  g_object_unref (object);
}

static void
gtk_popover_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  gtk_widget_measure (priv->contents_widget,
                      orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_popover_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->surface)
    gtk_popover_move_resize (popover);

  gtk_widget_size_allocate (priv->contents_widget,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
gtk_popover_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkPopover *popover = GTK_POPOVER (widget);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  gtk_widget_snapshot_child (widget, priv->contents_widget, snapshot);
}

static void gtk_popover_set_focus (GtkPopover  *popover,
                                 GtkWidget *widget);

static void gtk_popover_set_default (GtkPopover  *popover,
                                   GtkWidget *widget);

static void
gtk_popover_set_property (GObject       *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkPopover *popover = GTK_POPOVER (object);

  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      gtk_popover_set_relative_to (popover, g_value_get_object (value));
      break;

    case PROP_POINTING_TO:
      gtk_popover_set_pointing_to (popover, g_value_get_boxed (value));
      break;

    case PROP_POSITION:
      gtk_popover_set_position (popover, g_value_get_enum (value));
      break;

    case PROP_MODAL:
      gtk_popover_set_modal (popover, g_value_get_boolean (value));
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_FOCUS_WIDGET:
      gtk_popover_set_focus (popover, g_value_get_object (value));
      break;

    case NUM_PROPERTIES + GTK_ROOT_PROP_DEFAULT_WIDGET:
      gtk_popover_set_default (popover, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_popover_get_property (GObject      *object,
                        guint         prop_id,
                        GValue       *value,
                        GParamSpec   *pspec)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      g_value_set_object (value, priv->relative_to);
      break;

    case PROP_POINTING_TO:
      g_value_set_boxed (value, &priv->pointing_to);
      break;

    case PROP_POSITION:
      g_value_set_enum (value, priv->position);
      break;

    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
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
gtk_popover_activate_default (GtkPopover *popover)
{
  gtk_root_activate_default (GTK_ROOT (popover));
}

static void
gtk_popover_activate_focus (GtkPopover *popover)
{
  gtk_root_activate_focus (GTK_ROOT (popover));
}

static void
gtk_popover_close (GtkPopover *popover)
{
  gtk_widget_hide (GTK_WIDGET (popover));
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
gtk_popover_add (GtkContainer *container,
                 GtkWidget    *child)
{
  GtkPopover *popover = GTK_POPOVER (container);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  _gtk_bin_set_child (GTK_BIN (popover), child);
  gtk_widget_set_parent (child, priv->contents_widget);
}
 
static void
gtk_popover_remove (GtkContainer *container,
                    GtkWidget    *child)
{
  GtkPopover *popover = GTK_POPOVER (container);

  _gtk_bin_set_child (GTK_BIN (popover), NULL);
  gtk_widget_unparent (child);
}

static void
gtk_popover_class_init (GtkPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkBindingSet *binding_set;

  if (popover_list == NULL)
    popover_list = g_list_store_new (GTK_TYPE_WIDGET);

  object_class->constructed = gtk_popover_constructed;
  object_class->dispose = gtk_popover_dispose;
  object_class->finalize = gtk_popover_finalize;
  object_class->set_property = gtk_popover_set_property;
  object_class->get_property = gtk_popover_get_property;

  widget_class->realize = gtk_popover_realize;
  widget_class->unrealize = gtk_popover_unrealize;
  widget_class->map = gtk_popover_map;
  widget_class->unmap = gtk_popover_unmap;
  widget_class->show = gtk_popover_show;
  widget_class->hide = gtk_popover_hide;
  widget_class->measure = gtk_popover_measure;
  widget_class->size_allocate = gtk_popover_size_allocate;
  widget_class->snapshot = gtk_popover_snapshot;
  widget_class->move_focus = gtk_popover_move_focus;

  container_class->add = gtk_popover_add;
  container_class->remove = gtk_popover_remove;

  klass->activate_default = gtk_popover_activate_default;
  klass->activate_focus = gtk_popover_activate_focus;
  klass->close = gtk_popover_close;

  properties[PROP_RELATIVE_TO] =
      g_param_spec_object ("relative-to",
                           P_("Relative to"),
                           P_("Widget the bubble window points to"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE);

  properties[PROP_POINTING_TO] =
      g_param_spec_boxed ("pointing-to",
                          P_("Pointing to"),
                          P_("Rectangle the bubble window points to"),
                          GDK_TYPE_RECTANGLE,
                          GTK_PARAM_READWRITE);

  properties[PROP_POSITION] =
      g_param_spec_enum ("position",
                         P_("Position"),
                         P_("Position to place the bubble window"),
                         GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MODAL] =
      g_param_spec_boolean ("modal",
                            P_("Modal"),
                            P_("Whether the popover is modal"),
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
  gtk_root_install_properties (object_class, NUM_PROPERTIES);

  signals[ACTIVATE_FOCUS] =
    g_signal_new (I_("activate-focus"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopoverClass, activate_focus),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[ACTIVATE_DEFAULT] =
    g_signal_new (I_("activate-default"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopoverClass, activate_default),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[CLOSE] =
    g_signal_new (I_("close"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkPopoverClass, close),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[CLOSED] =
    g_signal_new (I_("closed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkPopoverClass, closed),
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
gtk_popover_new (GtkWidget *relative_to)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_POPOVER,
                                   "relative-to", relative_to,
                                   NULL));
}

static void
size_changed (GtkWidget *widget,
              int        width,
              int        height,
              int        baseline,
              GtkPopover  *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->surface)
    gtk_popover_move_resize (popover);
}

static void
gtk_popover_set_focus (GtkPopover  *popover,
                     GtkWidget *focus)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GtkWidget *old_focus = NULL;
  GdkSeat *seat;
  GdkDevice *device;
  GdkEvent *event;

  if (focus && !gtk_widget_is_sensitive (focus))
    return;

  if (priv->focus_widget)
    old_focus = g_object_ref (priv->focus_widget);
  g_set_object (&priv->focus_widget, NULL);

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (popover)));
  device = gdk_seat_get_keyboard (seat);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  gdk_event_set_display (event, gtk_widget_get_display (GTK_WIDGET (popover)));
  gdk_event_set_device (event, device);
  event->any.surface = _gtk_widget_get_surface (GTK_WIDGET (popover));
  if (event->any.surface)
    g_object_ref (event->any.surface);

  gtk_synthesize_crossing_events (GTK_ROOT (popover), old_focus, focus, event, GDK_CROSSING_NORMAL);

  g_object_unref (event);

  g_set_object (&priv->focus_widget, focus);

  g_clear_object (&old_focus);

  g_object_notify (G_OBJECT (popover), "focus-widget");
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
gtk_popover_set_is_active (GtkPopover *popover,
                         gboolean  active)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (priv->active == active)
    return;

  priv->active = active;

  if (priv->focus_widget &&
      priv->focus_widget != GTK_WIDGET (popover) &&
      gtk_widget_has_focus (priv->focus_widget) != active)
    do_focus_change (priv->focus_widget, active);
}

GListModel *
gtk_popover_get_popovers (void)
{
  if (popover_list == NULL)
    popover_list = g_list_store_new (GTK_TYPE_WIDGET);

  return G_LIST_MODEL (popover_list);
}

static void
gtk_popover_set_default (GtkPopover  *popover,
                       GtkWidget *widget)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

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

  g_object_notify (G_OBJECT (popover), "default-widget");
}

static GtkMnemonicHash *
gtk_popover_get_mnemonic_hash (GtkPopover *popover,
                             gboolean  create)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  if (!priv->mnemonic_hash && create)
    priv->mnemonic_hash = _gtk_mnemonic_hash_new ();

  return priv->mnemonic_hash;
}

static void
gtk_popover_root_add_mnemonic (GtkRoot   *root,
                             guint      keyval,
                             GtkWidget *target)
{
  _gtk_mnemonic_hash_add (gtk_popover_get_mnemonic_hash (GTK_POPOVER (root), TRUE), keyval, target);
}

static void
gtk_popover_root_remove_mnemonic (GtkRoot   *root,
                                guint      keyval,
                                GtkWidget *target)
{
  _gtk_mnemonic_hash_remove (gtk_popover_get_mnemonic_hash (GTK_POPOVER (root), TRUE), keyval, target);
}

static gboolean
gtk_popover_root_activate_key (GtkRoot     *root,
                             GdkEventKey *event)
{
  GdkModifierType modifier = event->state;
  guint keyval = event->keyval;

  if ((modifier & gtk_accelerator_get_default_mod_mask ()) == GDK_MOD1_MASK)
    {
      GtkMnemonicHash *hash = gtk_popover_get_mnemonic_hash (GTK_POPOVER (root), FALSE);
      if (hash)
        return _gtk_mnemonic_hash_activate (hash, keyval);      
    }

  return FALSE;
}

static GtkPointerFocus *
gtk_popover_lookup_pointer_focus (GtkPopover         *popover,
                                GdkDevice        *device,
                                GdkEventSequence *sequence)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
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
gtk_popover_add_pointer_focus (GtkPopover        *popover,
                             GtkPointerFocus *focus)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  priv->foci = g_list_prepend (priv->foci, gtk_pointer_focus_ref (focus));
}

static void
gtk_popover_remove_pointer_focus (GtkPopover        *popover,
                                GtkPointerFocus *focus)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  GList *pos;

  pos = g_list_find (priv->foci, focus);
  if (!pos)
    return;

  priv->foci = g_list_remove (priv->foci, focus);
  gtk_pointer_focus_unref (focus);
}

static void
gtk_popover_root_update_pointer_focus (GtkRoot          *root,
                                     GdkDevice        *device,
                                     GdkEventSequence *sequence,
                                     GtkWidget        *target,
                                     double            x,
                                     double            y)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPointerFocus *focus;

  focus = gtk_popover_lookup_pointer_focus (popover, device, sequence);
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
          gtk_popover_remove_pointer_focus (popover, focus);
        }

      gtk_pointer_focus_unref (focus);
    }
  else if (target)
    {
      focus = gtk_pointer_focus_new (root, target, device, sequence, x, y);
      gtk_popover_add_pointer_focus (popover, focus);
      gtk_pointer_focus_unref (focus);
    }
}

static void
gtk_popover_root_update_pointer_focus_on_state_change (GtkRoot   *root,
                                                     GtkWidget *widget)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
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
gtk_popover_root_lookup_pointer_focus (GtkRoot          *root,
                                     GdkDevice        *device,
                                     GdkEventSequence *sequence)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPointerFocus *focus;

  focus = gtk_popover_lookup_pointer_focus (popover, device, sequence);
  return focus ? gtk_pointer_focus_get_target (focus) : NULL;
}

static GtkWidget *
gtk_popover_root_lookup_effective_pointer_focus (GtkRoot          *root,
                                               GdkDevice        *device,
                                               GdkEventSequence *sequence)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPointerFocus *focus;

  focus = gtk_popover_lookup_pointer_focus (popover, device, sequence);
  return focus ? gtk_pointer_focus_get_effective_target (focus) : NULL;
}

static GtkWidget *
gtk_popover_root_lookup_pointer_focus_implicit_grab (GtkRoot          *root,
                                                   GdkDevice        *device,
                                                    GdkEventSequence *sequence)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPointerFocus *focus;

  focus = gtk_popover_lookup_pointer_focus (popover, device, sequence);
  return focus ? gtk_pointer_focus_get_implicit_grab (focus) : NULL;
}

static void
gtk_popover_root_set_pointer_focus_grab (GtkRoot          *root,
                                       GdkDevice        *device,
                                       GdkEventSequence *sequence,
                                       GtkWidget        *grab_widget)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPointerFocus *focus;

  focus = gtk_popover_lookup_pointer_focus (popover, device, sequence);
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
gtk_popover_root_maybe_update_cursor (GtkRoot   *root,
                                    GtkWidget *widget,
                                    GdkDevice *device)
{
  GtkPopover *popover = GTK_POPOVER (root);
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
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
gtk_popover_root_interface_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_popover_root_get_display;
  iface->get_renderer = gtk_popover_root_get_renderer;
  iface->get_surface_transform = gtk_popover_root_get_surface_transform;
  iface->check_resize = gtk_popover_root_check_resize;
  iface->add_mnemonic = gtk_popover_root_add_mnemonic;
  iface->remove_mnemonic = gtk_popover_root_remove_mnemonic;
  iface->activate_key = gtk_popover_root_activate_key;
  iface->update_pointer_focus = gtk_popover_root_update_pointer_focus;
  iface->update_pointer_focus_on_state_change = gtk_popover_root_update_pointer_focus_on_state_change;
  iface->lookup_pointer_focus = gtk_popover_root_lookup_pointer_focus;
  iface->lookup_pointer_focus_implicit_grab = gtk_popover_root_lookup_pointer_focus_implicit_grab;
  iface->lookup_effective_pointer_focus = gtk_popover_root_lookup_effective_pointer_focus;
  iface->set_pointer_focus_grab = gtk_popover_root_set_pointer_focus_grab;
  iface->maybe_update_cursor = gtk_popover_root_maybe_update_cursor;
}

void
gtk_popover_set_relative_to (GtkPopover  *popover,
                           GtkWidget *relative_to)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);
  
  g_return_if_fail (GTK_IS_POPOVER (popover));

  g_object_ref (popover);

  if (priv->relative_to)
    {
      g_signal_handlers_disconnect_by_func (priv->relative_to, size_changed, popover);
      gtk_widget_unparent (GTK_WIDGET (popover));
    }

  priv->relative_to = relative_to;

  if (priv->relative_to)
    {
      g_signal_connect (priv->relative_to, "size-allocate", G_CALLBACK (size_changed), popover);
      priv->display = gtk_widget_get_display (relative_to);
      gtk_css_node_set_parent (gtk_widget_get_css_node (GTK_WIDGET (popover)),
                               gtk_widget_get_css_node (relative_to));
      gtk_widget_set_parent (GTK_WIDGET (popover), relative_to);
    }

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_RELATIVE_TO]);

  g_object_unref (popover);
}

GtkWidget *
gtk_popover_get_relative_to (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), NULL);

  return priv->relative_to;
}

void
gtk_popover_set_pointing_to (GtkPopover           *popover,
                           const GdkRectangle *rect)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (rect)
    {
      priv->pointing_to = *rect;
      priv->has_pointing_to = TRUE;
    }
  else
    priv->has_pointing_to = FALSE;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_POINTING_TO]);
}

gboolean
gtk_popover_get_pointing_to (GtkPopover     *popover,
                           GdkRectangle *rect)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  if (priv->has_pointing_to)
    *rect = priv->pointing_to;

  return priv->has_pointing_to;
}

void
gtk_popover_set_position (GtkPopover        *popover,
                        GtkPositionType  position)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  if (priv->position == position)
    return;

  priv->position = position;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_POSITION]);
}

GtkPositionType
gtk_popover_get_position (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), GTK_POS_TOP);

  return priv->position;
}

void
gtk_popover_set_modal (GtkPopover *popover,
                     gboolean  modal)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_if_fail (GTK_IS_POPOVER (popover));

  modal = modal != FALSE;

  if (priv->modal == modal)
    return;

  priv->modal = modal;

  g_object_notify_by_pspec (G_OBJECT (popover), properties[PROP_MODAL]);
}

gboolean
gtk_popover_get_modal (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  return priv->modal;
}

void
gtk_popover_popup (GtkPopover *popover)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_widget_show (GTK_WIDGET (popover));
}

void
gtk_popover_popdown (GtkPopover *popover)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_widget_hide (GTK_WIDGET (popover));
}

void
gtk_popover_set_default_widget (GtkPopover *popover,
                                GtkWidget  *widget)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));

  gtk_root_set_default (GTK_ROOT (popover), widget);
}

static void
back_to_main (GtkWidget *popover)
{
  GtkWidget *stack;

  stack = gtk_bin_get_child (GTK_BIN (popover));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "main");
}

void
gtk_popover_bind_model (GtkPopover  *popover,
                        GMenuModel  *model,
                        const gchar *action_namespace)
{
  GtkWidget *child;
  GtkWidget *stack;
  GtkStyleContext *style_context;

  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  child = gtk_bin_get_child (GTK_BIN (popover));
  if (child)
    gtk_widget_destroy (child);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (popover));

  if (model)
    {
      stack = gtk_stack_new ();
      gtk_stack_set_vhomogeneous (GTK_STACK (stack), FALSE);
      gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
      gtk_stack_set_interpolate_size (GTK_STACK (stack), TRUE);
      gtk_widget_show (stack);
      gtk_container_add (GTK_CONTAINER (popover), stack);

      gtk_menu_section_box_new_toplevel (GTK_STACK (stack),
                                         model,
                                         action_namespace,
                                         popover);
      gtk_stack_set_visible_child_name (GTK_STACK (stack), "main");

      g_signal_connect (popover, "unmap", G_CALLBACK (back_to_main), NULL);
      g_signal_connect (popover, "map", G_CALLBACK (back_to_main), NULL);

      gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_MENU);
    }
  else
    {
      gtk_style_context_remove_class (style_context, GTK_STYLE_CLASS_MENU);
    }
}

GtkWidget *
gtk_popover_new_from_model (GtkWidget  *relative_to,
                            GMenuModel *model)
{
  GtkWidget *popover;

  g_return_val_if_fail (relative_to == NULL || GTK_IS_WIDGET (relative_to), NULL);
  g_return_val_if_fail (G_IS_MENU_MODEL (model), NULL);

  popover = gtk_popover_new (relative_to);
  gtk_popover_bind_model (GTK_POPOVER (popover), model, NULL);

  return popover;
}

GtkWidget *
gtk_popover_get_contents_widget (GtkPopover *popover)
{
  GtkPopoverPrivate *priv = gtk_popover_get_instance_private (popover);

  return priv->contents_widget;
}
