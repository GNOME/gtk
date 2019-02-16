/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 */

#include "config.h"

#include <gdk/gdktypes.h>
#include <gdk/gdkdevicemanager.h>
#include <gdk/gdkdeviceprivate.h>
#include <gdk/gdkseatdefaultprivate.h>
#include <gdk/gdkdevicemanagerprivate.h>
#include <gdk/gdkdisplayprivate.h>
#include "gdkdevicemanager-core-quartz.h"
#include "gdkquartzdevice-core.h"
#include "gdkkeysyms.h"
#include "gdkprivate-quartz.h"


#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    gdk_quartz_device_manager_core_finalize    (GObject *object);
static void    gdk_quartz_device_manager_core_constructed (GObject *object);

static GList * gdk_quartz_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                                            GdkDeviceType     type);
static GdkDevice * gdk_quartz_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager);


G_DEFINE_TYPE (GdkQuartzDeviceManagerCore, gdk_quartz_device_manager_core, GDK_TYPE_DEVICE_MANAGER)

static void
gdk_quartz_device_manager_core_class_init (GdkQuartzDeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_quartz_device_manager_core_finalize;
  object_class->constructed = gdk_quartz_device_manager_core_constructed;
  device_manager_class->list_devices = gdk_quartz_device_manager_core_list_devices;
  device_manager_class->get_client_pointer = gdk_quartz_device_manager_core_get_client_pointer;
}

static GdkDevice *
create_core_pointer (GdkDeviceManager *device_manager,
                     GdkDisplay       *display)
{
  GdkDevice *device = g_object_new (GDK_TYPE_QUARTZ_DEVICE_CORE,
                                    "name", "Core Pointer",
                                    "type", GDK_DEVICE_TYPE_MASTER,
                                    "input-source", GDK_SOURCE_MOUSE,
                                    "input-mode", GDK_MODE_SCREEN,
                                    "has-cursor", TRUE,
                                    "display", display,
                                    "device-manager", device_manager,
                                    NULL);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_XTILT, -1.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_YTILT, -1.0, 1.0, 0.001);

  return device;
}

static GdkDevice *
create_core_keyboard (GdkDeviceManager *device_manager,
                      GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_QUARTZ_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_quartz_device_manager_core_init (GdkQuartzDeviceManagerCore *device_manager)
{
  device_manager->known_tablet_devices = NULL;
}

static void
gdk_quartz_device_manager_core_finalize (GObject *object)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core;

  quartz_device_manager_core = GDK_QUARTZ_DEVICE_MANAGER_CORE (object);

  g_object_unref (quartz_device_manager_core->core_pointer);
  g_object_unref (quartz_device_manager_core->core_keyboard);

  g_list_free_full (quartz_device_manager_core->known_tablet_devices, g_object_unref);

  G_OBJECT_CLASS (gdk_quartz_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_quartz_device_manager_core_constructed (GObject *object)
{
  GdkQuartzDeviceManagerCore *device_manager;
  GdkDisplay *display;
  GdkSeat *seat;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);

  seat = gdk_seat_default_new_for_master_pair (device_manager->core_pointer,
                                               device_manager->core_keyboard);
  gdk_display_add_seat (display, seat);
  g_object_unref (seat);
}

static GList *
gdk_quartz_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                             GdkDeviceType     type)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core;
  GList *devices = NULL;

  quartz_device_manager_core = GDK_QUARTZ_DEVICE_MANAGER_CORE(device_manager);

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      devices = g_list_prepend (devices, quartz_device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, quartz_device_manager_core->core_pointer);
    }

  GList *devices_iter;
  for (devices_iter = quartz_device_manager_core->known_tablet_devices;
       devices_iter;
       devices_iter = g_list_next (devices_iter))
    {
      devices = g_list_append (devices, GDK_DEVICE(devices_iter->data));
    }

  return devices;
}

static GdkDevice *
gdk_quartz_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core;

  quartz_device_manager_core = (GdkQuartzDeviceManagerCore *) device_manager;
  return quartz_device_manager_core->core_pointer;
}

static GdkDevice *
gdk_quartz_device_manager_core_create_device(GdkDeviceManager *device_manager,
                                             const gchar *device_name,
                                             GdkInputSource source)
{
  GdkDisplay *display = gdk_device_manager_get_display (device_manager);
  GdkDevice *device = g_object_new (GDK_TYPE_QUARTZ_DEVICE_CORE,
                         "name", device_name,
                         "type", GDK_DEVICE_TYPE_SLAVE,
                         "input-source", source,
                         "input-mode", GDK_MODE_SCREEN,
                         "has-cursor", TRUE,
                         "display", display,
                         "device-manager", device_manager,
                         /* "cocoa-unique-id", [nsevent uniqueID], */
                         NULL);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_XTILT, -1.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_YTILT, -1.0, 1.0, 0.001);
  
  return device;
}

