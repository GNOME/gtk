/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdksurface-wayland.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkdragsurfaceprivate.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkglcontext-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkpopupprivate.h"
#include "gdkprivate-wayland.h"
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdktoplevelprivate.h"
#include "gdkdevice-wayland-private.h"

#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <unistd.h>

#include "gdksurface-wayland-private.h"

typedef struct
{
  GdkWaylandSurface parent_instance;
} GdkWaylandDragSurface;

typedef struct
{
  GdkWaylandSurfaceClass parent_class;
} GdkWaylandDragSurfaceClass;

static void gdk_wayland_drag_surface_iface_init (GdkDragSurfaceInterface *iface);

#define GDK_IS_WAYLAND_DRAG_SURFACE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DRAG_SURFACE))

G_DEFINE_TYPE_WITH_CODE (GdkWaylandDragSurface, gdk_wayland_drag_surface, GDK_TYPE_WAYLAND_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_DRAG_SURFACE,
                                                gdk_wayland_drag_surface_iface_init))


static void
gdk_wayland_drag_surface_init (GdkWaylandDragSurface *surface)
{
}

static gboolean
gdk_wayland_drag_surface_compute_size (GdkSurface *surface)
{
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (impl->next_layout.surface_geometry_dirty)
    {
      gdk_wayland_surface_update_size (surface,
                                       impl->next_layout.configured_width,
                                       impl->next_layout.configured_height,
                                       impl->scale);

      impl->next_layout.surface_geometry_dirty = FALSE;
    }

  return FALSE;
}

static void
gdk_wayland_drag_surface_class_init (GdkWaylandDragSurfaceClass *class)
{
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (class);

  surface_class->compute_size = gdk_wayland_drag_surface_compute_size;
}

static void
maybe_notify_mapped (GdkSurface *surface)
{
  if (surface->destroyed)
    return;

  if (!GDK_SURFACE_IS_MAPPED (surface))
    gdk_surface_set_is_mapped (surface, TRUE);
}

static gboolean
gdk_wayland_drag_surface_present (GdkDragSurface *drag_surface,
                                  int             width,
                                  int             height)
{
  GdkSurface *surface = GDK_SURFACE (drag_surface);
  GdkWaylandSurface *impl = GDK_WAYLAND_SURFACE (surface);

  if (!impl->display_server.wl_surface)
    gdk_wayland_surface_create_wl_surface (surface);

  impl->next_layout.configured_width = width;
  impl->next_layout.configured_height = height;
  impl->next_layout.surface_geometry_dirty = TRUE;
  gdk_surface_request_layout (surface);

  maybe_notify_mapped (surface);

  return TRUE;
}

static void
gdk_wayland_drag_surface_iface_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_wayland_drag_surface_present;
}

