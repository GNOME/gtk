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
#include "gdkscreenprivate.h"
#include "gdkwin32screen.h"
#include "gdkdisplayprivate.h"
#include "gdkdisplay-win32.h"
#include "gdkmonitor-win32.h"

#include <dwmapi.h>

struct _GdkWin32Screen
{
  GdkScreen parent_instance;

  GdkWindow *root_window;
};

struct _GdkWin32ScreenClass
{
  GdkScreenClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Screen, gdk_win32_screen, GDK_TYPE_SCREEN)

static gboolean
init_root_window_size (GdkWin32Screen *screen)
{
  GdkRectangle result;
  int i;
  GdkDisplay *display = _gdk_display;
  int monitor_count;
  GdkMonitor *monitor;
  gboolean changed;
  GdkWindowImplWin32 *root_impl;

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

  changed = screen->root_window->width != result.width ||
            screen->root_window->height != result.height;
  screen->root_window->width = result.width;
  screen->root_window->height = result.height;
  root_impl = GDK_WINDOW_IMPL_WIN32 (screen->root_window->impl);

  root_impl->unscaled_width = result.width * root_impl->window_scale;
  root_impl->unscaled_height = result.height * root_impl->window_scale;

  return changed;
}

static gboolean
init_root_window (GdkWin32Screen *screen_win32)
{
  GdkScreen *screen;
  GdkWindow *window;
  GdkWindowImplWin32 *impl_win32;
  gboolean changed;
  GdkWin32Display *win32_display;

  screen = GDK_SCREEN (screen_win32);

  g_assert (screen_win32->root_window == NULL);

  window = _gdk_display_create_window (_gdk_display);
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WIN32, NULL);
  impl_win32 = GDK_WINDOW_IMPL_WIN32 (window->impl);
  impl_win32->wrapper = window;

  window->impl_window = window;

  window->window_type = GDK_WINDOW_ROOT;

  screen_win32->root_window = window;

  changed = init_root_window_size (screen_win32);

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  /* width and height already initialised in init_root_window_size() */
  window->viewable = TRUE;
  win32_display = GDK_WIN32_DISPLAY (_gdk_display);

  if (win32_display->dpi_aware_type != PROCESS_DPI_UNAWARE)
    impl_win32->window_scale = _gdk_win32_display_get_monitor_scale_factor (win32_display,
                                                                            NULL,
                                                                            impl_win32->handle,
                                                                            NULL);
  else
    impl_win32->window_scale = 1;

  impl_win32->unscaled_width = window->width * impl_win32->window_scale;
  impl_win32->unscaled_height = window->height * impl_win32->window_scale;

  gdk_win32_handle_table_insert ((HANDLE *) &impl_win32->handle, window);

  GDK_NOTE (MISC, g_print ("screen->root_window=%p\n", window));

  return changed;
}

static void
gdk_win32_screen_init (GdkWin32Screen *win32_screen)
{
  GdkScreen *screen = GDK_SCREEN (win32_screen);
  _gdk_win32_screen_set_font_resolution (win32_screen);

  _gdk_win32_display_init_monitors (GDK_WIN32_DISPLAY (_gdk_display));
  init_root_window (win32_screen);
}

void
_gdk_win32_screen_on_displaychange_event (GdkWin32Screen *screen)
{
  gboolean monitors_changed;

  monitors_changed = _gdk_win32_display_init_monitors (GDK_WIN32_DISPLAY (_gdk_display));

  if (init_root_window_size (screen))
    g_signal_emit_by_name (screen, "size-changed");

  if (monitors_changed)
    g_signal_emit_by_name (screen, "monitors-changed");
}

void
_gdk_win32_screen_set_font_resolution (GdkWin32Screen *win32_screen)
{
  GdkScreen *screen = GDK_SCREEN (win32_screen);
  int logpixelsx = -1;
  const gchar *font_resolution;

  font_resolution = g_getenv ("GDK_WIN32_FONT_RESOLUTION");
  if (font_resolution)
    {
      int env_logpixelsx = atol (font_resolution);
      if (env_logpixelsx > 0)
        logpixelsx = env_logpixelsx;
    }
  else
    {
      gint dpi = -1;
      GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (gdk_screen_get_display (screen));
      guint scale = _gdk_win32_display_get_monitor_scale_factor (win32_display, NULL, NULL, &dpi);

      /* If we have a scale that is at least 2, don't scale up the fonts */
      if (scale >= 2)
        logpixelsx = USER_DEFAULT_SCREEN_DPI;
      else
        logpixelsx = dpi;
    }

  if (logpixelsx > 0)
    _gdk_screen_set_resolution (screen, logpixelsx);
}

static GdkDisplay *
gdk_win32_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

static GdkWindow *
gdk_win32_screen_get_root_window (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->root_window;
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
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->finalize = gdk_win32_screen_finalize;

  screen_class->get_display = gdk_win32_screen_get_display;
  screen_class->get_root_window = gdk_win32_screen_get_root_window;
  screen_class->get_setting = _gdk_win32_screen_get_setting;
}
