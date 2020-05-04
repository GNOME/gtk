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
#include "gdkmacosdisplay-private.h"
#include "gdkmacossurface-private.h"

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

static GdkSurface *
gdk_macos_device_surface_at_position (GdkDevice       *device,
                                      gdouble         *win_x,
                                      gdouble         *win_y,
                                      GdkModifierType *state,
                                      gboolean         get_toplevel)
{
  GdkMacosDisplay *display;
  GdkMacosSurface *surface;
  NSPoint point;
  gint x;
  gint y;

  g_assert (GDK_IS_MACOS_DEVICE (device));
  g_assert (win_x != NULL);
  g_assert (win_y != NULL);

  point = [NSEvent mouseLocation];
  display = GDK_MACOS_DISPLAY (gdk_device_get_display (device));

  if (state != NULL)
    *state = (_gdk_macos_display_get_current_keyboard_modifiers (display) |
              _gdk_macos_display_get_current_mouse_modifiers (display));

  surface = _gdk_macos_display_get_surface_at_display_coords (display, point.x, point.y, &x, &y);

  *win_x = x;
  *win_y = y;

  return GDK_SURFACE (surface);
}

static void
gdk_macos_device_class_init (GdkMacosDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->set_surface_cursor = gdk_macos_device_set_surface_cursor;
  device_class->surface_at_position = gdk_macos_device_surface_at_position;
}

static void
gdk_macos_device_init (GdkMacosDevice *self)
{
}
