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
#include "gdkseatdefaultprivate.h"

#include "gdkmacosdevice.h"
#include "gdkmacosseat-private.h"

GdkSeat *
_gdk_macos_seat_new (GdkMacosDisplay *display)
{
  GdkDevice *core_keyboard;
  GdkDevice *core_pointer;
  GdkSeat *seat;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  core_pointer = g_object_new (GDK_TYPE_MACOS_DEVICE,
                               "name", "Core Pointer",
                               "type", GDK_DEVICE_TYPE_MASTER,
                               "source", GDK_SOURCE_MOUSE,
                               "has-cursor", TRUE,
                               "display", display,
                               NULL);
  core_keyboard = g_object_new (GDK_TYPE_MACOS_DEVICE,
                                "name", "Core Keyboard",
                                "type", GDK_DEVICE_TYPE_MASTER,
                                "source", GDK_SOURCE_KEYBOARD,
                                "has-cursor", FALSE,
                                "display", display,
                                NULL);

  _gdk_device_set_associated_device (GDK_DEVICE (core_pointer),
                                     GDK_DEVICE (core_keyboard));
  _gdk_device_set_associated_device (GDK_DEVICE (core_keyboard),
                                     GDK_DEVICE (core_pointer));

  seat = gdk_seat_default_new_for_master_pair (core_pointer, core_keyboard);

  g_object_unref (core_pointer);
  g_object_unref (core_keyboard);

  return g_steal_pointer (&seat);
}
