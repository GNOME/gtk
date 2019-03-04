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
#include "gdk/gdkeventsprivate.h"

static GListStore *popup_list = NULL;

typedef struct {
  GdkDisplay *display;
  GskRenderer *renderer;
  GdkSurface *surface;
  GtkWidget *relative_to;
  GtkWidget *focus_widget;
  gboolean active;
  GtkWidget *default_widget;
} GtkPopupPrivate;


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
  rect.x = 0;
  rect.y = 0;
  rect.width = gtk_widget_get_width (priv->relative_to);
  rect.height = gtk_widget_get_height (priv->relative_to);
  gtk_widget_translate_coordinates (priv->relative_to, gtk_widget_get_toplevel (priv->relative_to),
                                    rect.x, rect.y, &rect.x, &rect.y);

  gdk_surface_move_to_rect (priv->surface,
                            &rect,
                            GDK_GRAVITY_SOUTH,
                            GDK_GRAVITY_NORTH,
                            GDK_ANCHOR_FLIP_Y,
                            0, 10);
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

static void
gtk_popup_root_interface_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_popup_root_get_display;
  iface->get_renderer = gtk_popup_root_get_renderer;
  iface->get_surface_transform = gtk_popup_root_get_surface_transform;
  iface->check_resize = gtk_popup_root_check_resize;
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
gtk_popup_init (GtkPopup *popup)
{
  GtkEventController *controller;

  gtk_widget_set_has_surface (GTK_WIDGET (popup), TRUE);

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
  GdkRectangle allocation;

  if (_gtk_widget_get_alloc_needed (widget))
    {
      allocation.x = 0;
      allocation.y = 0;
      allocation.width = 20; // FIXME
      allocation.height = 20;
      gtk_widget_size_allocate (widget, &allocation, -1);
      gtk_widget_queue_resize (widget);
    }

  gtk_widget_get_allocation (widget, &allocation);

#if 0
  priv->surface = gdk_surface_new_popup (priv->display, &allocation);
  // TODO xdg-popop window type
  gdk_surface_set_transient_for (priv->surface, gtk_widget_get_surface (priv->relative_to));
  gdk_surface_set_type_hint (priv->surface, GDK_SURFACE_TYPE_HINT_POPUP_MENU);
  gdk_surface_move_to_rect (priv->surface,
                            &allocation,
                            GDK_GRAVITY_SOUTH,
                            GDK_GRAVITY_NORTH,
                            GDK_ANCHOR_FLIP_Y,
                            0, 10);
#else
  priv->surface = gdk_surface_new_toplevel (priv->display, 20, 20);
#endif

  gtk_widget_set_surface (widget, priv->surface);
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
  gtk_widget_map (widget);

  gtk_popup_move_focus (widget, GTK_DIR_TAB_FORWARD);
}

static void
gtk_popup_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void
gtk_popup_map (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *child;

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->map (widget);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);

  gdk_surface_show (priv->surface);
}

static void
gtk_popup_unmap (GtkWidget *widget)
{
  GtkPopup *popup = GTK_POPUP (widget);
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GtkWidget *child;

  GTK_WIDGET_CLASS (gtk_popup_parent_class)->unmap (widget);

  gdk_surface_hide (priv->surface);

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

  switch (prop_id)
    {
    case 1 + GTK_ROOT_PROP_FOCUS_WIDGET:
      gtk_popup_set_focus (popup, g_value_get_object (value));
      break;
    case 1 + GTK_ROOT_PROP_DEFAULT_WIDGET:
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
    case 1 + GTK_ROOT_PROP_FOCUS_WIDGET:
      g_value_set_object (value, priv->focus_widget);
      break;
    case 1 + GTK_ROOT_PROP_DEFAULT_WIDGET:
      g_value_set_object (value, priv->default_widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
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

  gtk_root_install_properties (object_class, 1);

  binding_set = gtk_binding_set_by_class (klass);

  add_tab_bindings (binding_set, 0, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK, GTK_DIR_TAB_FORWARD);
  add_tab_bindings (binding_set, GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
  add_tab_bindings (binding_set, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_DIR_TAB_BACKWARD);
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
