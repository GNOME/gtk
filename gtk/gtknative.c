/*
 * Copyright © 2019 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtknativeprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdk-private.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtknative
 * @Title: GtkNative
 * @Short_description: Interface for widgets having surfaces
 * @See_also: #GtkRoot
 *
 * #GtkNative is the interface implemented by all widgets that can provide
 * a GdkSurface for widgets to render on.
 *
 * The obvious example of a #GtkNative is #GtkWindow.
 */

G_DEFINE_INTERFACE (GtkNative, gtk_native, GTK_TYPE_WIDGET)

static GskRenderer *
gtk_native_default_get_renderer (GtkNative *self)
{
  return NULL;
}

static void
gtk_native_default_get_surface_transform (GtkNative *self,
                                       int    *x,
                                       int    *y)
{
  *x = 0;
  *y = 0;
}

static void
gtk_native_default_check_resize (GtkNative *self)
{
}

static void
gtk_native_default_init (GtkNativeInterface *iface)
{
  iface->get_renderer = gtk_native_default_get_renderer;
  iface->get_surface_transform = gtk_native_default_get_surface_transform;
  iface->check_resize = gtk_native_default_check_resize;
}

/**
 * gtk_native_get_surface:
 * @self: a #GtkNative
 *
 * Returns the surface of this #GtkNative.
 *
 * Returns: (transfer none): the surface of @self
 */
GdkSurface *
gtk_native_get_surface (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_surface (self);
}

/**
 * gtk_native_get_renderer:
 * @self: a #GtkNative
 *
 * Returns the renderer that is used for this #GtkNative.
 *
 * Returns: (transfer none): the renderer for @self
 */
GskRenderer *
gtk_native_get_renderer (GtkNative *self)
{
  g_return_val_if_fail (GTK_IS_NATIVE (self), NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_renderer (self);
}

void
gtk_native_get_surface_transform (GtkNative *self,
                                  int       *x,
                                  int       *y)
{
  g_return_if_fail (GTK_IS_NATIVE (self));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);

  return GTK_NATIVE_GET_IFACE (self)->get_surface_transform (self, x, y);
}

/**
 * gtk_native_check_resize:
 * @self: a #GtkNative
 *
 * Reposition and resize a #GtkNative.
 *
 * Widgets need to call this function on their attached
 * native widgets when they receive a new size allocation.
 */
void
gtk_native_check_resize (GtkNative *self)
{
  g_return_if_fail (GTK_IS_NATIVE (self));

  GTK_NATIVE_GET_IFACE (self)->check_resize (self);
}

/**
 * gtk_native_get_for_surface:
 * @surface: a #GdkSurface
 *
 * Finds the GtkNative associated with the surface.
 *
 * Returns: (transfer none): the #GtkNative that is associated with @surface
 */
GtkWidget *
gtk_native_get_for_surface (GdkSurface *surface)
{
  GtkWidget *widget;

  widget = (GtkWidget *)gdk_surface_get_widget (surface);

  if (widget && GTK_IS_NATIVE (widget))
    return widget;

  return NULL;
}
