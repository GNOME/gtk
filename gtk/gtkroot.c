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

static GQuark quark_restyle_pending;
static GQuark quark_resize_handler;

G_DEFINE_INTERFACE_WITH_CODE (GtkRoot, gtk_root, GTK_TYPE_WIDGET,
                              g_type_interface_add_prerequisite (g_define_type_id, GTK_TYPE_NATIVE))

static GdkDisplay *
gtk_root_default_get_display (GtkRoot *self)
{
  return gdk_display_get_default ();
}


static GtkConstraintSolver *
gtk_root_default_get_constraint_solver (GtkRoot *self)
{
  return NULL;
}

static GtkWidget *
gtk_root_default_get_focus (GtkRoot *self)
{
  return NULL;
}

static void
gtk_root_default_set_focus (GtkRoot   *self,
                            GtkWidget *focus)
{
}

static void
gtk_root_default_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_root_default_get_display;
  iface->get_constraint_solver = gtk_root_default_get_constraint_solver;
  iface->get_focus = gtk_root_default_get_focus;
  iface->set_focus = gtk_root_default_set_focus;

  quark_restyle_pending = g_quark_from_static_string ("gtk-root-restyle-pending");
  quark_resize_handler = g_quark_from_static_string ("gtk-root-resize-handler");
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

GtkConstraintSolver *
gtk_root_get_constraint_solver (GtkRoot *self)
{
  GtkRootInterface *iface;

  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  iface = GTK_ROOT_GET_IFACE (self);
  return iface->get_constraint_solver (self);
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

  GTK_ROOT_GET_IFACE (self)->set_focus (self, focus);
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
  g_return_val_if_fail (GTK_IS_ROOT (self), NULL);

  return GTK_ROOT_GET_IFACE (self)->get_focus (self);
}

static gboolean
gtk_root_needs_layout (GtkRoot *self)
{
  if (g_object_get_qdata (G_OBJECT (self), quark_restyle_pending))
    return TRUE;

  return gtk_widget_needs_allocate (GTK_WIDGET (self));
}

static void
gtk_root_layout_cb (GdkFrameClock *clock,
                    GtkRoot       *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  /* We validate the style contexts in a single loop before even trying
   * to handle resizes instead of doing validations inline.
   * This is mostly necessary for compatibility reasons with old code,
   * because both css_changed and size_allocate functions often change
   * styles and so could cause infinite loops in this function.
   *
   * It's important to note that even an invalid style context returns
   * sane values. So the result of an invalid style context will never be
   * a program crash, but only a wrong layout or rendering.
   */
  if (g_object_get_qdata (G_OBJECT (self), quark_restyle_pending))
    {
      g_object_set_qdata (G_OBJECT (self), quark_restyle_pending, NULL);
      gtk_css_node_validate (gtk_widget_get_css_node (widget));
    }

  /* we may be invoked with a container_resize_queue of NULL, because
   * queue_resize could have been adding an extra idle function while
   * the queue still got processed. we better just ignore such case
   * than trying to explicitly work around them with some extra flags,
   * since it doesn't cause any actual harm.
   */
  if (gtk_widget_needs_allocate (widget))
    gtk_native_check_resize (GTK_NATIVE (self));

  if (!gtk_root_needs_layout (self))
    gtk_root_stop_layout (self);
  else
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

void
gtk_root_start_layout (GtkRoot *self)
{
  GdkFrameClock *clock;
  guint resize_handler;

  if (g_object_get_qdata (G_OBJECT (self), quark_resize_handler))
    return;

  if (!gtk_root_needs_layout (self))
    return;

  clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));
  if (clock == NULL)
    return;

  resize_handler = g_signal_connect (clock, "layout",
                                     G_CALLBACK (gtk_root_layout_cb), self);
  g_object_set_qdata (G_OBJECT (self), quark_resize_handler, GINT_TO_POINTER (resize_handler));

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}

void
gtk_root_stop_layout (GtkRoot *self)
{
  GdkFrameClock *clock;
  guint resize_handler;

  resize_handler = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (self), quark_resize_handler));

  if (resize_handler == 0)
    return;

  clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));
  g_signal_handler_disconnect (clock, resize_handler);
  g_object_set_qdata (G_OBJECT (self), quark_resize_handler, NULL);
}

void
gtk_root_queue_restyle (GtkRoot *self)
{
  if (g_object_get_qdata (G_OBJECT (self), quark_restyle_pending))
    return;

  g_object_set_qdata (G_OBJECT (self), quark_restyle_pending, GINT_TO_POINTER (1));

  gtk_root_start_layout (self);
}

