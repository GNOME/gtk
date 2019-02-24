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
#include "gtkcssnodeprivate.h"

typedef struct {
  GdkDisplay *display;
  GskRenderer *renderer;
  GdkSurface *surface;
  GtkWidget *relative_to;
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
gtk_popup_root_interface_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_popup_root_get_display;
  iface->get_renderer = gtk_popup_root_get_renderer;
  iface->get_surface_transform = gtk_popup_root_get_surface_transform;
}

static void
gtk_popup_init (GtkPopup *popup)
{
  gtk_widget_set_has_surface (GTK_WIDGET (popup), TRUE);
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
      allocation.width = 640;
      allocation.height = 480;
      gtk_widget_size_allocate (widget, &allocation, -1);
      gtk_widget_queue_resize (widget);
    }

  gtk_widget_get_allocation (widget, &allocation);

  priv->surface = gdk_surface_new_popup (priv->display, &allocation);
  // TODO xdg-popop window type
  gdk_surface_set_transient_for (priv->surface, gtk_widget_get_surface (priv->relative_to));
  gdk_surface_set_type_hint (priv->surface, GDK_SURFACE_TYPE_HINT_POPUP_MENU);

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
gtk_popup_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_css_node_validate (gtk_widget_get_css_node (widget));
  gtk_widget_realize (widget);
  gtk_widget_map (widget);
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
  G_OBJECT_CLASS (gtk_popup_parent_class)->dispose (object);
}

static void
gtk_popup_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popup_parent_class)->finalize (object);
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
    gdk_surface_move_resize (priv->surface, 0, 0, width, height);

  child = gtk_bin_get_child (GTK_BIN (widget));
  gtk_widget_size_allocate (child, &(GtkAllocation) { 0, 0, width, height }, baseline);
}

static void
gtk_popup_class_init (GtkPopupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popup_dispose;
  object_class->finalize = gtk_popup_finalize;

  widget_class->realize = gtk_popup_realize;
  widget_class->unrealize = gtk_popup_unrealize;
  widget_class->map = gtk_popup_map;
  widget_class->unmap = gtk_popup_unmap;
  widget_class->show = gtk_popup_show;
  widget_class->hide = gtk_popup_hide;
  widget_class->measure = gtk_popup_measure;
  widget_class->size_allocate = gtk_popup_size_allocate;
}

GtkWidget *
gtk_popup_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_POPUP, NULL));
}

static void
gtk_popup_move_resize (GtkPopup *popup)
{
  GtkPopupPrivate *priv = gtk_popup_get_instance_private (popup);
  GdkRectangle rect;
 
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

  gtk_widget_allocate (GTK_WIDGET (popup), rect.width, rect.height, -1, NULL);
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

void
gtk_popup_check_resize (GtkPopup *popup)
{
  GtkWidget *widget = GTK_WIDGET (popup);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    gtk_popup_move_resize (popup);
}

