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

#include "gdktoplevelprivate.h"

#include "gdkmacostoplevelsurface-private.h"

struct _GdkMacosToplevelSurface
{
  GdkMacosSurface parent_instance;
};

static void
toplevel_iface_init (GdkToplevelInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GdkMacosToplevelSurface, _gdk_macos_toplevel_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL, toplevel_iface_init))

static void
_gdk_macos_toplevel_surface_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gdk_macos_toplevel_surface_parent_class)->finalize (object);
}

static void
_gdk_macos_toplevel_surface_class_init (GdkMacosToplevelSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gdk_macos_toplevel_surface_finalize;

  gdk_toplevel_install_properties (object_class, 1);
}

static void
_gdk_macos_toplevel_surface_init (Gdkmacostoplevelsurface *self)
{
}

GdkMacosSurface *
_gdk_macos_toplevel_surface_new (GdkMacosDisplay *display,
                                 GdkSurface      *parent,
                                 int              x,
                                 int              y,
                                 int              width,
                                 int              height)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);
  g_return_val_if_fail (!parent || GDK_IS_MACOS_SURFACE (parent), NULL);

  return g_object_new (GDK_TYPE_MACOS_TOPLEVEL_SURFACE,
                       "display", display,
                       NULL);
}
