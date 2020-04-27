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

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#include "gdksurfaceprivate.h"

#include "gdkmacosdragsurface-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

typedef struct
{
  gint shadow_top;
  gint shadow_right;
  gint shadow_bottom;
  gint shadow_left;
} GdkMacosSurfacePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdkMacosSurface, gdk_macos_surface, GDK_TYPE_SURFACE)

static void
gdk_macos_surface_finalize (GObject *object)
{
  GdkMacosSurface *self = (GdkMacosSurface *)object;

  G_OBJECT_CLASS (gdk_macos_surface_parent_class)->finalize (object);
}

static void
gdk_macos_surface_class_init (GdkMacosSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_macos_surface_finalize;
}

static void
gdk_macos_surface_init (GdkMacosSurface *self)
{
}

GdkMacosSurface *
_gdk_macos_surface_new (GdkMacosDisplay   *display,
                        GdkSurfaceType     surface_type,
                        GdkSurface        *parent,
                        int                x,
                        int                y,
                        int                width,
                        int                height)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  switch (surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
      return _gdk_macos_toplevel_surface_new (display, parent, x, y, width, height);

    case GDK_SURFACE_POPUP:
      return _gdk_macos_popup_surface_new (display, parent, x, y, width, height);

    case GDK_SURFACE_TEMP:
      return _gdk_macos_drag_surface_new (display, parent, x, y, width, height);

    default:
      return NULL;
    }
}

void
_gdk_macos_surface_get_shadow (GdkMacosSurface *self,
                               gint            *top,
                               gint            *right,
                               gint            *bottom,
                               gint            *left)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (top)
    *top = priv->shadow_top;

  if (left)
    *left = priv->shadow_left;

  if (bottom)
    *bottom = priv->shadow_bottom;

  if (right)
    *right = priv->shadow_right;
}
