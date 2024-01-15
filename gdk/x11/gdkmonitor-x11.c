/*
 * Copyright © 2016 Red Hat, Inc
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

#include "gdkmonitor-x11.h"
#include "gdkscreen-x11.h"


G_DEFINE_TYPE (GdkX11Monitor, gdk_x11_monitor, GDK_TYPE_MONITOR)

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
gdk_x11_monitor_get_workarea (GdkMonitor   *monitor,
                              GdkRectangle *dest)
{
  GdkScreen *screen = gdk_display_get_default_screen (monitor->display);
  GdkRectangle workarea;

  gdk_monitor_get_geometry (monitor, dest);

  if (_gdk_x11_screen_get_monitor_work_area (screen, monitor, &workarea))
    {
      if (!gdk_monitor_has_fullscreen_window (monitor))
        *dest = workarea;
    }
  else
    {
      /* The EWMH constrains workarea to be a rectangle, so it
       * can't adequately deal with L-shaped monitor arrangements.
       * As a workaround, we ignore the workarea for anything
       * but the primary monitor. Since that is where the 'desktop
       * chrome' usually lives, this works ok in practice.
       */
      if (gdk_monitor_is_primary (monitor) &&
          !gdk_monitor_has_fullscreen_window (monitor))
        {
          gdk_x11_screen_get_work_area (screen, &workarea);
          if (gdk_rectangle_intersect (dest, &workarea, &workarea))
            *dest = workarea;
        }
    }
}

static void
gdk_x11_monitor_init (GdkX11Monitor *monitor)
{
}

static void
gdk_x11_monitor_finalize (GObject *gobject)
{
  GdkX11Monitor *self = GDK_X11_MONITOR (gobject);

  g_free (self->name);

  G_OBJECT_CLASS (gdk_x11_monitor_parent_class)->finalize (gobject);
}


static void
gdk_x11_monitor_class_init (GdkX11MonitorClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_x11_monitor_finalize;
  GDK_MONITOR_CLASS (class)->get_workarea = gdk_x11_monitor_get_workarea;
}

XID
gdk_x11_monitor_get_output (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_X11_MONITOR (monitor), 0);

  return GDK_X11_MONITOR (monitor)->output;
}

