/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2013 Chun-wei Fan
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

#ifndef __GDK_WIN32_MISC_H__
#define __GDK_WIN32_MISC_H__

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

#ifndef STRICT
#define STRICT			/* We want strict type checks */
#endif
#include <windows.h>
#include <commctrl.h>

G_BEGIN_DECLS

#ifdef INSIDE_GDK_WIN32

#include "gdkprivate-win32.h"

#define GDK_SURFACE_HWND(win)          (GDK_WIN32_SURFACE(win)->handle)
#else
/* definition for exported 'internals' go here */
#define GDK_SURFACE_HWND(d) (gdk_win32_surface_get_handle (d))

#endif /* INSIDE_GDK_WIN32 */

/* These need to be here so gtkstatusicon.c can pick them up if needed. */
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN 0x020B
#endif
#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP 0x020C
#endif
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(w) (HIWORD(w))
#endif
#ifndef XBUTTON1
#define XBUTTON1 1
#endif
#ifndef XBUTTON2
#define XBUTTON2 2
#endif

/* Return true if the GdkSurface is a win32 implemented window */
GDK_AVAILABLE_IN_ALL
gboolean      gdk_win32_surface_is_win32 (GdkSurface *window);
GDK_AVAILABLE_IN_ALL
HWND          gdk_win32_surface_get_impl_hwnd (GdkSurface *window);

/* Return the Gdk* for a particular HANDLE */
GDK_AVAILABLE_IN_ALL
gpointer      gdk_win32_handle_table_lookup (HWND handle);
/* Translate from window to Windows handle */
GDK_AVAILABLE_IN_ALL
HGDIOBJ       gdk_win32_surface_get_handle (GdkSurface *window);

GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_win32_surface_lookup_for_display (GdkDisplay *display,
                                                     HWND        anid);

#if defined (INSIDE_GDK_WIN32) || defined (GTK_COMPILATION) || defined (GTK_COMPILATION)

/* For internal GTK use only */
GDK_AVAILABLE_IN_ALL
GdkPixbuf    *gdk_win32_icon_to_pixbuf_libgtk_only (HICON hicon,
                                                    double *x_hot,
                                                    double *y_hot);
GDK_AVAILABLE_IN_ALL
HICON         gdk_win32_pixbuf_to_hicon_libgtk_only (GdkPixbuf *pixbuf);
GDK_AVAILABLE_IN_ALL
void          gdk_win32_set_modal_dialog_libgtk_only (HWND window);

#endif

G_END_DECLS

#endif /* __GDK_WIN32_MISC_H__ */
