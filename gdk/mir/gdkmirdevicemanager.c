/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdevicemanagerprivate.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

typedef struct GdkMirDeviceManager      GdkMirDeviceManager;
typedef struct GdkMirDeviceManagerClass GdkMirDeviceManagerClass;

#define GDK_TYPE_MIR_DEVICE_MANAGER              (gdk_mir_device_manager_get_type ())
#define GDK_MIR_DEVICE_MANAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_DEVICE_MANAGER, GdkMirDeviceManager))
#define GDK_MIR_DEVICE_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_DEVICE_MANAGER, GdkMirDeviceManagerClass))
#define GDK_IS_MIR_DEVICE_MANAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_DEVICE_MANAGER))
#define GDK_IS_MIR_DEVICE_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_DEVICE_MANAGER))
#define GDK_MIR_DEVICE_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_DEVICE_MANAGER, GdkMirDeviceManagerClass))

struct GdkMirDeviceManager
{
  GdkDeviceManager parent_instance;

  GdkDevice *pointer;
  GdkDevice *keyboard;
};

struct GdkMirDeviceManagerClass
{
  GdkDeviceManagerClass parent_class;
};

G_DEFINE_TYPE (GdkMirDeviceManager, gdk_mir_device_manager, GDK_TYPE_DEVICE_MANAGER)

GdkDeviceManager *
_gdk_mir_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_MIR_DEVICE_MANAGER, "display", display, NULL);
}

static GList *
gdk_mir_device_manager_list_devices (GdkDeviceManager *device_manager,
                                     GdkDeviceType     type)
{
  //g_printerr ("gdk_mir_device_manager_list_devices (%u)\n", type);
  GdkMirDeviceManager *dm = GDK_MIR_DEVICE_MANAGER (device_manager);

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      GList *devices;

      devices = g_list_append (NULL, dm->keyboard);
      devices = g_list_append (devices, dm->pointer);

      return devices;
    }

  return NULL;
}

static GdkDevice *
gdk_mir_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  //g_printerr ("gdk_mir_device_manager_get_client_pointer\n");
  return GDK_MIR_DEVICE_MANAGER (device_manager)->pointer;
}

GdkDevice *
_gdk_mir_device_manager_get_keyboard (GdkDeviceManager *device_manager)
{
  return GDK_MIR_DEVICE_MANAGER (device_manager)->keyboard;
}

static void
gdk_mir_device_manager_init (GdkMirDeviceManager *device_manager)
{
  device_manager->keyboard = _gdk_mir_device_new (GDK_DEVICE_MANAGER (device_manager), "Mir Keyboard", GDK_SOURCE_KEYBOARD, FALSE);
  device_manager->pointer = _gdk_mir_device_new (GDK_DEVICE_MANAGER (device_manager), "Mir Pointer", GDK_SOURCE_MOUSE, TRUE);
}

static void
gdk_mir_device_manager_class_init (GdkMirDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);

  device_manager_class->list_devices = gdk_mir_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_mir_device_manager_get_client_pointer;
}
