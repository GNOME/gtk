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

#include "gdkdevicetoolprivate.h"
#include "gdkdevice-xi2-private.h"
#include "gdkdisplayprivate.h"
#include "gdkseat-xi2-private.h"

struct _GdkX11SeatXI2
{
  GdkSeat parent_instance;

  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;
  GdkDevice *logical_touch;
  GList *physical_pointers;
  GList *physical_keyboards;
  GdkSeatCapabilities capabilities;

  GPtrArray *tools;
};

G_DEFINE_TYPE (GdkX11SeatXI2, gdk_x11_seat_xi2, GDK_TYPE_SEAT)

static GdkDevice *gdk_x11_seat_xi2_get_logical_device (GdkSeat             *seat,
                                                       GdkSeatCapabilities  capability);
static GList *gdk_x11_seat_xi2_get_devices (GdkSeat             *seat,
                                            GdkSeatCapabilities  capabilities);
static GdkGrabStatus gdk_x11_seat_xi2_grab (GdkSeat    *seat,
                                            GdkSurface *surface);
static void gdk_x11_seat_xi2_ungrab (GdkSeat *seat);

static void
gdk_x11_seat_xi2_dispose (GObject *object)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (object);
  GList *l;

  if (seat_xi2->logical_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (seat_xi2), seat_xi2->logical_pointer);
      g_clear_object (&seat_xi2->logical_pointer);
    }

  if (seat_xi2->logical_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (seat_xi2), seat_xi2->logical_keyboard);
      g_clear_object (&seat_xi2->logical_pointer);
    }

  if (seat_xi2->logical_touch)
    {
      gdk_seat_device_removed (GDK_SEAT (seat_xi2), seat_xi2->logical_touch);
      g_clear_object (&seat_xi2->logical_touch);
    }

  for (l = seat_xi2->physical_pointers; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat_xi2), l->data);
      g_object_unref (l->data);
    }

  for (l = seat_xi2->physical_keyboards; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat_xi2), l->data);
      g_object_unref (l->data);
    }

  g_clear_pointer (&seat_xi2->tools, g_ptr_array_unref);

  g_list_free (seat_xi2->physical_pointers);
  g_list_free (seat_xi2->physical_keyboards);
  seat_xi2->physical_pointers = NULL;
  seat_xi2->physical_keyboards = NULL;

  G_OBJECT_CLASS (gdk_x11_seat_xi2_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_x11_seat_xi2_get_capabilities (GdkSeat *seat)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);

  return seat_xi2->capabilities;
}

static GList *
gdk_x11_seat_xi2_get_tools (GdkSeat *seat)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);
  GdkDeviceTool *tool;
  GList *tools = NULL;
  guint i;

  if (!seat_xi2->tools)
    return NULL;

  for (i = 0; i < seat_xi2->tools->len; i++)
    {
      tool = g_ptr_array_index (seat_xi2->tools, i);
      tools = g_list_prepend (tools, tool);
    }

  return tools;
}

static void
gdk_x11_seat_xi2_class_init (GdkX11SeatXI2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_x11_seat_xi2_dispose;

  seat_class->get_capabilities = gdk_x11_seat_xi2_get_capabilities;
  seat_class->get_logical_device = gdk_x11_seat_xi2_get_logical_device;
  seat_class->get_devices = gdk_x11_seat_xi2_get_devices;
  seat_class->get_tools = gdk_x11_seat_xi2_get_tools;
  seat_class->grab = gdk_x11_seat_xi2_grab;
  seat_class->ungrab = gdk_x11_seat_xi2_ungrab;
}

static void
gdk_x11_seat_xi2_init (GdkX11SeatXI2 *seat)
{
}

