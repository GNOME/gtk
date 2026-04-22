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

#include "gdkdisplayprivate.h"
#include "gdkseat-xi2-private.h"

struct _GdkX11SeatXI2
{
  GdkSeatDefault parent_instance;

  GdkDevice *logical_touch;
};

G_DEFINE_TYPE (GdkX11SeatXI2, gdk_x11_seat_xi2, GDK_TYPE_SEAT_DEFAULT)

static void gdk_x11_seat_xi2_finalize (GObject *object);
static GdkDevice *gdk_x11_seat_xi2_get_logical_device (GdkSeat             *seat,
                                                       GdkSeatCapabilities  capability);
static GList *gdk_x11_seat_xi2_get_devices (GdkSeat             *seat,
                                            GdkSeatCapabilities  capabilities);
static GdkGrabStatus gdk_x11_seat_xi2_grab (GdkSeat                *seat,
                                            GdkSurface              *surface,
                                            GdkSeatCapabilities     capabilities,
                                            gboolean                owner_events,
                                            GdkCursor              *cursor,
                                            GdkEvent               *event,
                                            GdkSeatGrabPrepareFunc  prepare_func,
                                            gpointer                prepare_func_data);
static void gdk_x11_seat_xi2_ungrab (GdkSeat *seat);

static void
gdk_x11_seat_xi2_class_init (GdkX11SeatXI2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->finalize = gdk_x11_seat_xi2_finalize;

  seat_class->get_logical_device = gdk_x11_seat_xi2_get_logical_device;
  seat_class->get_devices = gdk_x11_seat_xi2_get_devices;
  seat_class->grab = gdk_x11_seat_xi2_grab;
  seat_class->ungrab = gdk_x11_seat_xi2_ungrab;
}

static void
gdk_x11_seat_xi2_init (GdkX11SeatXI2 *seat)
{
}

static void
gdk_x11_seat_xi2_finalize (GObject *object)
{
  GdkX11SeatXI2 *seat = GDK_X11_SEAT_XI2 (object);
  g_clear_object (&seat->logical_touch);

  G_OBJECT_CLASS (gdk_x11_seat_xi2_parent_class)->finalize (object);
}

GdkSeat *
gdk_x11_seat_xi2_new_for_logical_pair (GdkDevice *pointer,
                                       GdkDevice *keyboard)
{
  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_device_get_display (pointer);

  seat = g_object_new (GDK_TYPE_X11_SEAT_XI2,
                       "display", display,
                       NULL);

  gdk_seat_default_init_for_logical_pair (GDK_SEAT_DEFAULT (seat), pointer, keyboard);

  return seat;
}

GdkDevice *
gdk_x11_seat_xi2_get_logical_touch (GdkX11SeatXI2 *seat)
{
  return seat->logical_touch;
}

void
gdk_x11_seat_xi2_set_logical_touch (GdkX11SeatXI2 *seat, GdkDevice *device)
{
  GdkDevice *logical_pointer;

  g_clear_object (&seat->logical_touch);
  if (!device)
    return;

  seat->logical_touch = g_object_ref (device);
  logical_pointer = gdk_seat_get_pointer (GDK_SEAT (seat));

  _gdk_device_set_associated_device (seat->logical_touch, logical_pointer);
  gdk_seat_device_added (GDK_SEAT (seat), seat->logical_touch);
}

static GdkDevice *
gdk_x11_seat_xi2_get_logical_device (GdkSeat             *seat,
                                     GdkSeatCapabilities  capability)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);

  if (capability == GDK_SEAT_CAPABILITY_TOUCH && seat_xi2->logical_touch)
    return seat_xi2->logical_touch;

  return GDK_SEAT_CLASS (gdk_x11_seat_xi2_parent_class)->get_logical_device (seat, capability);
}

static GList *
gdk_x11_seat_xi2_get_devices (GdkSeat             *seat,
                              GdkSeatCapabilities  capabilities)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);
  GList *devices;

  devices = GDK_SEAT_CLASS (gdk_x11_seat_xi2_parent_class)->get_devices (seat, capabilities);

  if (capabilities & GDK_SEAT_CAPABILITY_TOUCH && seat_xi2->logical_touch)
    devices = g_list_prepend (devices, seat_xi2->logical_touch);

  return devices;
}

static GdkGrabStatus
gdk_x11_seat_xi2_grab (GdkSeat                *seat,
                       GdkSurface              *surface,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       GdkEvent               *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkGrabStatus status;

  /* Grab keyboard and pointer.
   * Strip the TOUCH capability to force emulated pointer events
   */
  status = GDK_SEAT_CLASS (gdk_x11_seat_xi2_parent_class)->grab (seat,
                                                                 surface,
                                                                 capabilities & ~GDK_SEAT_CAPABILITY_TOUCH,
                                                                 owner_events,
                                                                 cursor,
                                                                 event,
                                                                 prepare_func,
                                                                 prepare_func_data);

  if (status != GDK_GRAB_SUCCESS)
    return status;

  /* Add a gdk device grab on the logical touch device */
  if (capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    {
      GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);
      GdkDisplay *display;
      gulong serial;
      guint32 evtime;

      display = gdk_surface_get_display (surface);
      serial = _gdk_display_get_next_serial (display);
      evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;

      _gdk_display_add_device_grab (display,
                                    seat_xi2->logical_touch,
                                    surface,
                                    owner_events,
                                    GDK_TOUCH_MASK,
                                    serial,
                                    evtime,
                                    FALSE);
    }

  return status;
}

void
gdk_x11_seat_xi2_ungrab (GdkSeat *seat)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);

  GDK_SEAT_CLASS (gdk_x11_seat_xi2_parent_class)->ungrab (seat);

  if (seat_xi2->logical_touch)
    {
      GdkDeviceGrabInfo *touch_grab;
      GdkDisplay *display;

      display = gdk_device_get_display (seat_xi2->logical_touch);
      touch_grab = _gdk_display_get_last_device_grab (display, seat_xi2->logical_touch);

      if (touch_grab)
        {
          gulong serial = _gdk_display_get_next_serial (display);

          touch_grab->serial_end = serial;
          _gdk_display_device_grab_update (display, seat_xi2->logical_touch, serial);
        }
    }
}
