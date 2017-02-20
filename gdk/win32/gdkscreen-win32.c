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

typedef struct
{
  gchar *name;
  gint width_mm, height_mm;
  GdkRectangle rect;
  GdkRectangle work_rect;
} GdkWin32Monitor;

struct _GdkWin32Screen
{
  GdkScreen parent_instance;

  gint num_monitors;
  GdkWin32Monitor **monitors;

  GdkVisual *system_visual;
  GdkVisual *rgba_visual;
  gint available_visual_depths[1];
  GdkVisualType available_visual_types[1];

  GdkWindow *root_window;

  gint always_composited : 1;
};

struct _GdkWin32ScreenClass
{
  GdkScreenClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Screen, gdk_win32_screen, GDK_TYPE_SCREEN)

static void
free_monitor (gpointer user_data)
{
  GdkWin32Monitor *monitor = (GdkWin32Monitor *)user_data;

  g_free (monitor->name);
  g_free (monitor);
}

static gint
get_color_precision (gulong mask)
{
  gint p = 0;

  while (mask & 0x1)
    {
      p++;
      mask >>= 1;
    }

  return p;
}

static GdkVisual *
init_visual (GdkScreen *screen,
             gboolean is_rgba)
{
  GdkVisual *visual;
  struct
  {
    BITMAPINFOHEADER bi;
    union
    {
      RGBQUAD colors[256];
      DWORD fields[256];
    } u;
  } bmi;
  HBITMAP hbm;

  const gint rastercaps = GetDeviceCaps (_gdk_display_hdc, RASTERCAPS);
  const int numcolors = GetDeviceCaps (_gdk_display_hdc, NUMCOLORS);
  gint bitspixel = GetDeviceCaps (_gdk_display_hdc, BITSPIXEL);
  gint map_entries = 0;

  visual = g_object_new (GDK_TYPE_VISUAL, NULL);
  visual->screen = screen;

  if (rastercaps & RC_PALETTE)
    {
      const int sizepalette = GetDeviceCaps (_gdk_display_hdc, SIZEPALETTE);
      gchar *max_colors = getenv ("GDK_WIN32_MAX_COLORS");
      visual->type = GDK_VISUAL_PSEUDO_COLOR;

      g_assert (sizepalette == 256);

      if (max_colors != NULL)
        _gdk_max_colors = atoi (max_colors);

      map_entries = _gdk_max_colors;

      if (map_entries >= 16 && map_entries < sizepalette)
        {
          if (map_entries < 32)
            {
              map_entries = 16;
              visual->type = GDK_VISUAL_STATIC_COLOR;
              bitspixel = 4;
            }
          else if (map_entries < 64)
            {
              map_entries = 32;
              bitspixel = 5;
            }
          else if (map_entries < 128)
            {
              map_entries = 64;
              bitspixel = 6;
            }
          else if (map_entries < 256)
            {
              map_entries = 128;
              bitspixel = 7;
            }
          else
            g_assert_not_reached ();
        }
      else
        map_entries = sizepalette;
    }
  else if (bitspixel == 1 && numcolors == 16)
    {
      bitspixel = 4;
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 1)
    {
      visual->type = GDK_VISUAL_STATIC_GRAY;
      map_entries = 2;
    }
  else if (bitspixel == 4)
    {
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 16;
    }
  else if (bitspixel == 8)
    {
      visual->type = GDK_VISUAL_STATIC_COLOR;
      map_entries = 256;
    }
  else if (bitspixel == 16)
    {
      visual->type = GDK_VISUAL_TRUE_COLOR;
#if 1
      /* This code by Mike Enright,
       * see http://www.users.cts.com/sd/m/menright/display.html
       */
      memset (&bmi, 0, sizeof (bmi));
      bmi.bi.biSize = sizeof (bmi.bi);

      hbm = CreateCompatibleBitmap (_gdk_display_hdc, 1, 1);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
                 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      GetDIBits (_gdk_display_hdc, hbm, 0, 1, NULL,
                 (BITMAPINFO *) &bmi, DIB_RGB_COLORS);
      DeleteObject (hbm);

      if (bmi.bi.biCompression != BI_BITFIELDS)
        {
          /* Either BI_RGB or BI_RLE_something
           * .... or perhaps (!!) something else.
           * Theoretically biCompression might be
           * mmioFourCC('c','v','i','d') but I doubt it.
           */
          if (bmi.bi.biCompression == BI_RGB)
            {
              /* It's 555 */
              bitspixel = 15;
              visual->red_mask   = 0x00007C00;
              visual->green_mask = 0x000003E0;
              visual->blue_mask  = 0x0000001F;
            }
          else
            {
              g_assert_not_reached ();
            }
        }
      else
        {
          DWORD allmasks =
            bmi.u.fields[0] | bmi.u.fields[1] | bmi.u.fields[2];
          int k = 0;
          while (allmasks)
            {
              if (allmasks&1)
                k++;
              allmasks/=2;
            }
          bitspixel = k;
          visual->red_mask = bmi.u.fields[0];
          visual->green_mask = bmi.u.fields[1];
          visual->blue_mask  = bmi.u.fields[2];
        }
#else
      /* Old, incorrect (but still working) code. */
#if 0
      visual->red_mask   = 0x0000F800;
      visual->green_mask = 0x000007E0;
      visual->blue_mask  = 0x0000001F;
#else
      visual->red_mask   = 0x00007C00;
      visual->green_mask = 0x000003E0;
      visual->blue_mask  = 0x0000001F;
#endif
#endif
    }
  else if (bitspixel == 24 || bitspixel == 32)
    {
      if (!is_rgba)
        bitspixel = 24;
      visual->type = GDK_VISUAL_TRUE_COLOR;
      visual->red_mask   = 0x00FF0000;
      visual->green_mask = 0x0000FF00;
      visual->blue_mask  = 0x000000FF;
    }
  else
    g_error ("_gdk_visual_init: unsupported BITSPIXEL: %d\n", bitspixel);

  visual->depth = bitspixel;
  visual->byte_order = GDK_LSB_FIRST;
  visual->bits_per_rgb = 42; /* Not used? */

  if ((visual->type != GDK_VISUAL_TRUE_COLOR) &&
      (visual->type != GDK_VISUAL_DIRECT_COLOR))
    {
      visual->red_mask = 0;
      visual->green_mask = 0;
      visual->blue_mask = 0;
    }
  else
    map_entries = 1 << (MAX (get_color_precision (visual->red_mask),
                             MAX (get_color_precision (visual->green_mask),
                                  get_color_precision (visual->blue_mask))));

  visual->colormap_size = map_entries;

  return visual;
}

