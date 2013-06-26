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

#ifndef __GDK_WINDOW_X11_H__
#define __GDK_WINDOW_X11_H__

#include "gdk/x11/gdkprivate-x11.h"
#include "gdk/gdkwindowimpl.h"

#include <X11/Xlib.h>

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#ifdef HAVE_XSYNC
#include <X11/Xlib.h>
#include <X11/extensions/sync.h>
#endif

G_BEGIN_DECLS

typedef struct _GdkToplevelX11 GdkToplevelX11;
typedef struct _GdkWindowImplX11 GdkWindowImplX11;
typedef struct _GdkWindowImplX11Class GdkWindowImplX11Class;
typedef struct _GdkXPositionInfo GdkXPositionInfo;

/* Window implementation for X11
 */

#define GDK_TYPE_WINDOW_IMPL_X11              (gdk_window_impl_x11_get_type ())
#define GDK_WINDOW_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11))
#define GDK_WINDOW_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11Class))
#define GDK_IS_WINDOW_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_X11))
#define GDK_IS_WINDOW_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_X11))
#define GDK_WINDOW_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_X11, GdkWindowImplX11Class))

struct _GdkWindowImplX11
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;

  Window xid;

  GdkToplevelX11 *toplevel;	/* Toplevel-specific information */
  GdkCursor *cursor;
  GHashTable *device_cursor;

  gint8 toplevel_window_type;
  guint no_bg : 1;        /* Set when the window background is temporarily
                           * unset during resizing and scaling */
  guint override_redirect : 1;
  guint frame_clock_connected : 1;
  guint frame_sync_enabled : 1;

  cairo_surface_t *cairo_surface;

#if defined (HAVE_XCOMPOSITE) && defined(HAVE_XDAMAGE) && defined (HAVE_XFIXES)
  Damage damage;
#endif
};
 
struct _GdkWindowImplX11Class 
{
  GdkWindowImplClass parent_class;
};

struct _GdkToplevelX11
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
  guint have_hidden : 1;	/* _NET_WM_STATE_HIDDEN */

  guint is_leader : 1;

  /* Set if the WM is presenting us as focused, i.e. with active decorations
   */
  guint have_focused : 1;

  guint in_frame : 1;

  /* If we're expecting a response from the compositor after painting a frame */
  guint frame_pending : 1;

  /* Whether pending_counter_value/configure_counter_value are updates
   * to the extended update counter */
  guint pending_counter_value_is_extended : 1;
  guint configure_counter_value_is_extended : 1;

  gulong map_serial;	/* Serial of last transition from unmapped */
  
  cairo_surface_t *icon_pixmap;
  cairo_surface_t *icon_mask;
  GdkWindow *group_leader;

  /* Time of most recent user interaction. */
  gulong user_time;

  /* We use an extra X window for toplevel windows that we XSetInputFocus()
   * to in order to avoid getting keyboard events redirected to subwindows
   * that might not even be part of this app
   */
  Window focus_window;
 
#ifdef HAVE_XSYNC
  XID update_counter;
  XID extended_update_counter;
  gint64 pending_counter_value; /* latest _NET_WM_SYNC_REQUEST value received */
  gint64 configure_counter_value; /* Latest _NET_WM_SYNC_REQUEST value received
				 * where we have also seen the corresponding
				 * ConfigureNotify
				 */
  gint64 current_counter_value;

  /* After a _NET_WM_FRAME_DRAWN message, this is the soonest that we think
   * frame after will be presented */
  gint64 throttled_presentation_time;
#endif
};

GType gdk_window_impl_x11_get_type (void);

void            gdk_x11_window_set_user_time        (GdkWindow *window,
						     guint32    timestamp);
void            gdk_x11_window_set_frame_sync_enabled (GdkWindow *window,
                                                       gboolean   frame_sync_enabled);

GdkToplevelX11 *_gdk_x11_window_get_toplevel        (GdkWindow *window);
void            _gdk_x11_window_tmp_unset_bg        (GdkWindow *window,
						     gboolean   recurse);
void            _gdk_x11_window_tmp_reset_bg        (GdkWindow *window,
						     gboolean   recurse);
void            _gdk_x11_window_tmp_unset_parent_bg (GdkWindow *window);
void            _gdk_x11_window_tmp_reset_parent_bg (GdkWindow *window);

GdkCursor      *_gdk_x11_window_get_cursor          (GdkWindow *window);

void            _gdk_x11_window_update_size         (GdkWindowImplX11 *impl);

G_END_DECLS

#endif /* __GDK_WINDOW_X11_H__ */
