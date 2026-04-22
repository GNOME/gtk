/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2025 Canonical Ltd.
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
 * Author: Alessandro Astone <alessandro.astone@canonical.com>
 */

#include "gdkseatdefaultprivate.h"

#define GDK_TYPE_X11_SEAT_XI2 (gdk_x11_seat_xi2_get_type ())
G_DECLARE_FINAL_TYPE (GdkX11SeatXI2, gdk_x11_seat_xi2, GDK, X11_SEAT_XI2, GdkSeatDefault)

GdkSeat *gdk_x11_seat_xi2_new_for_logical_pair (GdkDevice *pointer,
                                                GdkDevice *keyboard);

GdkDevice *gdk_x11_seat_xi2_get_logical_touch (GdkX11SeatXI2 *seat);
void       gdk_x11_seat_xi2_set_logical_touch (GdkX11SeatXI2 *seat, GdkDevice *device);
