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

#ifndef __GDK_MACOS_SURFACE_PRIVATE_H__
#define __GDK_MACOS_SURFACE_PRIVATE_H__

#include "gdksurfaceprivate.h"

#include "gdkmacosdisplay.h"
#include "gdkmacossurface.h"

G_BEGIN_DECLS

struct _GdkMacosSurface
{
  GdkSurface parent_instance;
};

struct _GdkMacosSurfaceClass
{
  GdkSurfaceClass parent_class;
};

GdkMacosSurface *_gdk_macos_surface_new        (GdkMacosDisplay *display,
                                                GdkSurfaceType   surface_type,
                                                GdkSurface      *parent,
                                                int              x,
                                                int              y,
                                                int              width,
                                                int              height);
void             _gdk_macos_surface_get_shadow (GdkMacosSurface *self,
                                                gint            *top,
                                                gint            *right,
                                                gint            *bottom,
                                                gint            *left);

G_END_DECLS

#endif /* __GDK_MACOS_SURFACE_PRIVATE_H__ */
