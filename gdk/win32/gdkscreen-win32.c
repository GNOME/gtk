/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002 Hans Breuer
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define _WIN32_WINNT 0x0600

#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkwin32screen.h"
#include "gdkdisplayprivate.h"
#include "gdkdisplay-win32.h"
#include "gdkmonitor-win32.h"

#include <dwmapi.h>

struct _GdkWin32Screen
{
  GObject parent_instance;

  int width, height;
  int surface_scale;
};

struct _GdkWin32ScreenClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Screen, gdk_win32_screen, G_TYPE_OBJECT)

static void
init_root_window_size (GdkWin32Screen *screen)
{
  GdkRectangle result;
  int i;
  GdkDisplay *display = _gdk_display;
  int monitor_count;
  GdkMonitor *monitor;

  monitor_count = gdk_display_get_n_monitors (display);
  monitor = gdk_display_get_monitor (display, 0);
  gdk_monitor_get_geometry (monitor, &result);

  for (i = 1; i < monitor_count; i++)
  {
    GdkRectangle rect;

    monitor = gdk_display_get_monitor (display, i);
    gdk_monitor_get_geometry (monitor, &rect);
    gdk_rectangle_union (&result, &rect, &result);
  }

  screen->width = result.width;
  screen->height = result.height;
}

static void
init_root_window (GdkWin32Screen *screen_win32)
{
  GdkWin32Display *win32_display;

  init_root_window_size (screen_win32);

  win32_display = GDK_WIN32_DISPLAY (_gdk_display);

  if (win32_display->dpi_aware_type != PROCESS_DPI_UNAWARE)
    screen_win32->surface_scale = _gdk_win32_display_get_monitor_scale_factor (win32_display,
                                                                              NULL,
                                                                              NULL,
                                                                              NULL);
  else
    screen_win32->surface_scale = 1;
}

static void
gdk_win32_screen_init (GdkWin32Screen *win32_screen)
{
  _gdk_win32_display_init_monitors (GDK_WIN32_DISPLAY (_gdk_display));
  init_root_window (win32_screen);
}

void
_gdk_win32_screen_on_displaychange_event (GdkWin32Screen *screen)
{
  _gdk_win32_display_init_monitors (GDK_WIN32_DISPLAY (_gdk_display));
  init_root_window_size (screen);
}

static void
gdk_win32_screen_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_win32_screen_parent_class)->finalize (object);
}

static void
gdk_win32_screen_class_init (GdkWin32ScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_win32_screen_finalize;
}
