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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkdragsurfaceprivate.h"

#include "gdkmacosdragsurface-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosDragSurface
{
  GdkMacosSurface parent_instance;
};

struct _GdkMacosDragSurfaceClass
{
  GdkMacosSurfaceClass parent_instance;
};

static gboolean
_gdk_macos_drag_surface_present (GdkDragSurface *surface,
                                 int             width,
                                 int             height)
{
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  _gdk_macos_surface_move_resize (GDK_MACOS_SURFACE (surface),
                                  -1, -1,
                                  width, height);

  if (!GDK_SURFACE_IS_MAPPED (GDK_SURFACE (surface)))
    _gdk_macos_surface_show (GDK_MACOS_SURFACE (surface));

  return GDK_SURFACE_IS_MAPPED (GDK_SURFACE (surface));
}

static void
drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = _gdk_macos_drag_surface_present;
}

G_DEFINE_TYPE_WITH_CODE (GdkMacosDragSurface, _gdk_macos_drag_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE, drag_surface_iface_init))

static void
_gdk_macos_drag_surface_class_init (GdkMacosDragSurfaceClass *klass)
{
}

static void
_gdk_macos_drag_surface_init (GdkMacosDragSurface *self)
{
}

GdkMacosSurface *
_gdk_macos_drag_surface_new (GdkMacosDisplay *display,
                             GdkFrameClock   *frame_clock,
                             int              x,
                             int              y,
                             int              width,
                             int              height)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosWindow *window;
  GdkMacosSurface *self;
  NSScreen *screen;
  NSUInteger style_mask;
  NSRect content_rect;
  NSRect screen_rect;
  int nx;
  int ny;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);
  g_return_val_if_fail (!frame_clock || GDK_IS_FRAME_CLOCK (frame_clock), NULL);

  style_mask = NSWindowStyleMaskBorderless;

  _gdk_macos_display_to_display_coords (display, x, y, &nx, &ny);

  screen = _gdk_macos_display_get_screen_at_display_coords (display, nx, ny);
  screen_rect = [screen frame];
  nx -= screen_rect.origin.x;
  ny -= screen_rect.origin.y;
  content_rect = NSMakeRect (nx, ny - height, width, height);

  window = [[GdkMacosWindow alloc] initWithContentRect:content_rect
                                             styleMask:style_mask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO
                                                screen:screen];

  [window setOpaque:NO];
  [window setBackgroundColor:[NSColor clearColor]];
  [window setDecorated:NO];

  self = g_object_new (GDK_TYPE_MACOS_DRAG_SURFACE,
                       "display", display,
                       "frame-clock", frame_clock,
                       "native", window,
                       NULL);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&self);
}

void
_gdk_macos_drag_surface_drag_motion (GdkMacosDragSurface *self,
                                     int                  x_root,
                                     int                  y_root,
                                     GdkDragAction        suggested_action,
                                     GdkDragAction        possible_actions,
                                     guint32              evtime)
{
  g_return_if_fail (GDK_IS_MACOS_DRAG_SURFACE (self));

  _gdk_macos_surface_move (GDK_MACOS_SURFACE (self), x_root, y_root);
}
