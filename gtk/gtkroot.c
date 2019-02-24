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
#include "gdk/gdk-private.h"

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
gtk_root_default_init (GtkRootInterface *iface)
{
  iface->get_display = gtk_root_default_get_display;
  iface->get_renderer = gtk_root_default_get_renderer;
  iface->get_surface_transform = gtk_root_default_get_surface_transform;
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