static void
gdk_x11_seat_xi2_init_for_logical_pair (GdkX11SeatXI2 *seat_xi2,
                                        GdkDevice     *pointer,
                                        GdkDevice     *keyboard)
{
  seat_xi2->logical_pointer = g_object_ref (pointer);
  seat_xi2->logical_keyboard = g_object_ref (keyboard);

  gdk_seat_device_added (GDK_SEAT (seat_xi2), seat_xi2->logical_pointer);
  gdk_seat_device_added (GDK_SEAT (seat_xi2), seat_xi2->logical_keyboard);
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

  gdk_x11_seat_xi2_init_for_logical_pair (GDK_X11_SEAT_XI2 (seat), pointer, keyboard);

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

  /* There must be only one flag set */
  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return seat_xi2->logical_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return seat_xi2->logical_keyboard;
    default:
      g_warning ("Unhandled capability %x", capability);
      break;
    }

  return NULL;
}

static GdkSeatCapabilities
device_get_capability (GdkDevice *device)
{
  GdkInputSource source;

  source = gdk_device_get_source (device);

  switch (source)
    {
    case GDK_SOURCE_KEYBOARD:
      return GDK_SEAT_CAPABILITY_KEYBOARD;
    case GDK_SOURCE_TOUCHSCREEN:
      return GDK_SEAT_CAPABILITY_TOUCH;
    case GDK_SOURCE_PEN:
      return GDK_SEAT_CAPABILITY_TABLET_STYLUS;
    case GDK_SOURCE_TABLET_PAD:
      return GDK_SEAT_CAPABILITY_TABLET_PAD;
    case GDK_SOURCE_MOUSE:
    case GDK_SOURCE_TOUCHPAD:
    case GDK_SOURCE_TRACKPOINT:
    default:
      return GDK_SEAT_CAPABILITY_POINTER;
    }

  return GDK_SEAT_CAPABILITY_NONE;
}

static GList *
append_filtered (GList               *list,
                 GList               *devices,
                 GdkSeatCapabilities  capabilities)
{
  GList *l;

  for (l = devices; l; l = l->next)
    {
      GdkSeatCapabilities device_cap;

      device_cap = device_get_capability (l->data);

      if ((device_cap & capabilities) != 0)
        list = g_list_prepend (list, l->data);
    }

  return list;
}

static GList *
gdk_x11_seat_xi2_get_devices (GdkSeat             *seat,
                              GdkSeatCapabilities  capabilities)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);
  GList *devices = NULL;

  if (capabilities & (GDK_SEAT_CAPABILITY_ALL_POINTING))
    devices = append_filtered (devices, seat_xi2->physical_pointers, capabilities);

  if (capabilities & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    devices = append_filtered (devices, seat_xi2->physical_keyboards, capabilities);

  if (capabilities & GDK_SEAT_CAPABILITY_TOUCH && seat_xi2->logical_touch)
    devices = g_list_prepend (devices, seat_xi2->logical_touch);

  return devices;
}

static GdkGrabStatus
gdk_x11_seat_xi2_grab (GdkSeat    *seat,
                       GdkSurface *surface)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);
  guint32 evtime = GDK_CURRENT_TIME;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  GdkDisplay *display;
  gulong serial;
  gboolean was_visible;

  was_visible = gdk_surface_get_mapped (surface);

  status = gdk_x11_device_xi2_grab (seat_xi2->logical_pointer, surface,
                                    TRUE,
                                    NULL,
                                    evtime);
  if (status != GDK_GRAB_SUCCESS)
    return status;

  status = gdk_device_grab (seat_xi2->logical_pointer, surface);

  if (status == GDK_GRAB_SUCCESS)
    {
      status = gdk_x11_device_xi2_grab (seat_xi2->logical_keyboard, surface,
                                        TRUE,
                                        NULL,
                                        evtime);

      if (status == GDK_GRAB_SUCCESS)
        {
          status = gdk_device_grab (seat_xi2->logical_keyboard, surface);
        }

      if (status != GDK_GRAB_SUCCESS)
        {
          gdk_x11_device_xi2_ungrab (seat_xi2->logical_pointer, evtime);
          gdk_device_ungrab (seat_xi2->logical_pointer);
        }
    }

  if (status != GDK_GRAB_SUCCESS && !was_visible)
    gdk_surface_hide (surface);

  if (status != GDK_GRAB_SUCCESS)
    return status;

  /* Add a gdk device grab on the logical touch device */
  display = gdk_surface_get_display (surface);
  serial = _gdk_display_get_next_serial (display);

  _gdk_display_add_device_grab (display,
                                seat_xi2->logical_touch,
                                surface,
                                TRUE,
                                serial,
                                FALSE);

  return status;
}

