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

struct _GdkMacosDragSurface
{
  GdkMacosSurface parent_instance;
};

struct _GdkMacosDragSurfaceClass
{
  GdkMacosSurfaceClass parent_instance;
};

static void
drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GdkMacosDragSurface, _gdk_macos_drag_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE, drag_surface_iface_init))

static void
_gdk_macos_drag_surface_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gdk_macos_drag_surface_parent_class)->finalize (object);
}

static void
_gdk_macos_drag_surface_class_init (GdkMacosDragSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gdk_macos_drag_surface_finalize;
}

static void
_gdk_macos_drag_surface_init (GdkMacosDragSurface *self)
{
}

GdkMacosSurface *
_gdk_macos_drag_surface_new (GdkMacosDisplay *display,
                             GdkSurface      *parent,
                             GdkFrameClock   *frame_clock,
                             int              x,
                             int              y,
                             int              width,
                             int              height)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);
  g_return_val_if_fail (!frame_clock || GDK_IS_FRAME_CLOCK (frame_clock), NULL);
  g_return_val_if_fail (!parent || GDK_IS_MACOS_SURFACE (parent), NULL);

  return g_object_new (GDK_TYPE_MACOS_DRAG_SURFACE,
                       "display", display,
                       "frame-clock", frame_clock,
                       NULL);
}
