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

#define GDK_TYPE_BROADWAY_SEAT         (gdk_broadway_seat_get_type ())
#define GDK_BROADWAY_SEAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_BROADWAY_SEAT, GdkBroadwaySeat))
#define GDK_IS_BROADWAY_SEAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_BROADWAY_SEAT))
#define GDK_BROADWAY_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_BROADWAY_SEAT, GdkBroadwaySeatClass))
#define GDK_IS_BROADWAY_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_BROADWAY_SEAT))
#define GDK_BROADWAY_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_BROADWAY_SEAT, GdkBroadwaySeatClass))

typedef struct _GdkBroadwaySeat GdkBroadwaySeat;
typedef struct _GdkBroadwaySeatClass GdkBroadwaySeatClass;

struct _GdkBroadwaySeat
{
  GdkSeat parent_instance;
};

struct _GdkBroadwaySeatClass
{
  GdkSeatClass parent_class;
};

GType     gdk_broadway_seat_get_type     (void) G_GNUC_CONST;

GdkSeat * gdk_broadway_seat_new_for_logical_pair    (GdkDevice     *pointer,
                                                     GdkDevice     *keyboard);

void      gdk_broadway_seat_add_physical_device     (GdkBroadwaySeat *seat,
                                                     GdkDevice       *device);
void      gdk_broadway_seat_remove_physical_device  (GdkBroadwaySeat *seat,
                                                     GdkDevice       *device);
void      gdk_broadway_seat_add_tool                (GdkBroadwaySeat *seat,
                                                     GdkDeviceTool   *tool);
void      gdk_broadway_seat_remove_tool             (GdkBroadwaySeat *seat,
                                                     GdkDeviceTool   *tool);

G_END_DECLS
