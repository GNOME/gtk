/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2015 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include "config.h"

#include "gdkwaylandseat.h"
#include "gdk/gdkseatprivate.h"
#include "gdkdisplay-wayland.h"

#define GDK_WAYLAND_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))
#define GDK_IS_WAYLAND_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_SEAT))
#define GDK_WAYLAND_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))

struct _GdkWaylandSeatClass
{
  GdkSeatClass parent_class;
};

void gdk_wayland_seat_clear_touchpoints (GdkWaylandSeat *seat,
                                         GdkSurface     *surface);

uint32_t _gdk_wayland_seat_get_implicit_grab_serial (GdkSeat          *seat,
                                                     GdkDevice        *event,
                                                     GdkEventSequence *sequence);
uint32_t _gdk_wayland_seat_get_last_implicit_grab_serial (GdkWaylandSeat     *seat,
                                                          GdkEventSequence **sequence);
void gdk_wayland_seat_set_global_cursor (GdkSeat   *seat,
                                         GdkCursor *cursor);
void gdk_wayland_seat_set_drag (GdkSeat        *seat,
                                GdkDrag *drag);

void       gdk_wayland_display_create_seat    (GdkWaylandDisplay     *display,
                                               uint32_t               id,
                                               struct wl_seat        *seat);
void       gdk_wayland_display_remove_seat    (GdkWaylandDisplay     *display,
                                               uint32_t               id);

