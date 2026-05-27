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

#include "gdkseat.h"
#include "gdkseatprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_SEAT         (gdk_win32_seat_get_type ())
#define GDK_WIN32_SEAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WIN32_SEAT, GdkWin32Seat))
#define GDK_IS_WIN32_SEAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WIN32_SEAT))
#define GDK_WIN32_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WIN32_SEAT, GdkWin32SeatClass))
#define GDK_IS_WIN32_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WIN32_SEAT))
#define GDK_WIN32_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WIN32_SEAT, GdkWin32SeatClass))

typedef struct _GdkWin32Seat GdkWin32Seat;
typedef struct _GdkWin32SeatClass GdkWin32SeatClass;

struct _GdkWin32Seat
{
  GdkSeat parent_instance;
};

struct _GdkWin32SeatClass
{
  GdkSeatClass parent_class;
};

GType     gdk_win32_seat_get_type     (void) G_GNUC_CONST;

GdkSeat * gdk_win32_seat_new_for_logical_pair         (GdkDevice     *pointer,
                                                       GdkDevice     *keyboard);

void      gdk_win32_seat_add_physical_device          (GdkWin32Seat    *seat,
                                                       GdkDevice     *device);
void      gdk_win32_seat_remove_physical_device       (GdkWin32Seat    *seat,
                                                       GdkDevice     *device);
void      gdk_win32_seat_add_tool                     (GdkWin32Seat    *seat,
                                                       GdkDeviceTool *tool);
void      gdk_win32_seat_remove_tool                  (GdkWin32Seat    *seat,
                                                       GdkDeviceTool *tool);

G_END_DECLS