static void
init_root_window_size (GdkWin32Screen *screen)
{
  GdkRectangle rect;
  int i;

  rect = screen->monitors[0]->rect;
  for (i = 1; i < screen->num_monitors; i++)
    gdk_rectangle_union (&rect, &screen->monitors[i]->rect, &rect);

  screen->root_window->width = rect.width;
  screen->root_window->height = rect.height;
}

static void
init_root_window (GdkWin32Screen *screen_win32)
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

  init_root_window_size (screen_win32);

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  /* width and height already initialised in init_root_window_size() */
  window->viewable = TRUE;

  gdk_win32_handle_table_insert ((HANDLE *) &impl_win32->handle, window);

  GDK_NOTE (MISC, g_print ("screen->root_window=%p\n", window));
}

typedef struct {
    GdkWin32Screen *screen;
    GPtrArray *monitors;
} EnumMonitorData;

static BOOL CALLBACK
enum_monitor (HMONITOR hmonitor,
              HDC      hdc,
              LPRECT   rect,
              LPARAM   param)
{
  /* The struct MONITORINFOEX definition is for some reason different
   * in the winuser.h bundled with mingw64 from that in MSDN and the
   * official 32-bit mingw (the MONITORINFO part is in a separate "mi"
   * member). So to keep this easily compileable with either, repeat
   * the MSDN definition it here.
   */
  typedef struct tagMONITORINFOEXA2 {
    DWORD cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    DWORD dwFlags;
    CHAR szDevice[CCHDEVICENAME];
  } MONITORINFOEXA2;

  EnumMonitorData *data = (EnumMonitorData *) param;
  GdkWin32Monitor *monitor;
  MONITORINFOEXA2 monitor_info;
  HDC hDC;

  monitor_info.cbSize = sizeof (MONITORINFOEXA2);
  GetMonitorInfoA (hmonitor, (MONITORINFO *) &monitor_info);

#ifndef MONITORINFOF_PRIMARY
#define MONITORINFOF_PRIMARY 1
#endif

  monitor = g_new0 (GdkWin32Monitor, 1);
  monitor->name = g_strdup (monitor_info.szDevice);
  hDC = CreateDCA ("DISPLAY", monitor_info.szDevice, NULL, NULL);
  monitor->width_mm = GetDeviceCaps (hDC, HORZSIZE);
  monitor->height_mm = GetDeviceCaps (hDC, VERTSIZE);
  DeleteDC (hDC);
  monitor->rect.x = monitor_info.rcMonitor.left;
  monitor->rect.y = monitor_info.rcMonitor.top;
  monitor->rect.width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  monitor->rect.height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
  monitor->work_rect.x = monitor_info.rcWork.left;
  monitor->work_rect.y = monitor_info.rcWork.top;
  monitor->work_rect.width = monitor_info.rcWork.right - monitor_info.rcWork.left;
  monitor->work_rect.height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;

  if (monitor_info.dwFlags & MONITORINFOF_PRIMARY)
    /* Put primary monitor at index 0, just in case somebody needs
     * to know which one is the primary.
     */
    g_ptr_array_insert (data->monitors, 0, monitor);
  else
    g_ptr_array_add (data->monitors, monitor);

  return TRUE;
}

