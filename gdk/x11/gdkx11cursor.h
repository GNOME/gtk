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

#if !defined (__GDKX_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkx.h> can be included directly."
#endif

#ifndef __GDK_X11_CURSOR_H__
#define __GDK_X11_CURSOR_H__

#include <gdk/gdk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS

Display *gdk_x11_cursor_get_xdisplay      (GdkCursor   *cursor);
Cursor   gdk_x11_cursor_get_xcursor       (GdkCursor   *cursor);

/**
 * GDK_CURSOR_XDISPLAY:
 * @cursor: a #GdkCursor.
 *
 * Returns the display of a #GdkCursor.
 *
 * Returns: an Xlib <type>Display*</type>.
 */
#define GDK_CURSOR_XDISPLAY(cursor)   (gdk_x11_cursor_get_xdisplay (cursor))

/**
 * GDK_CURSOR_XCURSOR:
 * @cursor: a #GdkCursor.
 *
 * Returns the X cursor belonging to a #GdkCursor.
 *
 * Returns: an Xlib <type>Cursor</type>.
 */
#define GDK_CURSOR_XCURSOR(cursor)    (gdk_x11_cursor_get_xcursor (cursor))

G_END_DECLS

#endif /* __GDK_X11_CURSOR_H__ */
