/*
 * Copyright © 2022 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacosmonitor.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

static void
_gdk_macos_display_position_toplevel_with_parent (GdkMacosDisplay *self,
                                                  GdkMacosSurface *surface,
                                                  GdkMacosSurface *parent,
                                                  int             *x,
                                                  int             *y)
{
  GdkRectangle surface_rect;
  GdkRectangle parent_rect;
  GdkRectangle workarea;
  GdkMonitor *monitor;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (parent));

  /* If x/y is set, we should place relative to parent */
  if (GDK_SURFACE (surface)->x != 0 || GDK_SURFACE (surface)->y != 0)
    {
      *x = parent->root_x + GDK_SURFACE (surface)->x;
      *y = parent->root_y + GDK_SURFACE (surface)->y;
      return;
    }

  /* Try to center on top of the parent but also try to make the whole thing
   * visible in case that lands us under the topbar/panel/etc.
   */
  surface_rect.x = surface->root_x + surface->shadow_left;
  surface_rect.y = surface->root_y + surface->shadow_top;
  surface_rect.width = GDK_SURFACE (surface)->width - surface->shadow_left - surface->shadow_right;
  surface_rect.height = GDK_SURFACE (surface)->height - surface->shadow_top - surface->shadow_bottom;

  parent_rect.x = parent->root_x + surface->shadow_left;
  parent_rect.y = parent->root_y + surface->shadow_top;
  parent_rect.width = GDK_SURFACE (parent)->width - surface->shadow_left - surface->shadow_right;
  parent_rect.height = GDK_SURFACE (parent)->height - surface->shadow_top - surface->shadow_bottom;

  /* Try to place centered atop parent */
  surface_rect.x = parent_rect.x + ((parent_rect.width - surface_rect.width) / 2);
  surface_rect.y = parent_rect.y + ((parent_rect.height - surface_rect.height) / 2);

  /* Now make sure that we don't overlap the top-bar */
  monitor = _gdk_macos_surface_get_best_monitor (parent);
  gdk_macos_monitor_get_workarea (monitor, &workarea);

  if (surface_rect.x < workarea.x)
    surface_rect.x = workarea.x;

  if (surface_rect.y < workarea.y)
    surface_rect.y = workarea.y;

  *x = surface_rect.x - surface->shadow_left;
  *y = surface_rect.y - surface->shadow_top;
}

static void
_gdk_macos_display_position_toplevel (GdkMacosDisplay *self,
                                      GdkMacosSurface *surface,
                                      int             *x,
                                      int             *y)
{
  cairo_rectangle_int_t surface_rect;
  GdkRectangle workarea;
  GdkMonitor *monitor;
  CGPoint mouse;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));

  mouse = [NSEvent mouseLocation];
  monitor = _gdk_macos_display_get_monitor_at_display_coords (self, mouse.x, mouse.y);
  gdk_macos_monitor_get_workarea (monitor, &workarea);

  /* First place at top-left of current monitor */
  surface_rect.width = GDK_SURFACE (surface)->width - surface->shadow_left - surface->shadow_right;
  surface_rect.height = GDK_SURFACE (surface)->height - surface->shadow_top - surface->shadow_bottom;
  surface_rect.x = workarea.x + ((workarea.width - surface_rect.width) / 2);
  surface_rect.y = workarea.y + ((workarea.height - surface_rect.height) / 2);

  if (surface_rect.x < workarea.x)
    surface_rect.x = workarea.x;

  if (surface_rect.y < workarea.y)
    surface_rect.y = workarea.y;

  /* TODO: If there is another window at this same position, perhaps we should move it */

  *x = surface_rect.x - surface->shadow_left;
  *y = surface_rect.y - surface->shadow_top;
}

/*<private>
 * _gdk_macos_display_position_surface:
 *
 * Tries to position a window on a screen without landing in edges
 * and other weird areas the user can't use.
 */
void
_gdk_macos_display_position_surface (GdkMacosDisplay *self,
                                     GdkMacosSurface *surface,
                                     int             *x,
                                     int             *y)
{
  GdkSurface *transient_for;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));

  transient_for = GDK_SURFACE (surface)->transient_for;

  if (transient_for != NULL)
    _gdk_macos_display_position_toplevel_with_parent (self, surface, GDK_MACOS_SURFACE (transient_for), x, y);
  else
    _gdk_macos_display_position_toplevel (self, surface, x, y);
}
