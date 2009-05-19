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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gdk/gdktypes.h>
#include <gdk/gdkdevicemanager.h>
#include "gdkdevicemanager-core.h"
#include "gdkinputprivate.h"

static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

static GList * gdk_device_manager_core_get_devices (GdkDeviceManager *device_manager,
                                                    GdkDeviceType     type);


G_DEFINE_TYPE (GdkDeviceManagerCore, gdk_device_manager_core, GDK_TYPE_DEVICE_MANAGER)

static void
gdk_device_manager_core_class_init (GdkDeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);

  device_manager_class->get_devices = gdk_device_manager_core_get_devices;
}

static GdkDevice *
create_core_pointer (GdkDisplay *display)
{
  GdkDevice *core_pointer;
  GdkDevicePrivate *private;

  core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  private = (GdkDevicePrivate *) core_pointer;

  core_pointer->name = "Core Pointer";
  core_pointer->source = GDK_SOURCE_MOUSE;
  core_pointer->mode = GDK_MODE_SCREEN;
  core_pointer->has_cursor = TRUE;
  core_pointer->num_axes = G_N_ELEMENTS (gdk_input_core_axes);
  core_pointer->axes = gdk_input_core_axes;
  core_pointer->num_keys = 0;
  core_pointer->keys = NULL;

  private->display = display;

  return core_pointer;
}

static void
gdk_device_manager_core_init (GdkDeviceManagerCore *device_manager)
{
  GdkDisplay *display;

  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  device_manager->core_pointer = create_core_pointer (display);
}

static GList *
gdk_device_manager_core_get_devices (GdkDeviceManager *device_manager,
                                     GdkDeviceType     type)
{
  GdkDeviceManagerCore *device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      device_manager_core = (GdkDeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, device_manager_core->core_pointer);
    }

  return devices;
}