static void
init_monitors (GdkWin32Screen *screen)
{
  EnumMonitorData data;
  gint i;

  data.screen = screen;
  data.monitors = g_ptr_array_new ();

  for (i = 0; i < screen->num_monitors; i++)
    free_monitor (screen->monitors[i]);

  EnumDisplayMonitors (NULL, NULL, enum_monitor, (LPARAM) &data);

  screen->num_monitors = data.monitors->len;
  screen->monitors = (GdkWin32Monitor **)g_ptr_array_free (data.monitors, FALSE);

  _gdk_offset_x = G_MININT;
  _gdk_offset_y = G_MININT;

  /* Calculate offset */
  for (i = 0; i < screen->num_monitors; i++)
    {
      GdkRectangle *rect = &screen->monitors[i]->rect;
      _gdk_offset_x = MAX (_gdk_offset_x, -rect->x);
      _gdk_offset_y = MAX (_gdk_offset_y, -rect->y);
    }
  GDK_NOTE (MISC, g_print ("Multi-monitor offset: (%d,%d)\n",
                           _gdk_offset_x, _gdk_offset_y));

  /* Translate monitor coords into GDK coordinate space */
  for (i = 0; i < screen->num_monitors; i++)
    {
      GdkRectangle *rect;
      rect = &screen->monitors[i]->rect;
      rect->x += _gdk_offset_x;
      rect->y += _gdk_offset_y;
      rect = &screen->monitors[i]->work_rect;
      rect->x += _gdk_offset_x;
      rect->y += _gdk_offset_y;
      GDK_NOTE (MISC, g_print ("Monitor %d: %dx%d@%+d%+d\n", i,
                               rect->width, rect->height, rect->x, rect->y));
    }
}

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

  win32_screen->system_visual = init_visual (screen, FALSE);
  win32_screen->rgba_visual = init_visual (screen, TRUE);

  win32_screen->available_visual_depths[0] = win32_screen->rgba_visual->depth;
  win32_screen->available_visual_types[0] = win32_screen->rgba_visual->type;

  init_monitors (win32_screen);
  init_root_window (win32_screen);

  /* On Windows 8 and later, DWM (composition) is always enabled */
  win32_screen->always_composited = g_win32_check_windows_version (6, 2, 0, G_WIN32_OS_ANY);
}

void
_gdk_win32_screen_on_displaychange_event (GdkWin32Screen *screen)
{
  init_monitors (screen);
  init_root_window_size (screen);
  g_signal_emit_by_name (screen, "size-changed");
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

  return GDK_WIN32_SCREEN (screen)->num_monitors;
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
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);
  g_return_val_if_fail (num_monitor < win32_screen->num_monitors, 0);

  return win32_screen->monitors[num_monitor]->width_mm;
}

static gint
gdk_win32_screen_get_monitor_height_mm (GdkScreen *screen,
                                        gint       num_monitor)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), 0);
  g_return_val_if_fail (num_monitor < win32_screen->num_monitors, 0);

  return win32_screen->monitors[num_monitor]->height_mm;
}

static gchar *
gdk_win32_screen_get_monitor_plug_name (GdkScreen *screen,
                                        gint       num_monitor)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  g_return_val_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()), NULL);
  g_return_val_if_fail (num_monitor < win32_screen->num_monitors, NULL);

  return g_strdup (win32_screen->monitors[num_monitor]->name);
}

static void
gdk_win32_screen_get_monitor_geometry (GdkScreen    *screen,
                                       gint          num_monitor,
                                       GdkRectangle *dest)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  g_return_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()));
  g_return_if_fail (num_monitor < win32_screen->num_monitors);

  *dest = win32_screen->monitors[num_monitor]->rect;
}

