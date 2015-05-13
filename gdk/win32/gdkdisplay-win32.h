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

#ifndef __GDK_DISPLAY__WIN32_H__
#define __GDK_DISPLAY__WIN32_H__

struct _GdkWin32Display
{
  GdkDisplay display;

  Win32CursorTheme *cursor_theme;
  gchar *cursor_theme_name;
  int cursor_theme_size;
  GHashTable *cursor_cache;

  /* WGL/OpenGL Items */
  guint have_wgl : 1;
  guint gl_version;
  HDC gl_hdc;
  HWND gl_hwnd;

  guint hasWglARBCreateContext : 1;
  guint hasWglEXTSwapControl : 1;
  guint hasWglOMLSyncControl : 1;
};

struct _GdkWin32DisplayClass
{
  GdkDisplayClass display_class;
};

#endif /* __GDK_DISPLAY__WIN32_H__ */