GdkDevice *
_gdk_quartz_device_manager_core_device_for_ns_event (GdkDeviceManager *device_manager,
                                                     NSEvent *nsevent)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core = GDK_QUARTZ_DEVICE_MANAGER_CORE (device_manager);
  GdkDevice *device = NULL;

  if ([nsevent type] == NSTabletProximity ||
      [nsevent subtype] == NSTabletProximityEventSubtype)
    {
      GList *devices_iter = NULL;
      GdkInputSource input_source = GDK_SOURCE_MOUSE;

      if ([nsevent pointingDeviceType] == NSPenPointingDevice)
        input_source = GDK_SOURCE_PEN;
      else if ([nsevent pointingDeviceType] == NSCursorPointingDevice)
        input_source = GDK_SOURCE_CURSOR;
      else if ([nsevent pointingDeviceType] == NSEraserPointingDevice)
        input_source = GDK_SOURCE_ERASER;

      for (devices_iter = quartz_device_manager_core->known_tablet_devices;
           devices_iter;
           devices_iter = g_list_next (devices_iter))
        {
          GdkDevice *device_to_check = GDK_DEVICE(devices_iter->data);

          if (input_source == gdk_device_get_source (device_to_check) &&
              [nsevent uniqueID] == gdk_quartz_device_core_get_unique (device_to_check))
            {
              device = device_to_check;
              if ([nsevent isEnteringProximity])
                {
                  if (!gdk_quartz_device_core_is_active(device), [nsevent deviceID])
                    quartz_device_manager_core->num_active_devices++;

                  gdk_quartz_device_core_set_active(device, TRUE, [nsevent deviceID]);
                }
              else
                {
                  if (gdk_quartz_device_core_is_active(device), [nsevent deviceID])
                    quartz_device_manager_core->num_active_devices--;

                  gdk_quartz_device_core_set_active(device, FALSE, [nsevent deviceID]);
                }
            }
        }

      /* If we haven't seen this devie before, add it */
      if (!device)
        {
          switch (input_source)
            {
            case GDK_SOURCE_PEN:
              device = gdk_quartz_device_manager_core_create_device(device_manager,
                                                                    "Quartz Pen",
                                                                    GDK_SOURCE_PEN);
              break;
            case GDK_SOURCE_CURSOR:
              device = gdk_quartz_device_manager_core_create_device(device_manager,
                                                                    "Quartz Cursor",
                                                                    GDK_SOURCE_CURSOR);
              break;
            case GDK_SOURCE_ERASER:
              device = gdk_quartz_device_manager_core_create_device(device_manager,
                                                                    "Quartz Eraser",
                                                                    GDK_SOURCE_ERASER);
              break;
            default:
              g_warning("GDK Quarz unknown input source: %i", input_source);
              break;
            }
            
          _gdk_device_set_associated_device (GDK_DEVICE (device), quartz_device_manager_core->core_pointer);
          _gdk_device_add_slave (quartz_device_manager_core->core_pointer, GDK_DEVICE (device));

          gdk_quartz_device_core_set_unique (device, [nsevent uniqueID]);
          gdk_quartz_device_core_set_active (device, TRUE, [nsevent deviceID]);

          quartz_device_manager_core->known_tablet_devices = g_list_append (quartz_device_manager_core->known_tablet_devices,
                                                                   device);

          if ([nsevent isEnteringProximity])
            {
              if (!gdk_quartz_device_core_is_active(device), [nsevent deviceID])
                quartz_device_manager_core->num_active_devices++;
              gdk_quartz_device_core_set_active (device, TRUE, [nsevent deviceID]);
            }
        }

      if (quartz_device_manager_core->num_active_devices)
        [NSEvent setMouseCoalescingEnabled: FALSE];
      else
        [NSEvent setMouseCoalescingEnabled: TRUE];
    }
  else if ([nsevent subtype] == NSTabletPointEventSubtype)
    {
      /* Find the device based on deviceID */
      GList *devices_iter = NULL;
      for (devices_iter = quartz_device_manager_core->known_tablet_devices;
           devices_iter && !device;
           devices_iter = g_list_next(devices_iter))
        {
          GdkDevice *device_to_check = GDK_DEVICE(devices_iter->data);

          if (gdk_quartz_device_core_is_active(device_to_check, [nsevent deviceID]))
            {
              device = device_to_check;
            }
        }
    }

  if (!device)
    device = quartz_device_manager_core->core_pointer;

  return device;
}
