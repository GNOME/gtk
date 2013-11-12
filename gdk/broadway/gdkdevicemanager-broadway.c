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

#include "gdkdevicemanager-broadway.h"

#include "gdktypes.h"
#include "gdkdevicemanager.h"
#include "gdkdevice-broadway.h"
#include "gdkkeysyms.h"
#include "gdkprivate-broadway.h"

#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    gdk_broadway_device_manager_finalize    (GObject *object);
static void    gdk_broadway_device_manager_constructed (GObject *object);

static GList * gdk_broadway_device_manager_list_devices (GdkDeviceManager *device_manager,
							 GdkDeviceType     type);
static GdkDevice * gdk_broadway_device_manager_get_client_pointer (GdkDeviceManager *device_manager);

G_DEFINE_TYPE (GdkBroadwayDeviceManager, gdk_broadway_device_manager, GDK_TYPE_DEVICE_MANAGER)

static void
gdk_broadway_device_manager_class_init (GdkBroadwayDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_broadway_device_manager_finalize;
  object_class->constructed = gdk_broadway_device_manager_constructed;
  device_manager_class->list_devices = gdk_broadway_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_broadway_device_manager_get_client_pointer;
}

static GdkDevice *
create_core_pointer (GdkDeviceManager *device_manager,
                     GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
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
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static GdkDevice *
create_touchscreen (GdkDeviceManager *device_manager,
                    GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE,
                       "name", "Touchscreen",
                       "type", GDK_DEVICE_TYPE_SLAVE,
                       "input-source", GDK_SOURCE_TOUCHSCREEN,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_broadway_device_manager_init (GdkBroadwayDeviceManager *device_manager)
{
}

static void
gdk_broadway_device_manager_finalize (GObject *object)
{
  GdkBroadwayDeviceManager *device_manager;

  device_manager = GDK_BROADWAY_DEVICE_MANAGER (object);

  g_object_unref (device_manager->core_pointer);
  g_object_unref (device_manager->core_keyboard);
  g_object_unref (device_manager->touchscreen);

  G_OBJECT_CLASS (gdk_broadway_device_manager_parent_class)->finalize (object);
}

static void
gdk_broadway_device_manager_constructed (GObject *object)
{
  GdkBroadwayDeviceManager *device_manager;
  GdkDisplay *display;

  device_manager = GDK_BROADWAY_DEVICE_MANAGER (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->touchscreen = create_touchscreen (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);
  _gdk_device_set_associated_device (device_manager->touchscreen, device_manager->core_pointer);
  _gdk_device_add_slave (device_manager->core_pointer, device_manager->touchscreen);
}


static GList *
gdk_broadway_device_manager_list_devices (GdkDeviceManager *device_manager,
					  GdkDeviceType     type)
{
  GdkBroadwayDeviceManager *broadway_device_manager = (GdkBroadwayDeviceManager *) device_manager;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      devices = g_list_prepend (devices, broadway_device_manager->core_keyboard);
      devices = g_list_prepend (devices, broadway_device_manager->core_pointer);
    }

  if (type == GDK_DEVICE_TYPE_SLAVE)
    {
      devices = g_list_prepend (devices, broadway_device_manager->touchscreen);
    }

  return devices;
}

static GdkDevice *
gdk_broadway_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkBroadwayDeviceManager *broadway_device_manager = (GdkBroadwayDeviceManager *) device_manager;

  return broadway_device_manager->core_pointer;
}

GdkDeviceManager *
_gdk_broadway_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_BROADWAY_DEVICE_MANAGER,
		       "display", display,
		       NULL);
}
