/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_WINDOW_WIN32_H__
#define __GDK_WINDOW_WIN32_H__

#include "gdk/win32/gdkprivate-win32.h"
#include "gdk/gdkwindowimpl.h"
#include "gdk/gdkcursor.h"

#include <windows.h>

G_BEGIN_DECLS

/* Window implementation for Win32
 */

typedef struct _GdkWindowImplWin32 GdkWindowImplWin32;
typedef struct _GdkWindowImplWin32Class GdkWindowImplWin32Class;

#define GDK_TYPE_WINDOW_IMPL_WIN32              (_gdk_window_impl_win32_get_type ())
#define GDK_WINDOW_IMPL_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32))
#define GDK_WINDOW_IMPL_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32Class))
#define GDK_IS_WINDOW_IMPL_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_WIN32))
#define GDK_IS_WINDOW_IMPL_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_WIN32))
#define GDK_WINDOW_IMPL_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32Class))

struct _GdkWindowImplWin32
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;
  HANDLE handle;

  gint8 toplevel_window_type;

  GdkCursor *cursor;
  HICON   hicon_big;
  HICON   hicon_small;

  /* Window size hints */
  gint hint_flags;
  GdkGeometry hints;

  GdkEventMask native_event_mask;

  GdkWindowTypeHint type_hint;

  GdkWindow *transient_owner;
  GSList    *transient_children;
  gint       num_transients;
  gboolean   changing_state;

  gint initial_x;
  gint initial_y;

  guint no_bg : 1;
  guint inhibit_configure : 1;
  guint override_redirect : 1;

  cairo_surface_t *cairo_surface;
  HDC              hdc;
  int              hdc_count;
  HBITMAP          saved_dc_bitmap; /* Original bitmap for dc */
};

struct _GdkWindowImplWin32Class
{
  GdkWindowImplClass parent_class;
};

GType _gdk_window_impl_win32_get_type (void);

void  _gdk_win32_window_tmp_unset_bg  (GdkWindow *window,
				       gboolean   recurse);
void  _gdk_win32_window_tmp_reset_bg  (GdkWindow *window,
				       gboolean   recurse);

void  _gdk_win32_window_tmp_unset_parent_bg (GdkWindow *window);
void  _gdk_win32_window_tmp_reset_parent_bg (GdkWindow *window);

G_END_DECLS

#endif /* __GDK_WINDOW_WIN32_H__ */
