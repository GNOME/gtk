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

#ifndef __GDK_WINDOW_WAYLAND_H__
#define __GDK_WINDOW_WAYLAND_H__

#include <gdk/wayland/gdkprivate-wayland.h>
#include <gdk/gdkwindowimpl.h>

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

G_BEGIN_DECLS

typedef struct _GdkToplevelWayland GdkToplevelWayland;
typedef struct _GdkXPositionInfo GdkXPositionInfo;

struct _GdkToplevelWayland
{

  /* Set if the window, or any descendent of it, is the server's focus window
   */
  guint has_focus_window : 1;

  /* Set if window->has_focus_window and the focus isn't grabbed elsewhere.
   */
  guint has_focus : 1;

  /* Set if the pointer is inside this window. (This is needed for
   * for focus tracking)
   */
  guint has_pointer : 1;
  
  /* Set if the window is a descendent of the focus window and the pointer is
   * inside it. (This is the case where the window will receive keystroke
   * events even window->has_focus_window is FALSE)
   */
  guint has_pointer_focus : 1;

  /* Set if we are requesting these hints */
  guint skip_taskbar_hint : 1;
  guint skip_pager_hint : 1;
  guint urgency_hint : 1;

  guint on_all_desktops : 1;   /* _NET_WM_STICKY == 0xFFFFFFFF */

  guint have_sticky : 1;	/* _NET_WM_STATE_STICKY */
  guint have_maxvert : 1;       /* _NET_WM_STATE_MAXIMIZED_VERT */
  guint have_maxhorz : 1;       /* _NET_WM_STATE_MAXIMIZED_HORZ */
  guint have_fullscreen : 1;    /* _NET_WM_STATE_FULLSCREEN */

  gulong map_serial;	/* Serial of last transition from unmapped */

  cairo_surface_t *icon_pixmap;
  cairo_surface_t *icon_mask;

  /* Time of most recent user interaction. */
  gulong user_time;
};

GType               _gdk_window_impl_wayland_get_type  (void);

GdkToplevelWayland *_gdk_wayland_window_get_toplevel  (GdkWindow   *window);

GdkCursor          *_gdk_wayland_window_get_cursor    (GdkWindow   *window);
void                _gdk_wayland_window_get_offsets   (GdkWindow   *window,
						       gint        *x_offset,
						       gint        *y_offset);

void                _gdk_wayland_window_update_size   (GdkWindow   *window);

G_END_DECLS

#endif /* __GDK_WINDOW_WAYLAND_H__ */
