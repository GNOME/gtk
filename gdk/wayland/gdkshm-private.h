/*
 * Copyright © 2025 Red Hat, Inc
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

#include <glib.h>
#include <cairo.h>
#include "gdkdisplay-wayland.h"

cairo_surface_t * gdk_wayland_display_create_shm_surface  (GdkWaylandDisplay *display,
                                                           uint32_t           width,
                                                           uint32_t           height);
struct wl_buffer *_gdk_wayland_shm_surface_get_wl_buffer  (cairo_surface_t   *surface);
gboolean          _gdk_wayland_is_shm_surface             (cairo_surface_t   *surface);
struct wl_buffer *_gdk_wayland_shm_texture_get_wl_buffer  (GdkWaylandDisplay *display,
                                                           GdkTexture        *texture);
