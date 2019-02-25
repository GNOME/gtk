/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkrootprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdk-private.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkroot
 * @Title: GtkRoot
 * @Short_description: Interface for root widgets
 * @See_also: #GtkWindow
 *
 * #GtkRoot is the interface implemented by all widgets that can act as a toplevel
 * widget to a hierarchy of widgets. The root widget takes care of providing the
 * connection to the windowing system and manages layout, drawing and event delivery
 * for its widget hierarchy.
 *
 * The obvious example of a #GtkRoot is #GtkWindow.
 */

G_DEFINE_INTERFACE (GtkRoot, gtk_root, GTK_TYPE_WIDGET)

static GdkDisplay *
gtk_root_default_get_display (GtkRoot *self)
{
  return gdk_display_get_default ();
}

static GskRenderer *
gtk_root_default_get_renderer (GtkRoot *self)
{
  return NULL;
}

static void
gtk_root_default_get_surface_transform (GtkRoot *self,
                                        int     *x,
                                        int     *y)
{
  *x = 0;
  *y = 0;
}

static void
gtk_root_default_check_resize (GtkRoot *self)
{
}

static void
gtk_root_default_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_root_default_get_display;
  iface->get_renderer = gtk_root_default_get_renderer;
  iface->get_surface_transform = gtk_root_default_get_surface_transform;
  iface->check_resize = gtk_root_default_check_resize;

  g_object_interface_install_property (iface,
      g_param_spec_object ("focus-widget",
                           P_("Focus widget"),
                           P_("The focus widget"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

GdkDisplay *
gtk_root_get_display (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_display (self);
}

GskRenderer *
gtk_root_get_renderer (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_renderer (self);
}

void
gtk_root_get_surface_transform (GtkRoot *self,
                                int     *x,
                                int     *y)
{
  GtkRootInterface *iface;

  g_return_if_fail (GTK_IS_ROOT (self));
  g_return_if_fail (x != 0);
  g_return_if_fail (y != 0);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_surface_transform (self, x, y);
}

/**
 * gtk_root_get_for_surface:
 * @surface: a #GdkSurface
 *
 * Finds the GtkRoot associated with the surface.
 * 
 * Returns: (transfer none): the #GtkRoot that is associated with @surface
 */
GtkWidget *
gtk_root_get_for_surface (GdkSurface *surface)
{
  GtkWidget *widget;

  widget = (GtkWidget *)gdk_surface_get_widget (surface);

  if (widget && GTK_IS_ROOT (widget))
    return widget;

  return NULL;
}

/**
 * gtk_root_set_focus:
 * @self: a #GtkRoot
 * @focus: (allow-none): widget to be the new focus widget, or %NULL
 *    to unset the focus widget
 *
 * If @focus is not the current focus widget, and is focusable, sets
 * it as the focus widget for the root. If @focus is %NULL, unsets
 * the focus widget for the root.
 *
 * To set the focus to a particular widget in the root, it is usually
 * more convenient to use gtk_widget_grab_focus() instead of this function.
 */
void
gtk_root_set_focus (GtkRoot   *self,
                    GtkWidget *focus)
{
  g_return_if_fail (GTK_IS_ROOT (self));
  g_return_if_fail (focus == NULL || GTK_IS_WIDGET (focus));

  g_object_set (self, "focus-widget", focus, NULL);
}

/**
 * gtk_root_get_focus:
 * @self: a #GtkRoot
 *
 * Retrieves the current focused widget within the root.
 *
 * Note that this is the widget that would have the focus
 * if the root is active; if the root is not focused then
 * `gtk_widget_has_focus (widget)` will be %FALSE for the
 * widget.
 *
 * Returns: (nullable) (transfer none): the currently focused widget,
 *    or %NULL if there is none.
 */
GtkWidget *
gtk_root_get_focus (GtkRoot *self)
{
  GtkWidget *focus;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  g_object_get (self, "focus-widget", &focus, NULL);

  if (focus)
    g_object_unref (focus);

  return focus;
}

guint
gtk_root_install_properties (GObjectClass *object_class,
                             guint         first_prop)
{
  g_object_class_override_property (object_class, first_prop + GTK_ROOT_PROP_FOCUS_WIDGET, "focus-widget");
  return GTK_ROOT_NUM_PROPERTIES;
}

static void
gtk_root_check_resize (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_if_fail (GTK_IS_ROOT (self));

  iface = GTK_ROOT_GET_IFACE (self);
  iface->check_resize (self);
}

static gboolean
gtk_root_needs_layout_phase (GtkRoot *root)
{
  if (gtk_css_node_is_invalid (gtk_widget_get_css_node (GTK_WIDGET (root))))
    return TRUE;

  return gtk_widget_needs_allocate (GTK_WIDGET (root));
}

static void
gtk_root_do_layout_phase (GdkFrameClock *clock,
                          GtkRoot       *root)
{
  /* We validate the style contexts in a single loop before even trying
   * to handle resizes instead of doing validations inline.
   * This is mostly necessary for compatibility reasons with old code,
   * because both style_updated and size_allocate functions often change
   * styles and so could cause infinite loops in this function.
   *
   * It's important to note that even an invalid style context returns
   * sane values. So the result of an invalid style context will never be
   * a program crash, but only a wrong layout or rendering.
   */

  if (gtk_css_node_is_invalid (gtk_widget_get_css_node (GTK_WIDGET (root))))
    gtk_css_node_validate (gtk_widget_get_css_node (GTK_WIDGET (root)));

  /* we may be invoked with a container_resize_queue of NULL, because
   * queue_resize could have been adding an extra idle function while
   * the queue still got processed. we better just ignore such case
   * than trying to explicitly work around them with some extra flags,
   * since it doesn't cause any actual harm.
   */
  if (gtk_widget_needs_allocate (GTK_WIDGET (root)))
    gtk_root_check_resize (root);

  if (!gtk_root_needs_layout_phase (root))
    gtk_root_stop_layout_phase (root);
  else
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

void
gtk_root_start_layout_phase (GtkRoot *root)
{
  GdkFrameClock *clock;
  guint resize_handler;

  resize_handler = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (root), "resize-handler"));
  if (resize_handler != 0)
    return;

  if (!gtk_root_needs_layout_phase (root))
    return;

  clock = gtk_widget_get_frame_clock (GTK_WIDGET (root));
  if (clock == NULL)
    return;

  resize_handler = g_signal_connect (clock, "layout", G_CALLBACK (gtk_root_do_layout_phase), root);
  g_object_set_data (G_OBJECT (root), "resize-handler", GUINT_TO_POINTER (resize_handler));
  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

void
gtk_root_stop_layout_phase (GtkRoot *root)
{
  guint resize_handler;

  resize_handler = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (root), "resize-handler"));
  if (resize_handler == 0)
    return;

  g_signal_handler_disconnect (gtk_widget_get_frame_clock (GTK_WIDGET (root)), resize_handler);
  g_object_set_data (G_OBJECT (root), "resize-handler", NULL);
}

void
gtk_root_queue_restyle (GtkRoot *root)
{
  g_return_if_fail (GTK_ROOT (root));

  if (!gtk_css_node_is_invalid (gtk_widget_get_css_node (GTK_WIDGET (root))))
    return;

  gtk_root_start_layout_phase (root);
}
