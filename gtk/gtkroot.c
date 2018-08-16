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
#include "gtknative.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdk-private.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkshortcutmanager.h"

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

G_DEFINE_INTERFACE_WITH_CODE (GtkRoot, gtk_root, GTK_TYPE_WIDGET,
                              g_type_interface_add_prerequisite (g_define_type_id, GTK_TYPE_NATIVE);
                              g_type_interface_add_prerequisite (g_define_type_id, GTK_TYPE_SHORTCUT_MANAGER));

static GdkDisplay *
gtk_root_default_get_display (GtkRoot *self)
{
  return gdk_display_get_default ();
}

static void
gtk_root_default_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_root_default_get_display;

  g_object_interface_install_property (iface,
      g_param_spec_object ("focus-widget",
                           P_("Focus widget"),
                           P_("The focus widget"),
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

/**
 * gtk_root_get_display:
 * @self: a #GtkRoot
 *
 * Returns the display that this GtkRoot is on.
 *
 * Returns: (transfer none): the display of @root
 */
GdkDisplay *
gtk_root_get_display (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_display (self);
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
