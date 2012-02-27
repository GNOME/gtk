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

struct _GdkWin32Screen
{
  GdkScreen parent_instance;
};

struct _GdkWin32ScreenClass
{
  GdkScreenClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Screen, gdk_win32_screen, GDK_TYPE_SCREEN)

static void
gdk_win32_screen_init (GdkWin32Screen *display)
{
}

static GdkDisplay *
gdk_win32_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

static gint
gdk_win32_screen_get_width (GdkScreen *screen)
{
  return GDK_WINDOW (_gdk_root)->width;
}

static gint
gdk_win32_screen_get_height (GdkScreen *screen)
{
  return GDK_WINDOW (_gdk_root)->height;
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
  return _gdk_root;
}

static gint
gdk_win32_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);

  return _gdk_num_monitors;
}

static gint
gdk_win32_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);

  return 0;
}

static gint
gdk_win32_screen_get_monitor_width_mm (GdkScreen *screen,
                                       gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return _gdk_monitors[num_monitor].width_mm;
}

static gint
gdk_win32_screen_get_monitor_height_mm (GdkScreen *screen,
                                        gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return _gdk_monitors[num_monitor].height_mm;
}

static gchar *
gdk_win32_screen_get_monitor_plug_name (GdkScreen *screen,
                                        gint       num_monitor)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);
  g_return_val_if_fail (num_monitor < _gdk_num_monitors, 0);

  return g_strdup (_gdk_monitors[num_monitor].name);
}

static void
gdk_win32_screen_get_monitor_geometry (GdkScreen    *screen,
                                       gint          num_monitor,
                                       GdkRectangle *dest)
{
  g_return_if_fail (screen == _gdk_screen);
  g_return_if_fail (num_monitor < _gdk_num_monitors);

  *dest = _gdk_monitors[num_monitor].rect;
}

static GdkVisual *
gdk_win32_screen_get_rgba_visual (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, NULL);

  return NULL;
}

static gint
gdk_win32_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (screen == _gdk_screen, 0);

  return 0;
}

gchar *
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 int          screen_number)
{
  if (screen_number != 0)
    return NULL;

  return g_strdup (display_name);
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
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return FALSE;
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
  screen_class->get_rgba_visual = gdk_win32_screen_get_rgba_visual;
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