void
gdk_x11_seat_xi2_ungrab (GdkSeat *seat)
{
  GdkX11SeatXI2 *seat_xi2 = GDK_X11_SEAT_XI2 (seat);

  gdk_x11_device_xi2_ungrab (seat_xi2->logical_pointer, GDK_CURRENT_TIME);
  gdk_x11_device_xi2_ungrab (seat_xi2->logical_keyboard, GDK_CURRENT_TIME);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gdk_device_ungrab (seat_xi2->logical_pointer);
  gdk_device_ungrab (seat_xi2->logical_keyboard);
  G_GNUC_END_IGNORE_DEPRECATIONS;

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

void
gdk_x11_seat_xi2_add_physical_device (GdkX11SeatXI2 *seat_xi2,
                                      GdkDevice     *device)
{
  GdkSeatCapabilities capability;

  g_return_if_fail (GDK_IS_X11_SEAT_XI2 (seat_xi2));
  g_return_if_fail (GDK_IS_DEVICE (device));

  capability = device_get_capability (device);

  if (capability & GDK_SEAT_CAPABILITY_ALL_POINTING)
    seat_xi2->physical_pointers = g_list_prepend (seat_xi2->physical_pointers, g_object_ref (device));
  else if (capability & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    seat_xi2->physical_keyboards = g_list_prepend (seat_xi2->physical_keyboards, g_object_ref (device));
  else
    {
      g_critical ("Unhandled capability %x for device '%s'",
                  capability, gdk_device_get_name (device));
      return;
    }

  seat_xi2->capabilities |= capability;

  gdk_seat_device_added (GDK_SEAT (seat_xi2), device);
}

void
gdk_x11_seat_xi2_remove_physical_device (GdkX11SeatXI2 *seat_xi2,
                                         GdkDevice     *device)
{
  GList *l;

  g_return_if_fail (GDK_IS_X11_SEAT_XI2 (seat_xi2));
  g_return_if_fail (GDK_IS_DEVICE (device));

  if (g_list_find (seat_xi2->physical_pointers, device))
    {
      seat_xi2->physical_pointers = g_list_remove (seat_xi2->physical_pointers, device);

      seat_xi2->capabilities &= ~(GDK_SEAT_CAPABILITY_ALL_POINTING);
      for (l = seat_xi2->physical_pointers; l; l = l->next)
        seat_xi2->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat_xi2), device);
      g_object_unref (device);
    }
  else if (g_list_find (seat_xi2->physical_keyboards, device))
    {
      seat_xi2->physical_keyboards = g_list_remove (seat_xi2->physical_keyboards, device);

      seat_xi2->capabilities &= ~(GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD);
      for (l = seat_xi2->physical_keyboards; l; l = l->next)
        seat_xi2->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat_xi2), device);
      g_object_unref (device);
    }
}

void
gdk_x11_seat_xi2_add_tool (GdkX11SeatXI2 *seat_xi2,
                           GdkDeviceTool *tool)
{
  g_return_if_fail (GDK_IS_X11_SEAT_XI2 (seat_xi2));
  g_return_if_fail (tool != NULL);

  if (!seat_xi2->tools)
    seat_xi2->tools = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  g_ptr_array_add (seat_xi2->tools, g_object_ref (tool));
  g_signal_emit_by_name (seat_xi2, "tool-added", tool);
}

void
gdk_x11_seat_xi2_remove_tool (GdkX11SeatXI2 *seat_xi2,
                              GdkDeviceTool *tool)
{
  g_return_if_fail (GDK_IS_X11_SEAT_XI2 (seat_xi2));
  g_return_if_fail (tool != NULL);

  if (tool != gdk_seat_get_tool (GDK_SEAT (seat_xi2), tool->serial, tool->hw_id, tool->type))
    return;

  g_signal_emit_by_name (seat_xi2, "tool-removed", tool);
  g_ptr_array_remove (seat_xi2->tools, tool);
}
