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

#include <gdk/gdkwindow.h>
#include "gdkdevice-core.h"

static void gdk_device_core_get_state ();

G_DEFINE_TYPE (GdkDeviceCore, gdk_device_core, GDK_TYPE_DEVICE)

static GdkDeviceAxis gdk_device_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

static void
gdk_device_core_class_init (GdkDeviceCoreClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_state = gdk_device_core_get_state;
}

static void
gdk_device_core_init (GdkDeviceCore *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  device->num_axes = G_N_ELEMENTS (gdk_device_core_axes);
  device->axes = gdk_device_core_axes;

  device->num_keys = 0;
  device->keys = NULL;
}

static void
gdk_device_core_get_state (GdkDevice       *device,
                           GdkWindow       *window,
                           gdouble         *axes,
                           GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}
