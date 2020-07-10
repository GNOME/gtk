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
#include "gdkinternal-quartz.h"

typedef enum
{
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101200
  GDK_QUARTZ_POINTER_DEVICE_TYPE_CURSOR = NSCursorPointingDevice,
  GDK_QUARTZ_POINTER_DEVICE_TYPE_ERASER = NSEraserPointingDevice,
  GDK_QUARTZ_POINTER_DEVICE_TYPE_PEN = NSPenPointingDevice,
#else
  GDK_QUARTZ_POINTER_DEVICE_TYPE_CURSOR = NSPointingDeviceTypeCursor,
  GDK_QUARTZ_POINTER_DEVICE_TYPE_ERASER = NSPointingDeviceTypeEraser,
  GDK_QUARTZ_POINTER_DEVICE_TYPE_PEN = NSPointingDeviceTypePen,
#endif
} GdkQuartzPointerDeviceType;

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
  return g_object_new (GDK_TYPE_QUARTZ_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
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
  GdkQuartzDeviceManagerCore *self;
  GList *devices = NULL;
  GList *l;

  self = GDK_QUARTZ_DEVICE_MANAGER_CORE (device_manager);

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      devices = g_list_prepend (devices, self->core_keyboard);
      devices = g_list_prepend (devices, self->core_pointer);
    }

  for (l = self->known_tablet_devices; l; l = g_list_next (l))
    {
      devices = g_list_prepend (devices, GDK_DEVICE (l->data));
    }

  devices = g_list_reverse (devices);

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
create_core_device (GdkDeviceManager *device_manager,
                    const gchar      *device_name,
                    GdkInputSource    source)
{
  GdkDisplay *display = gdk_device_manager_get_display (device_manager);
  GdkDevice *device = g_object_new (GDK_TYPE_QUARTZ_DEVICE_CORE,
                                    "name", device_name,
                                    "type", GDK_DEVICE_TYPE_SLAVE,
                                    "input-source", source,
                                    "input-mode", GDK_MODE_DISABLED,
                                    "has-cursor", FALSE,
                                    "display", display,
                                    "device-manager", device_manager,
                                    NULL);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_PRESSURE, 0.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_XTILT, -1.0, 1.0, 0.001);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_YTILT, -1.0, 1.0, 0.001);

  return device;
}

static void
mimic_device_axes (GdkDevice *logical,
                   GdkDevice *physical)
{
  double axis_min, axis_max, axis_resolution;
  GdkAtom axis_label;
  GdkAxisUse axis_use;
  int axis_count;
  int i;

  axis_count = gdk_device_get_n_axes (physical);

  for (i = 0; i < axis_count; i++)
    {
      _gdk_device_get_axis_info (physical, i, &axis_label, &axis_use, &axis_min,
                                 &axis_max, &axis_resolution);
      _gdk_device_add_axis (logical, axis_label, axis_use, axis_min,
                            axis_max, axis_resolution);
    }
}

