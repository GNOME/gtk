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
#include "gdk.h"
#include "gdkprivate-win32.h"
#include "gdkscreenprivate.h"
#include "gdkwin32screen.h"
#include "gdkdisplayprivate.h"
#include "gdkvisualprivate.h"

#include <dwmapi.h>

struct _GdkWin32Screen
{
  GdkScreen parent_instance;

  GdkWindow *root_window;

  gint always_composited : 1;
};

struct _GdkWin32ScreenClass
{
  GdkScreenClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Screen, gdk_win32_screen, GDK_TYPE_SCREEN)

static void
gdk_win32_screen_init (GdkWin32Screen *win32_screen)
{
  GdkScreen *screen = GDK_SCREEN (win32_screen);
  HDC screen_dc;
  int logpixelsx = -1;
  const gchar *font_resolution;

  screen_dc = GetDC (NULL);

  if (screen_dc)
    {
      logpixelsx = GetDeviceCaps(screen_dc, LOGPIXELSX);
      ReleaseDC (NULL, screen_dc);
    }

  font_resolution = g_getenv ("GDK_WIN32_FONT_RESOLUTION");
  if (font_resolution)
    {
      int env_logpixelsx = atol (font_resolution);
      if (env_logpixelsx > 0)
        logpixelsx = env_logpixelsx;
    }

  if (logpixelsx > 0)
    _gdk_screen_set_resolution (screen, logpixelsx);

  /* On Windows 8 and later, DWM (composition) is always enabled */
  win32_screen->always_composited = g_win32_check_windows_version (6, 2, 0, G_WIN32_OS_ANY);
}

void
_gdk_screen_init_root_window_size (GdkWin32Screen *screen)
{
  GdkRectangle rect;
  int i;

  rect = _gdk_monitors[0].rect;
  for (i = 1; i < _gdk_num_monitors; i++)
    gdk_rectangle_union (&rect, &_gdk_monitors[i].rect, &rect);

  screen->root_window->width = rect.width;
  screen->root_window->height = rect.height;
}

void
_gdk_screen_init_root_window (GdkWin32Screen *screen_win32)
{
  GdkScreen *screen;
  GdkWindow *window;
  GdkWindowImplWin32 *impl_win32;

  screen = GDK_SCREEN (screen_win32);

  g_assert (screen_win32->root_window == NULL);

  window = _gdk_display_create_window (_gdk_display);
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WIN32, NULL);
  impl_win32 = GDK_WINDOW_IMPL_WIN32 (window->impl);
  impl_win32->wrapper = window;

  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = window->visual->depth;

  screen_win32->root_window = window;

  _gdk_screen_init_root_window_size (screen_win32);

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  /* width and height already initialised in _gdk_screen_init_root_window_size() */
  window->viewable = TRUE;

  gdk_win32_handle_table_insert ((HANDLE *) &impl_win32->handle, window);

  GDK_NOTE (MISC, g_print ("screen->root_window=%p\n", window));
}

static GdkDisplay *
gdk_win32_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

static gint
gdk_win32_screen_get_width (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->root_window->width;
}

static gint
gdk_win32_screen_get_height (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->root_window->height;
}

static gint
gdk_win32_screen_get_width_mm (GdkScreen *screen)
{
  return (double) gdk_screen_get_width (screen) / GetDeviceCaps (_gdk_display_hdc, LOGPIXELSX) * 25.4;
}

static gint
gdk_win32_screen_get_height_mm (GdkScreen *screen)
{
  return (double) gdk_screen_get_height (screen) / GetDeviceCaps (_gdk_display_hdc, LOGPIXELSY) * 25.4;
}

static GdkWindow *
gdk_win32_screen_get_root_window (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->root_window;
}

static gint
gdk_win32_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);

  return _gdk_num_monitors;
}

static gint
gdk_win32_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);

  return 0;
}

static gint
gdk_win32_screen_get_monitor_width_mm (GdkScreen *screen,
                                       gint       num_monitor)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return _gdk_monitors[num_monitor].width_mm;
}

static gint
gdk_win32_screen_get_monitor_height_mm (GdkScreen *screen,
                                        gint       num_monitor)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return _gdk_monitors[num_monitor].height_mm;
}

static gchar *
gdk_win32_screen_get_monitor_plug_name (GdkScreen *screen,
                                        gint       num_monitor)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return g_strdup (_gdk_monitors[num_monitor].name);
}

static void
gdk_win32_screen_get_monitor_geometry (GdkScreen    *screen,
                                       gint          num_monitor,
                                       GdkRectangle *dest)
{
  g_return_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()));
  g_return_if_fail (num_monitor < _gdk_num_monitors);

  *dest = _gdk_monitors[num_monitor].rect;
}

static gint
gdk_win32_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);

  return 0;
}

static gchar *
gdk_win32_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (_gdk_display));
}

static GdkWindow *
gdk_win32_screen_get_active_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

static GList *
gdk_win32_screen_get_window_stack (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

static gboolean
gdk_win32_screen_is_composited (GdkScreen *screen)
{
  if (GDK_WIN32_SCREEN (screen)->always_composited)
    return TRUE;
  else
    {
      gboolean is_composited;

      if (DwmIsCompositionEnabled (&is_composited) != S_OK)
        return FALSE;
      return is_composited;
    }
}

static void
gdk_win32_screen_finalize (GObject *object)
{
}

static void
gdk_win32_screen_dispose (GObject *object)
{
}

static void
gdk_win32_screen_class_init (GdkWin32ScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_win32_screen_dispose;
  object_class->finalize = gdk_win32_screen_finalize;

  screen_class->get_display = gdk_win32_screen_get_display;
  screen_class->get_width = gdk_win32_screen_get_width;
  screen_class->get_height = gdk_win32_screen_get_height;
  screen_class->get_width_mm = gdk_win32_screen_get_width_mm;
  screen_class->get_height_mm = gdk_win32_screen_get_height_mm;
  screen_class->get_number = gdk_win32_screen_get_number;
  screen_class->get_root_window = gdk_win32_screen_get_root_window;
  screen_class->get_n_monitors = gdk_win32_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_win32_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_win32_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_win32_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_win32_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_win32_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_win32_screen_get_monitor_geometry;
  screen_class->get_system_visual = _gdk_win32_screen_get_system_visual;
  screen_class->get_rgba_visual = _gdk_win32_screen_get_rgba_visual;
  screen_class->is_composited = gdk_win32_screen_is_composited;
  screen_class->make_display_name = gdk_win32_screen_make_display_name;
  screen_class->get_active_window = gdk_win32_screen_get_active_window;
  screen_class->get_window_stack = gdk_win32_screen_get_window_stack;
  screen_class->get_setting = _gdk_win32_screen_get_setting;
  screen_class->visual_get_best_depth = _gdk_win32_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = _gdk_win32_screen_visual_get_best_type;
  screen_class->visual_get_best = _gdk_win32_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = _gdk_win32_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = _gdk_win32_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = _gdk_win32_screen_visual_get_best_with_both;
  screen_class->query_depths = _gdk_win32_screen_query_depths;
  screen_class->query_visual_types = _gdk_win32_screen_query_visual_types;
  screen_class->list_visuals = _gdk_win32_screen_list_visuals;
}