static void
gdk_win32_screen_get_monitor_workarea (GdkScreen    *screen,
                                       gint          num_monitor,
                                       GdkRectangle *dest)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  g_return_if_fail (screen == gdk_display_get_default_screen (gdk_display_get_default ()));
  g_return_if_fail (num_monitor < win32_screen->num_monitors);

  *dest = win32_screen->monitors[num_monitor]->work_rect;
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

static gint
gdk_win32_screen_visual_get_best_depth (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->available_visual_depths[0];
}

static GdkVisualType
gdk_win32_screen_visual_get_best_type (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->available_visual_types[0];
}

static GdkVisual *
gdk_win32_screen_get_system_visual (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->system_visual;
}

static GdkVisual *
gdk_win32_screen_get_rgba_visual (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->rgba_visual;
}

static GdkVisual*
gdk_win32_screen_visual_get_best (GdkScreen *screen)
{
  return GDK_WIN32_SCREEN (screen)->rgba_visual;
}

static GdkVisual *
gdk_win32_screen_visual_get_best_with_depth (GdkScreen *screen,
                                             gint       depth)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  if (depth == win32_screen->rgba_visual->depth)
    return win32_screen->rgba_visual;
  else if (depth == win32_screen->system_visual->depth)
    return win32_screen->system_visual;

  return NULL;
}

static GdkVisual *
gdk_win32_screen_visual_get_best_with_type (GdkScreen     *screen,
                                            GdkVisualType  visual_type)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  if (visual_type == win32_screen->rgba_visual->type)
    return win32_screen->rgba_visual;
  else if (visual_type == win32_screen->system_visual->type)
    return win32_screen->system_visual;

  return NULL;
}

static GdkVisual *
gdk_win32_screen_visual_get_best_with_both (GdkScreen     *screen,
                                            gint           depth,
                                            GdkVisualType  visual_type)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);

  if ((depth == win32_screen->rgba_visual->depth) && (visual_type == win32_screen->rgba_visual->type))
    return win32_screen->rgba_visual;
  else if ((depth == win32_screen->system_visual->depth) && (visual_type == win32_screen->system_visual->type))
    return win32_screen->system_visual;

  return NULL;
}

static void
gdk_win32_screen_query_depths (GdkScreen  *screen,
                               gint      **depths,
                               gint       *count)
{
  *count = 1;
  *depths = GDK_WIN32_SCREEN (screen)->available_visual_depths;
}

static void
gdk_win32_screen_query_visual_types (GdkScreen      *screen,
                                     GdkVisualType **visual_types,
                                     gint           *count)
{
  *count = 1;
  *visual_types = GDK_WIN32_SCREEN (screen)->available_visual_types;
}

static GList *
gdk_win32_screen_list_visuals (GdkScreen *screen)
{
  GdkWin32Screen *win32_screen = GDK_WIN32_SCREEN (screen);
  GList *result = NULL;

  result = g_list_append (result, win32_screen->rgba_visual);
  result = g_list_append (result, win32_screen->system_visual);

  return result;
}

static void
gdk_win32_screen_finalize (GObject *object)
{
  GdkWin32Screen *screen = GDK_WIN32_SCREEN (object);
  gint i;

  for (i = 0; i < screen->num_monitors; i++)
    free_monitor (screen->monitors[i]);

  G_OBJECT_CLASS (gdk_win32_screen_parent_class)->finalize (object);
}

static void
gdk_win32_screen_class_init (GdkWin32ScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

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
  screen_class->get_monitor_workarea = gdk_win32_screen_get_monitor_workarea;
  screen_class->is_composited = gdk_win32_screen_is_composited;
  screen_class->make_display_name = gdk_win32_screen_make_display_name;
  screen_class->get_active_window = gdk_win32_screen_get_active_window;
  screen_class->get_window_stack = gdk_win32_screen_get_window_stack;
  screen_class->get_setting = _gdk_win32_screen_get_setting;
  screen_class->get_system_visual = gdk_win32_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_win32_screen_get_rgba_visual;
  screen_class->visual_get_best_depth = gdk_win32_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = gdk_win32_screen_visual_get_best_type;
  screen_class->visual_get_best = gdk_win32_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = gdk_win32_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = gdk_win32_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = gdk_win32_screen_visual_get_best_with_both;
  screen_class->query_depths = gdk_win32_screen_query_depths;
  screen_class->query_visual_types = gdk_win32_screen_query_visual_types;
  screen_class->list_visuals = gdk_win32_screen_list_visuals;
}
