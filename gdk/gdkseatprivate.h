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

#ifndef __GDK_SEAT_PRIVATE_H__
#define __GDK_SEAT_PRIVATE_H__

typedef struct _GdkSeatClass GdkSeatClass;

#include "gdkseat.h"

#define GDK_SEAT_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_SEAT, GdkSeatClass))
#define GDK_IS_SEAT_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_SEAT))
#define GDK_SEAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_SEAT, GdkSeatClass))

struct _GdkSeatClass
{
  GObjectClass parent_class;

  void (* device_added)   (GdkSeat   *seat,
                           GdkDevice *device);
  void (* device_removed) (GdkSeat   *seat,
                           GdkDevice *device);
  void (* device_changed) (GdkSeat   *seat,
                           GdkDevice *device);

  GdkSeatCapabilities (*get_capabilities) (GdkSeat *seat);

  GdkGrabStatus (* grab)   (GdkSeat                *seat,
                            GdkSurface              *surface,
                            GdkSeatCapabilities     capabilities,
                            gboolean                owner_events,
                            GdkCursor              *cursor,
                            const GdkEvent         *event,
                            GdkSeatGrabPrepareFunc  prepare_func,
                            gpointer                prepare_func_data);
  void          (* ungrab) (GdkSeat                *seat);

  GdkDevice * (* get_master) (GdkSeat             *seat,
                              GdkSeatCapabilities  capability);
  GList *     (* get_slaves) (GdkSeat             *seat,
                              GdkSeatCapabilities  capabilities);

  GdkDeviceTool * (* get_tool) (GdkSeat *seat,
                                guint64  serial,
                                guint64  tool_id);
  GList *     (* get_master_pointers) (GdkSeat             *seat,
                                       GdkSeatCapabilities  capabilities);
};

void gdk_seat_device_added   (GdkSeat   *seat,
                              GdkDevice *device);
void gdk_seat_device_removed (GdkSeat   *seat,
                              GdkDevice *device);

void gdk_seat_tool_added     (GdkSeat       *seat,
                              GdkDeviceTool *tool);
void gdk_seat_tool_removed   (GdkSeat       *seat,
                              GdkDeviceTool *tool);

GdkDeviceTool *
     gdk_seat_get_tool       (GdkSeat   *seat,
                              guint64    serial,
                              guint64    hw_id);

#endif /* __GDK_SEAT_PRIVATE_H__ */
