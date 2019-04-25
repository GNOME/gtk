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

#include "gtkbudprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdk-private.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkbud
 * @Title: GtkBud
 * @Short_description: Interface for widgets having surfaces
 * @See_also: #GtkRoot
 *
 * #GtkBud is the interface implemented by all widgets that can provide
 * a GdkSurface for widgets to render on.
 *
 * The obvious example of a #GtkBud is #GtkWindow.
 */

G_DEFINE_INTERFACE (GtkBud, gtk_bud, GTK_TYPE_WIDGET)

static GskRenderer *
gtk_bud_default_get_renderer (GtkBud *self)
{
  return NULL;
}

static void
gtk_bud_default_get_surface_transform (GtkBud *self,
                                       int    *x,
                                       int    *y)
{
  *x = 0;
  *y = 0;
}

static void
gtk_bud_default_check_resize (GtkBud *self)
{
}

static void
gtk_bud_default_init (GtkBudInterface *iface)
{
  iface->get_renderer = gtk_bud_default_get_renderer;
  iface->get_surface_transform = gtk_bud_default_get_surface_transform;
  iface->check_resize = gtk_bud_default_check_resize;
}

GskRenderer *
gtk_bud_get_renderer (GtkBud *self)
{
  g_return_val_if_fail (GTK_IS_BUD (self), NULL);

  return GTK_BUD_GET_IFACE (self)->get_renderer (self);
}

void
gtk_bud_get_surface_transform (GtkBud *self,
                               int    *x,
                               int    *y)
{
  g_return_if_fail (GTK_IS_BUD (self));
  g_return_if_fail (x != 0);
  g_return_if_fail (y != 0);

  return GTK_BUD_GET_IFACE (self)->get_surface_transform (self, x, y);
}

void
gtk_bud_check_resize (GtkBud *self)
{
  g_return_if_fail (GTK_IS_BUD (self));

  GTK_BUD_GET_IFACE (self)->check_resize (self);
}

/**
 * gtk_bud_get_for_surface:
 * @surface: a #GdkSurface
 *
 * Finds the GtkBud associated with the surface.
 * 
 * Returns: (transfer none): the #GtkBud that is associated with @surface
 */
GtkWidget *
gtk_bud_get_for_surface (GdkSurface *surface)
{
  GtkWidget *widget;

  widget = (GtkWidget *)gdk_surface_get_widget (surface);

  if (widget && GTK_IS_BUD (widget))
    return widget;

  return NULL;
}
