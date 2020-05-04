/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkdeviceprivate.h"

#include "gdkmacoscursor-private.h"
#include "gdkmacosdevice.h"
#include "gdkmacossurface.h"

struct _GdkMacosDevice
{
  GdkDevice parent_instance;
};

struct _GdkMacosDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkMacosDevice, gdk_macos_device, GDK_TYPE_DEVICE)

static void
gdk_macos_device_set_surface_cursor (GdkDevice  *device,
                                     GdkSurface *surface,
                                     GdkCursor  *cursor)
{
  NSCursor *nscursor;

  g_assert (GDK_IS_MACOS_DEVICE (device));
  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (!cursor || GDK_IS_CURSOR (cursor));

  nscursor = _gdk_macos_cursor_get_ns_cursor (cursor);

  if (nscursor != NULL)
    {
      [nscursor set];
      [nscursor release];
    }
}

static void
gdk_macos_device_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_macos_device_parent_class)->finalize (object);
}

static void
gdk_macos_device_class_init (GdkMacosDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  object_class->finalize = gdk_macos_device_finalize;

  device_class->set_surface_cursor = gdk_macos_device_set_surface_cursor;
}

static void
gdk_macos_device_init (GdkMacosDevice *self)
{
}
