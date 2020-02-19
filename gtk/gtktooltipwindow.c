/* GTK - The GIMP Toolkit
 * Copyright 2015  Emmanuele Bassi 
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtktooltipwindowprivate.h"

#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkaccessible.h"
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkstylecontext.h"
#include "gtkcssnodeprivate.h"

struct _GtkTooltipWindow
{
  GtkWindow parent_instance;

  GdkSurface *surface;
  GskRenderer *renderer;

  GdkSurfaceState state;
  GtkWidget *relative_to;
  GdkRectangle rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  int dx;
  int dy;
  guint surface_transform_changed_cb;

  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *custom_widget;
};

struct _GtkTooltipWindowClass
{
  GtkWindowClass parent_class;
};

static void gtk_tooltip_window_native_init (GtkNativeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTooltipWindow, gtk_tooltip_window, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_NATIVE,
                                                gtk_tooltip_window_native_init))


static GdkSurface *
gtk_tooltip_window_native_get_surface (GtkNative *native)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (native);

  return window->surface;
}

static GskRenderer *
gtk_tooltip_window_native_get_renderer (GtkNative *native)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (native);

  return window->renderer;
}

static void
gtk_tooltip_window_native_get_surface_transform (GtkNative *native,
                                                 int       *x,
                                                 int       *y)
{
  GtkStyleContext *context;
  GtkBorder margin, border, padding;

  context = gtk_widget_get_style_context (GTK_WIDGET (native));
  gtk_style_context_get_margin (context, &margin);
  gtk_style_context_get_border (context, &border);
  gtk_style_context_get_padding (context, &padding);

  *x = margin.left + border.left + padding.left;
  *y = margin.top + border.top + padding.top;
}

static GdkPopupLayout *
create_popup_layout (GtkTooltipWindow *window)
{
  GdkPopupLayout *layout;

  layout = gdk_popup_layout_new (&window->rect,
                                 window->rect_anchor,
                                 window->surface_anchor);
  gdk_popup_layout_set_anchor_hints (layout, window->anchor_hints);
  gdk_popup_layout_set_offset (layout, window->dx, window->dy);

  return layout;
}

static void
relayout_popup (GtkTooltipWindow *window)
{
  GdkPopupLayout *layout;

  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    return;

  layout = create_popup_layout (window);
  gdk_surface_present_popup (window->surface,
                             gdk_surface_get_width (window->surface),
                             gdk_surface_get_height (window->surface),
                             layout);
  gdk_popup_layout_unref (layout);
}

static void
gtk_tooltip_window_move_resize (GtkTooltipWindow *window)
{
  GtkRequisition req;

  if (window->surface)
    {
      gtk_widget_get_preferred_size (GTK_WIDGET (window), NULL, &req);
      gdk_surface_resize (window->surface, req.width, req.height);

      relayout_popup (window);
    }
}

static void
gtk_tooltip_window_native_check_resize (GtkNative *native)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (native);
  GtkWidget *widget = GTK_WIDGET (native);

  if (!_gtk_widget_get_alloc_needed (widget))
    gtk_widget_ensure_allocate (widget);
  else if (gtk_widget_get_visible (widget))
    {
      gtk_tooltip_window_move_resize (window);
      if (window->surface)
        gtk_widget_allocate (GTK_WIDGET (window),
                             gdk_surface_get_width (window->surface),
                             gdk_surface_get_height (window->surface),
                             -1, NULL);
    }
}

static void
gtk_tooltip_window_native_init (GtkNativeInterface *iface)
{
  iface->get_surface = gtk_tooltip_window_native_get_surface;
  iface->get_renderer = gtk_tooltip_window_native_get_renderer;
  iface->get_surface_transform = gtk_tooltip_window_native_get_surface_transform;
  iface->check_resize = gtk_tooltip_window_native_check_resize;
}

static void
surface_state_changed (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GdkSurfaceState new_surface_state;
  GdkSurfaceState changed_mask;

  new_surface_state = gdk_surface_get_state (window->surface);
  changed_mask = new_surface_state ^ window->state;
  window->state = new_surface_state;

  if (changed_mask & GDK_SURFACE_STATE_WITHDRAWN)
    {
      if (window->state & GDK_SURFACE_STATE_WITHDRAWN)
        gtk_widget_hide (widget);
    }
}

static void
surface_size_changed (GtkWidget *widget,
                      guint      width,
                      guint      height)
{
}

static gboolean
surface_render (GdkSurface     *surface,
                cairo_region_t *region,
                GtkWidget      *widget)
{
  gtk_widget_render (widget, surface, region);
  return TRUE;
}

static gboolean
surface_event (GdkSurface *surface,
               GdkEvent   *event,
               GtkWidget  *widget)
{
  gtk_main_do_event (event);
  return TRUE;
}

static void
gtk_tooltip_window_realize (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GdkDisplay *display;
  GdkSurface *parent;

  display = gtk_widget_get_display (window->relative_to);

  parent = gtk_native_get_surface (gtk_widget_get_native (window->relative_to));
  window->surface = gdk_surface_new_popup (display, parent, FALSE);

  gdk_surface_set_widget (window->surface, widget);

  g_signal_connect_swapped (window->surface, "notify::state", G_CALLBACK (surface_state_changed), widget);
  g_signal_connect_swapped (window->surface, "size-changed", G_CALLBACK (surface_size_changed), widget);
  g_signal_connect (window->surface, "render", G_CALLBACK (surface_render), widget);
  g_signal_connect (window->surface, "event", G_CALLBACK (surface_event), widget);

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->realize (widget);

  window->renderer = gsk_renderer_new_for_surface (window->surface);
}

static void
gtk_tooltip_window_unrealize (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->unrealize (widget);

  gsk_renderer_unrealize (window->renderer);
  g_clear_object (&window->renderer);

  g_signal_handlers_disconnect_by_func (window->surface, surface_state_changed, widget);
  g_signal_handlers_disconnect_by_func (window->surface, surface_size_changed, widget);
  g_signal_handlers_disconnect_by_func (window->surface, surface_render, widget);
  g_signal_handlers_disconnect_by_func (window->surface, surface_event, widget);
  gdk_surface_set_widget (window->surface, NULL);
  gdk_surface_destroy (window->surface);
  g_clear_object (&window->surface);
}


static void
unset_surface_transform_changed_cb (gpointer data)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (data);

  window->surface_transform_changed_cb = 0;
}

static gboolean
surface_transform_changed_cb (GtkWidget               *widget,
                              const graphene_matrix_t *transform,
                              gpointer                 user_data)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);

  relayout_popup (window);

  return G_SOURCE_CONTINUE;
}


static void
gtk_tooltip_window_map (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GdkPopupLayout *layout;
  GtkWidget *child;

  layout = create_popup_layout (window);
  gdk_surface_present_popup (window->surface,
                             gdk_surface_get_width (window->surface),
                             gdk_surface_get_height (window->surface),
                             layout);
  gdk_popup_layout_unref (layout);

  window->surface_transform_changed_cb =
    gtk_widget_add_surface_transform_changed_callback (window->relative_to,
                                                       surface_transform_changed_cb,
                                                       window,
                                                       unset_surface_transform_changed_cb);

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->map (widget);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_map (child);
}

static void
gtk_tooltip_window_unmap (GtkWidget *widget)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GtkWidget *child;

  gtk_widget_remove_surface_transform_changed_callback (window->relative_to,
                                                        window->surface_transform_changed_cb);
  window->surface_transform_changed_cb = 0;

  GTK_WIDGET_CLASS (gtk_tooltip_window_parent_class)->unmap (widget);
  gdk_surface_hide (window->surface);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child != NULL)
    gtk_widget_unmap (child);
}

static void
gtk_tooltip_window_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *minimum_baseline,
                            int            *natural_baseline)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child)
    gtk_widget_measure (child,
                        orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
gtk_tooltip_window_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (widget);
  GtkWidget *child;

  gtk_tooltip_window_move_resize (window);

  child = gtk_bin_get_child (GTK_BIN (window));

  if (child)
    gtk_widget_allocate (child, width, height, baseline, NULL);
}

static void
gtk_tooltip_window_show (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, TRUE);
  gtk_widget_realize (widget);
  gtk_tooltip_window_native_check_resize (GTK_NATIVE (widget));
  gtk_widget_map (widget);
}

static void
gtk_tooltip_window_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void size_changed (GtkWidget        *widget,
                          int               width,
                          int               height,
                          int               baseline,
                          GtkTooltipWindow *window);

static void
gtk_tooltip_window_dispose (GObject *object)
{
  GtkTooltipWindow *window = GTK_TOOLTIP_WINDOW (object);
  
  if (window->relative_to)
    {
      g_signal_handlers_disconnect_by_func (window->relative_to, size_changed, window);
      gtk_widget_unparent (GTK_WIDGET (window));
    }

  G_OBJECT_CLASS (gtk_tooltip_window_parent_class)->dispose (object);
}

static void
gtk_tooltip_window_class_init (GtkTooltipWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_tooltip_window_dispose;
  widget_class->realize = gtk_tooltip_window_realize;
  widget_class->unrealize = gtk_tooltip_window_unrealize;
  widget_class->map = gtk_tooltip_window_map;
  widget_class->unmap = gtk_tooltip_window_unmap;
  widget_class->measure = gtk_tooltip_window_measure;
  widget_class->size_allocate = gtk_tooltip_window_size_allocate;
  widget_class->show = gtk_tooltip_window_show; 
  widget_class->hide = gtk_tooltip_window_hide; 

  gtk_widget_class_set_css_name (widget_class, I_("tooltip"));
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_TOOL_TIP);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtktooltipwindow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, box);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, image);
  gtk_widget_class_bind_template_child (widget_class, GtkTooltipWindow, label);

  gtk_widget_class_set_css_name (widget_class, "tooltip");
}

static void
gtk_tooltip_window_init (GtkTooltipWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gtk_tooltip_window_new (void)
{
  return g_object_new (GTK_TYPE_TOOLTIP_WINDOW, NULL);
}

void
gtk_tooltip_window_set_label_markup (GtkTooltipWindow *window,
                                     const char       *markup)
{
  if (markup != NULL)
    {
      gtk_label_set_markup (GTK_LABEL (window->label), markup);
      gtk_widget_show (window->label);
    }
  else
    {
      gtk_widget_hide (window->label);
    }
}

void
gtk_tooltip_window_set_label_text (GtkTooltipWindow *window,
                                   const char       *text)
{
  if (text != NULL)
    {
      gtk_label_set_text (GTK_LABEL (window->label), text);
      gtk_widget_show (window->label);
    }
  else
    {
      gtk_widget_hide (window->label);
    }
}

void
gtk_tooltip_window_set_image_icon (GtkTooltipWindow *window,
                                   GdkPaintable     *paintable)
{

  if (paintable != NULL)
    {
      gtk_image_set_from_paintable (GTK_IMAGE (window->image), paintable);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_image_icon_from_name (GtkTooltipWindow *window,
                                             const char       *icon_name)
{
  if (icon_name)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (window->image), icon_name);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_image_icon_from_gicon (GtkTooltipWindow *window,
                                              GIcon            *gicon)
{
  if (gicon != NULL)
    {
      gtk_image_set_from_gicon (GTK_IMAGE (window->image), gicon);
      gtk_widget_show (window->image);
    }
  else
    {
      gtk_widget_hide (window->image);
    }
}

void
gtk_tooltip_window_set_custom_widget (GtkTooltipWindow *window,
                                      GtkWidget        *custom_widget)
{
  /* No need to do anything if the custom widget stays the same */
  if (window->custom_widget == custom_widget)
    return;

  if (window->custom_widget != NULL)
    {
      GtkWidget *custom = window->custom_widget;

      /* Note: We must reset window->custom_widget first,
       * since gtk_container_remove() will recurse into
       * gtk_tooltip_set_custom()
       */
      window->custom_widget = NULL;
      gtk_container_remove (GTK_CONTAINER (window->box), custom);
      g_object_unref (custom);
    }

  if (custom_widget != NULL)
    {
      window->custom_widget = g_object_ref (custom_widget);

      gtk_container_add (GTK_CONTAINER (window->box), custom_widget);
      gtk_widget_show (custom_widget);
      gtk_widget_hide (window->image);
      gtk_widget_hide (window->label);
    }
}

