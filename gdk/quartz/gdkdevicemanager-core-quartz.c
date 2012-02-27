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
#include <gdk/gdkdevicemanagerprivate.h>
#include "gdkdevicemanager-core-quartz.h"
#include "gdkquartzdevice-core.h"
#include "gdkkeysyms.h"


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
}

static void
gdk_quartz_device_manager_core_finalize (GObject *object)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core;

  quartz_device_manager_core = GDK_QUARTZ_DEVICE_MANAGER_CORE (object);

  g_object_unref (quartz_device_manager_core->core_pointer);
  g_object_unref (quartz_device_manager_core->core_keyboard);

  G_OBJECT_CLASS (gdk_quartz_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_quartz_device_manager_core_constructed (GObject *object)
{
  GdkQuartzDeviceManagerCore *device_manager;
  GdkDisplay *display;

  device_manager = GDK_QUARTZ_DEVICE_MANAGER_CORE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);
}

static GList *
gdk_quartz_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                             GdkDeviceType     type)
{
  GdkQuartzDeviceManagerCore *quartz_device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      quartz_device_manager_core = (GdkQuartzDeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, quartz_device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, quartz_device_manager_core->core_pointer);
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