static void
translate_device_axes (GdkDevice *source_device,
                       gboolean   active)
{
  GdkSeat *seat = gdk_display_get_default_seat (_gdk_display);
  GdkDevice *core_pointer = gdk_seat_get_pointer (seat);

  g_object_freeze_notify (G_OBJECT (core_pointer));

  _gdk_device_reset_axes (core_pointer);
  if (active && source_device)
    {
      mimic_device_axes (core_pointer, source_device);
    }
  else
    {
      _gdk_device_add_axis (core_pointer, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
      _gdk_device_add_axis (core_pointer, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
    }

  g_object_thaw_notify (G_OBJECT (core_pointer));
}

void
_gdk_quartz_device_manager_register_device_for_ns_event (GdkDeviceManager *device_manager,
                                                         NSEvent          *nsevent)
{
  GdkQuartzDeviceManagerCore *self = GDK_QUARTZ_DEVICE_MANAGER_CORE (device_manager);
  GList *l = NULL;
  GdkInputSource input_source = GDK_SOURCE_MOUSE;
  GdkDevice *device = NULL;

  /* Only handle device updates for proximity events */
  if ([nsevent type] != GDK_QUARTZ_EVENT_TABLET_PROXIMITY &&
      [nsevent subtype] != GDK_QUARTZ_EVENT_SUBTYPE_TABLET_PROXIMITY)
    return;

  if ([nsevent pointingDeviceType] == GDK_QUARTZ_POINTER_DEVICE_TYPE_PEN)
    input_source = GDK_SOURCE_PEN;
  else if ([nsevent pointingDeviceType] == GDK_QUARTZ_POINTER_DEVICE_TYPE_CURSOR)
    input_source = GDK_SOURCE_CURSOR;
  else if ([nsevent pointingDeviceType] == GDK_QUARTZ_POINTER_DEVICE_TYPE_ERASER)
    input_source = GDK_SOURCE_ERASER;

  for (l = self->known_tablet_devices; l; l = g_list_next (l))
    {
      GdkDevice *device_to_check = GDK_DEVICE (l->data);

      if (input_source == gdk_device_get_source (device_to_check) &&
          [nsevent uniqueID] == _gdk_quartz_device_core_get_unique (device_to_check))
        {
          device = device_to_check;
          if ([nsevent isEnteringProximity])
            {
              if (!_gdk_quartz_device_core_is_active (device, [nsevent deviceID]))
                self->num_active_devices++;

              _gdk_quartz_device_core_set_active (device, TRUE, [nsevent deviceID]);
            }
          else
            {
              if (_gdk_quartz_device_core_is_active (device, [nsevent deviceID]))
                self->num_active_devices--;

              _gdk_quartz_device_core_set_active (device, FALSE, [nsevent deviceID]);
            }
        }
    }

  /* If we haven't seen this device before, add it */
  if (!device)
    {
      GdkSeat *seat;

      switch (input_source)
        {
        case GDK_SOURCE_PEN:
          device = create_core_device (device_manager,
                                       "Quartz Pen",
                                       GDK_SOURCE_PEN);
          break;
        case GDK_SOURCE_CURSOR:
          device = create_core_device (device_manager,
                                       "Quartz Cursor",
                                       GDK_SOURCE_CURSOR);
          break;
        case GDK_SOURCE_ERASER:
          device = create_core_device (device_manager,
                                       "Quartz Eraser",
                                       GDK_SOURCE_ERASER);
          break;
        default:
          g_warning ("GDK Quarz unknown input source: %i", input_source);
          break;
        }

      _gdk_device_set_associated_device (GDK_DEVICE (device), self->core_pointer);
      _gdk_device_add_slave (self->core_pointer, GDK_DEVICE (device));

      seat = gdk_device_get_seat (self->core_pointer);
      gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), device);

      _gdk_quartz_device_core_set_unique (device, [nsevent uniqueID]);
      _gdk_quartz_device_core_set_active (device, TRUE, [nsevent deviceID]);

      self->known_tablet_devices = g_list_append (self->known_tablet_devices,
                                                  device);

      if ([nsevent isEnteringProximity])
        {
          if (!_gdk_quartz_device_core_is_active (device, [nsevent deviceID]))
            self->num_active_devices++;
          _gdk_quartz_device_core_set_active (device, TRUE, [nsevent deviceID]);
        }
    }

  translate_device_axes (device, [nsevent isEnteringProximity]);

  if (self->num_active_devices)
    [NSEvent setMouseCoalescingEnabled: FALSE];
  else
    [NSEvent setMouseCoalescingEnabled: TRUE];
}

GdkDevice *
_gdk_quartz_device_manager_core_device_for_ns_event (GdkDeviceManager *device_manager,
                                                     NSEvent          *nsevent)
{
  GdkQuartzDeviceManagerCore *self = GDK_QUARTZ_DEVICE_MANAGER_CORE (device_manager);
  GdkDevice *device = NULL;

  if ([nsevent type] == GDK_QUARTZ_EVENT_TABLET_PROXIMITY ||
      [nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_TABLET_PROXIMITY ||
      [nsevent subtype] == GDK_QUARTZ_EVENT_SUBTYPE_TABLET_POINT)
    {
      /* Find the device based on deviceID */
      GList *l = NULL;

      for (l = self->known_tablet_devices; l && !device; l = g_list_next (l))
        {
          GdkDevice *device_to_check = GDK_DEVICE (l->data);

          if (_gdk_quartz_device_core_is_active (device_to_check, [nsevent deviceID]))
            device = device_to_check;
        }
    }

  if (!device)
    device = self->core_pointer;

  return device;
}
