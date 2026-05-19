/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <gdk/gdk.h>
#include <gdk/win32/gdkwin32display.h>
#include <windows.h>

G_BEGIN_DECLS

typedef struct _GdkWin32HCursor GdkWin32HCursor;
typedef struct _GdkWin32HCursorClass GdkWin32HCursorClass;

#define GDK_TYPE_WIN32_HCURSOR              (gdk_win32_hcursor_get_type())
#define GDK_WIN32_HCURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_HCURSOR, GdkWin32HCursor))
#define GDK_WIN32_HCURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_HCURSOR, GdkWin32HCursorClass))
#define GDK_IS_WIN32_HCURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_HCURSOR))
#define GDK_IS_WIN32_HCURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_HCURSOR))
#define GDK_WIN32_HCURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_HCURSOR, GdkWin32HCursorClass))

GType            gdk_win32_hcursor_get_type            (void);

struct _GdkWin32HCursorFake
{
  GObject          parent_instance;
  HCURSOR          readonly_handle;
};

GdkWin32HCursor *gdk_win32_hcursor_new                 (GdkWin32Display *display,
                                                        HCURSOR          handle,
                                                        gboolean         destroyable);

HCURSOR          gdk_win32_hcursor_get_handle          (GdkWin32HCursor *cursor);
GdkWin32HCursor *gdk_win32_display_get_win32hcursor    (GdkWin32Display *display,
                                                        GdkCursor       *cursor);
void             _gdk_win32_display_hcursor_ref        (GdkWin32Display *display,
                                                        HCURSOR          handle,
                                                        gboolean         destroyable);
void             _gdk_win32_display_hcursor_unref      (GdkWin32Display *display,
                                                        HCURSOR          handle);

G_END_DECLS

