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
#include "gdkscreen-quartz.h"


G_DEFINE_TYPE (GdkQuartzMonitor, gdk_quartz_monitor, GDK_TYPE_MONITOR)

static gboolean
gdk_monitor_has_fullscreen_window (GdkMonitor *monitor)
{
  GdkScreen *screen = gdk_display_get_default_screen (monitor->display);
  GList *toplevels, *l;
  GdkWindow *window;
  gboolean has_fullscreen;

  toplevels = gdk_screen_get_toplevel_windows (screen);

  has_fullscreen = FALSE;
  for (l = toplevels; l; l = l->next)
    {
      window = l->data;

      if ((gdk_window_get_state (window) & GDK_WINDOW_STATE_FULLSCREEN) == 0)
        continue;

      if (gdk_window_get_fullscreen_mode (window) == GDK_FULLSCREEN_ON_ALL_MONITORS ||
          gdk_display_get_monitor_at_window (monitor->display, window) == monitor)
        {
          has_fullscreen = TRUE;
          break;
        }
    }

  g_list_free (toplevels);

  return has_fullscreen;
}

static void
gdk_quartz_monitor_get_workarea (GdkMonitor   *monitor,
                                 GdkRectangle *dest)
{
  GdkQuartzScreen *quartz_screen = GDK_QUARTZ_SCREEN(gdk_display_get_default_screen (monitor->display));
  GdkQuartzMonitor *quartz_monitor = GDK_QUARTZ_MONITOR(monitor);

  GDK_QUARTZ_ALLOC_POOL;

  NSRect rect = [quartz_monitor->nsscreen visibleFrame];

  dest->x = rect.origin.x - quartz_screen->min_x;
  dest->y = quartz_screen->height - (rect.origin.y + rect.size.height) + quartz_screen->min_y;
  dest->width = rect.size.width;
  dest->height = rect.size.height;

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

