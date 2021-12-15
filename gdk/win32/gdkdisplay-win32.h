/*
 * gdkdisplay-win32.h
 *
 * Copyright 2014 Chun-wei Fan <fanc999@yahoo.com.tw>
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

#include "gdkdisplayprivate.h"
#include "gdkglcontext-win32.h"

#ifndef __GDK_DISPLAY__WIN32_H__
#define __GDK_DISPLAY__WIN32_H__

/* Define values used to set DPI-awareness */
typedef enum _GdkWin32ProcessDpiAwareness {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2
} GdkWin32ProcessDpiAwareness;

/* Define values for GL type used */
typedef enum _GdkWin32GLContextType
{
  GDK_WIN32_GL_PENDING,
  GDK_WIN32_GL_NONE,
  GDK_WIN32_GL_WGL,
  GDK_WIN32_GL_EGL
} GdkWin32GLContextType;

/* APIs from shcore.dll */
typedef HRESULT (WINAPI *funcSetProcessDpiAwareness) (GdkWin32ProcessDpiAwareness value);
typedef HRESULT (WINAPI *funcGetProcessDpiAwareness) (HANDLE                       handle,
                                                      GdkWin32ProcessDpiAwareness *awareness);
typedef HRESULT (WINAPI *funcGetDpiForMonitor)       (HMONITOR                monitor,
                                                      GdkWin32MonitorDpiType  dpi_type,
                                                      UINT                   *dpi_x,
                                                      UINT                   *dpi_y);

typedef struct _GdkWin32ShcoreFuncs
{
  HMODULE hshcore;
  funcSetProcessDpiAwareness setDpiAwareFunc;
  funcGetProcessDpiAwareness getDpiAwareFunc;
  funcGetDpiForMonitor getDpiForMonitorFunc;
} GdkWin32ShcoreFuncs;

/* DPI awareness APIs from user32.dll */
typedef BOOL (WINAPI *funcSetProcessDPIAware) (void);
typedef BOOL (WINAPI *funcIsProcessDPIAware)  (void);

typedef struct _GdkWin32User32DPIFuncs
{
  funcSetProcessDPIAware setDpiAwareFunc;
  funcIsProcessDPIAware isDpiAwareFunc;
} GdkWin32User32DPIFuncs;

struct _GdkWin32Display
{
  GdkDisplay display;

  GdkScreen *screen;

  Win32CursorTheme *cursor_theme;
  gchar *cursor_theme_name;
  int cursor_theme_size;
  GHashTable *cursor_cache;

  HWND hwnd;
  HWND clipboard_hwnd;

  /* OpenGL Items */
  GdkWin32GLContextType gl_type;
  guint gl_version;

  /* WGL Items */
  guint hasWglARBCreateContext : 1;
  guint hasWglEXTSwapControl : 1;
  guint hasWglOMLSyncControl : 1;
  guint hasWglARBPixelFormat : 1;
  guint hasWglARBmultisample : 1;

  /* compensate around Intel OpenGL driver issues on blitting, see issue #3487 */
  guint needIntelGLWorkaround : 1; 

  /* EGL (Angle) Items */
  HDC hdc_egl_temp;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLDisplay egl_disp;
  EGLConfig egl_config;
  guint hasEglKHRCreateContext : 1;
  guint hasEglSurfacelessContext : 1;
  EGLint egl_min_swap_interval;
#endif

  GPtrArray *monitors;

  /* HiDPI Items */
  guint have_at_least_win81 : 1;
  GdkWin32ProcessDpiAwareness dpi_aware_type;
  guint has_fixed_scale : 1;
  guint window_scale;

  GdkWin32ShcoreFuncs shcore_funcs;
  GdkWin32User32DPIFuncs user32_dpi_funcs;

  /* Running CPU items */
};

struct _GdkWin32DisplayClass
{
  GdkDisplayClass display_class;
};

gboolean   _gdk_win32_display_init_monitors    (GdkWin32Display *display);

GPtrArray *_gdk_win32_display_get_monitor_list (GdkWin32Display *display);

guint      _gdk_win32_display_get_monitor_scale_factor (GdkWin32Display *win32_display,
                                                        HMONITOR         hmonitor,
                                                        HWND             hwnd,
                                                        gint             *dpi);
#endif /* __GDK_DISPLAY__WIN32_H__ */
