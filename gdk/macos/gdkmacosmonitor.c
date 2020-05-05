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

#include "gdkmacosdisplay-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosMonitor
{
  GdkMonitor        parent_instance;
  CGDirectDisplayID screen_id;
};

struct _GdkMacosMonitorClass
{
  GdkMonitorClass parent_class;
};

G_DEFINE_TYPE (GdkMacosMonitor, gdk_macos_monitor, GDK_TYPE_MONITOR)

static void
gdk_macos_monitor_get_workarea (GdkMonitor   *monitor,
                                GdkRectangle *geometry)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosMonitor *self = (GdkMacosMonitor *)monitor;

  g_assert (GDK_IS_MACOS_MONITOR (self));
  g_assert (geometry != NULL);

  *geometry = monitor->geometry;

  for (id obj in [NSScreen screens])
    {
      CGDirectDisplayID screen_id;

      screen_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];

      if (screen_id == self->screen_id)
        {
          NSScreen *screen = (NSScreen *)obj;
          NSRect visibleFrame = [screen visibleFrame];
          int x;
          int y;

          _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (monitor->display),
                                                  visibleFrame.origin.x,
                                                  visibleFrame.origin.y,
                                                  &x, &y);

          geometry->x = x;
          geometry->y = y;
          geometry->width = visibleFrame.size.width;
          geometry->height = visibleFrame.size.height;

          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_monitor_class_init (GdkMacosMonitorClass *klass)
{
  GdkMonitorClass *monitor_class = GDK_MONITOR_CLASS (klass);

  monitor_class->get_workarea = gdk_macos_monitor_get_workarea;
}

static void
gdk_macos_monitor_init (GdkMacosMonitor *self)
{
}

GdkMacosMonitor *
_gdk_macos_monitor_new (GdkMacosDisplay   *display,
                        CGDirectDisplayID  screen_id)
{
  GdkMacosMonitor *self;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  self = g_object_new (GDK_TYPE_MACOS_MONITOR,
                       "display", display,
                       NULL);
  self->screen_id = screen_id;

  return g_steal_pointer (&self);
}

CGDirectDisplayID
_gdk_macos_monitor_get_screen_id (GdkMacosMonitor *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_MONITOR (self), 0);

  return self->screen_id;
}
