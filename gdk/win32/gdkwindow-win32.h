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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_WINDOW_WIN32_H__
#define __GDK_WINDOW_WIN32_H__

#include <gdk/win32/gdkdrawable-win32.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkWin32PositionInfo    GdkWin32PositionInfo;

struct _GdkWin32PositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to Win32 coordinates */
  gint y_offset;		/* within window to get GDK coodinates */
  gboolean big : 1;
  gboolean mapped : 1;
  gboolean no_bg : 1;	        /* Set when the window background
				 * is temporarily unset during resizing
				 * and scaling
				 */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};


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
  GdkDrawableImplWin32 parent_instance;

  gint width;
  gint height;
  
  GdkWin32PositionInfo position_info;

  HCURSOR hcursor;

  /* Window size hints */
  gint hint_flags;
  gint hint_x, hint_y;
  gint hint_min_width, hint_min_height;
  gint hint_max_width, hint_max_height;

  gboolean extension_events_selected;
};
 
struct _GdkWindowImplWin32Class 
{
  GdkDrawableImplWin32Class parent_class;

};

GType _gdk_window_impl_win32_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_WINDOW_WIN32_H__ */
