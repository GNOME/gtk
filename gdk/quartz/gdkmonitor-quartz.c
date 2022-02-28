/*
 * Copyright © 2017 Tom Schoonjans
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "gdkmonitor-quartz.h"
#include "gdkdisplay-quartz.h"
#include "gdkinternal-quartz.h"

G_DEFINE_TYPE (GdkQuartzMonitor, gdk_quartz_monitor, GDK_TYPE_MONITOR)

static void
gdk_quartz_monitor_get_workarea (GdkMonitor   *monitor,
                                 GdkRectangle *dest)
{
  GDK_QUARTZ_ALLOC_POOL;

  NSArray *array = [NSScreen screens];
  NSScreen* screen = NULL;
  for (id obj in array)
    {
      CGDirectDisplayID screen_id =
        [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
      GdkQuartzMonitor *q_mon = GDK_QUARTZ_MONITOR (monitor);
      if (screen_id == q_mon->id)
        {
          screen = obj;
          break;
        }
    }

  if (screen)
    {
      GdkQuartzDisplay *display =
        GDK_QUARTZ_DISPLAY (gdk_monitor_get_display (monitor));
      NSRect rect = [screen visibleFrame];
      dest->x = (int)trunc (display->geometry.origin.x + rect.origin.x);
      dest->y = (int)trunc (display->geometry.origin.y -
                            rect.origin.y - rect.size.height);
      dest->width = rect.size.width;
      dest->height = rect.size.height;
    }
  else
    *dest = monitor->geometry;

  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_quartz_monitor_init (GdkQuartzMonitor *monitor)
{
}

static void
gdk_quartz_monitor_class_init (GdkQuartzMonitorClass *class)
{
  GDK_MONITOR_CLASS (class)->get_workarea = gdk_quartz_monitor_get_workarea;
}

