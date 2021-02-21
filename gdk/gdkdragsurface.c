/*
 * Copyright © 2020 Red Hat, Inc.
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

#include "gdk-private.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkintl.h"

/**
 * GdkDragSurface:
 *
 * A #GdkDragSurface is an interface for surfaces used during DND.
 */

/**
 * GdkDragSurfaceInterface:
 *
 * The `GdkDragSurfaceInterface` implementation is private to GDK.
 */

G_DEFINE_INTERFACE (GdkDragSurface, gdk_drag_surface, GDK_TYPE_SURFACE)

static gboolean
gdk_drag_surface_default_present (GdkDragSurface *drag_surface,
                                  int          width,
                                  int          height)
{
  return FALSE;
}

static void
gdk_drag_surface_default_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_drag_surface_default_present;
}

/**
 * gdk_drag_surface_present:
 * @drag_surface: the `GdkDragSurface` to show
 * @width: the unconstrained drag_surface width to layout
 * @height: the unconstrained drag_surface height to layout
 *
 * Present @drag_surface.
 *
 * Returns: %FALSE if it failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_drag_surface_present (GdkDragSurface *drag_surface,
                          int          width,
                          int          height)
{
  g_return_val_if_fail (GDK_IS_DRAG_SURFACE (drag_surface), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);

  return GDK_DRAG_SURFACE_GET_IFACE (drag_surface)->present (drag_surface, width, height);
}
