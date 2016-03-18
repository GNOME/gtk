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

#ifndef __GDK_WAYLAND_SEAT_H__
#define __GDK_WAYLAND_SEAT_H__

#include "config.h"

#include <gdk/gdkseatprivate.h>

#define GDK_TYPE_WAYLAND_SEAT         (gdk_wayland_seat_get_type ())
#define GDK_WAYLAND_SEAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeat))
#define GDK_WAYLAND_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))
#define GDK_IS_WAYLAND_SEAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WAYLAND_SEAT))
#define GDK_IS_WAYLAND_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_SEAT))
#define GDK_WAYLAND_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_SEAT, GdkWaylandSeatClass))

typedef struct _GdkWaylandSeat GdkWaylandSeat;
typedef struct _GdkWaylandSeatClass GdkWaylandSeatClass;

struct _GdkWaylandSeatClass
{
  GdkSeatClass parent_class;
};

GType gdk_wayland_seat_get_type (void) G_GNUC_CONST;

#endif /* __GDK_WAYLAND_SEAT_H__ */
