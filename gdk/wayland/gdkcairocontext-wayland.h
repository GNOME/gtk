/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkconfig.h"

#include "gdkcairocontextprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_WAYLAND_CAIRO_CONTEXT (gdk_wayland_cairo_context_get_type ())

GDK_AVAILABLE_IN_ALL
GDK_DECLARE_INTERNAL_TYPE (GdkWaylandCairoContext, gdk_wayland_cairo_context, GDK, WAYLAND_CAIRO_CONTEXT, GdkCairoContext)

struct _GdkWaylandCairoContext
{
  GdkCairoContext parent_instance;

  GSList *surfaces;
  cairo_surface_t *cached_surface;
};

struct _GdkWaylandCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_END_DECLS

