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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GDK_WINDOW_X11_H__
#define __GDK_WINDOW_X11_H__

#include <gdk/x11/gdkdrawable-x11.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkXPositionInfo    GdkXPositionInfo;

struct _GdkXPositionInfo
{
  gint x;
  gint y;
  gint width;
  gint height;
  gint x_offset;		/* Offsets to add to X coordinates within window */
  gint y_offset;		/*   to get GDK coodinates within window */
  guint big : 1;
  guint mapped : 1;
  guint no_bg : 1;	        /* Set when the window background is temporarily
				 * unset during resizing and scaling */
  GdkRectangle clip_rect;	/* visible rectangle of window */
};


/* Window implementation for X11
 */

typedef struct _GdkWindowImplX11 GdkWindowImplX11;
typedef struct _GdkWindowImplX11Class GdkWindowImplX11Class;

#define GDK_TYPE_WINDOW_IMPL_X11              (gdk_window_impl_x11_get_type ())
#define GDK_WINDOW_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11))
#define GDK_WINDOW_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11Class))
#define GDK_IS_WINDOW_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_X11))
#define GDK_IS_WINDOW_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_X11))
#define GDK_WINDOW_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11Class))

struct _GdkWindowImplX11
{
  GdkDrawableImplX11 parent_instance;

  gint width;
  gint height;
  
  GdkXPositionInfo position_info;

  /* Set if the window, or any descendent of it, has the focus
   */
  guint has_focus : 1;

  /* Set if !window_has_focus, but events are being sent to the
   * window because the pointer is in it. (Typically, no window
   * manager is running.
   */
  guint has_pointer_focus : 1;

  /* We use an extra X window for toplevel windows that we XSetInputFocus()
   * to in order to avoid getting keyboard events redirected to subwindows
   * that might not even be part of this app
   */
  Window focus_window;
};
 
struct _GdkWindowImplX11Class 
{
  GdkDrawableImplX11Class parent_class;

};

GType gdk_window_impl_x11_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_WINDOW_X11_H__ */
