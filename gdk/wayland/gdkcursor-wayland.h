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

#include "gdkdisplay-wayland.h"
#include "gdkcursor.h"

struct wl_buffer *_gdk_wayland_cursor_get_buffer (GdkWaylandDisplay *display,
                                                  GdkCursor         *cursor,
                                                  double             desired_scale,
                                                  int               *hotspot_x,
                                                  int               *hotspot_y,
                                                  int               *w,
                                                  int               *h,
                                                  double            *scale);