static void
size_changed (GtkWidget        *widget,
              int               width,
              int               height,
              int               baseline,
              GtkTooltipWindow *window)
{
  gtk_native_check_resize (GTK_NATIVE (window));
}

void
gtk_tooltip_window_set_relative_to (GtkTooltipWindow *window,
                                    GtkWidget        *relative_to)
{
  g_return_if_fail (GTK_WIDGET (window) != relative_to);

  if (window->relative_to == relative_to)
    return;

  g_object_ref (window);

  if (window->relative_to)
    {
      g_signal_handlers_disconnect_by_func (window->relative_to, size_changed, window);
      gtk_widget_unparent (GTK_WIDGET (window));
    }

  window->relative_to = relative_to;

  if (window->relative_to)
    {
      g_signal_connect (window->relative_to, "size-allocate", G_CALLBACK (size_changed), window);
      gtk_css_node_set_parent (gtk_widget_get_css_node (GTK_WIDGET (window)),
                               gtk_widget_get_css_node (relative_to));
      gtk_widget_set_parent (GTK_WIDGET (window), relative_to);
    }

  g_object_unref (window);
}

void
gtk_tooltip_window_position (GtkTooltipWindow *window,
                             GdkRectangle     *rect,
                             GdkGravity        rect_anchor,
                             GdkGravity        surface_anchor,
                             GdkAnchorHints    anchor_hints,
                             int               dx,
                             int               dy)
{
  window->rect = *rect;
  window->rect_anchor = rect_anchor;
  window->surface_anchor = surface_anchor;
  window->anchor_hints = anchor_hints;
  window->dx = dx;
  window->dy = dy;

  relayout_popup (window);
}

