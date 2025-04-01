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
#include "gdkdisplay-wayland.h"

void       _gdk_wayland_display_deliver_event (GdkDisplay *display, GdkEvent *event);
void       gdk_wayland_display_install_gsources (GdkWaylandDisplay *display_wayland);
void       gdk_wayland_display_uninstall_gsources (GdkWaylandDisplay *display_wayland);
void       _gdk_wayland_display_queue_events (GdkDisplay *display);
