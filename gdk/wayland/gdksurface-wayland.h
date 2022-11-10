/* gdksurface-wayland.h: Private header for GdkWaylandSurface
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkwaylandsurface.h"

G_BEGIN_DECLS

void                     gdk_wayland_toplevel_set_dbus_properties       (GdkToplevel *toplevel,
                                                                         const char  *application_id,
                                                                         const char  *app_menu_path,
                                                                         const char  *menubar_path,
                                                                         const char  *window_object_path,
                                                                         const char  *application_object_path,
                                                                         const char  *unique_bus_name);

void                     gdk_wayland_toplevel_announce_csd              (GdkToplevel *toplevel);
void                     gdk_wayland_toplevel_announce_ssd              (GdkToplevel *toplevel);

gboolean                 gdk_wayland_toplevel_inhibit_idle              (GdkToplevel *toplevel);
void                     gdk_wayland_toplevel_uninhibit_idle            (GdkToplevel *toplevel);


struct gtk_surface1 *    gdk_wayland_toplevel_get_gtk_surface           (GdkWaylandToplevel *wayland_toplevel);

void                     gdk_wayland_surface_ensure_wl_egl_window       (GdkSurface  *surface);

G_END_DECLS
