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

#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include "gdkprivate-x11.h"
#include "gdkcursor.h"
#include "gdkpixmap-x11.h"
#include <gdk/gdkpixmap.h>

/**
 * gdk_cursor_new:
 * @cursor_type: cursor to create
 * 
 * Creates a new cursor from the set of builtin cursors.
 * Some useful ones are: #GDK_RIGHT_PTR (right-facing arrow),
 * #GDK_CROSSHAIR (crosshair), #GDK_IBEAM (I-beam), #GDK_WATCH (busy),
 * #GDK_FLEUR (for moving objects), #GDK_HAND (a pointing hand),
 * #GDK_LEFT_SIDE (resize left side), #GDK_RIGHT_SIDE (resize right side),
 * #GDK_TOP_LEFT_CORNER (resize northwest corner), #GDK_TOP_RIGHT_CORNER (resize
 * northeast corner), #GDK_BOTTOM_LEFT_CORNER (resize southwest corner),
 * #GDK_BOTTOM_RIGHT_CORNER (resize southeast corner),
 * #GDK_TOP_SIDE (resize top side), #GDK_BOTTOM (resize bottom side),
 * #GDK_SB_H_DOUBLE_ARROW (move vertical splitter),
 * #GDK_SB_V_DOUBLE_ARROW (move horizontal splitter).
 *
 * To make the cursor invisible, use gdk_cursor_new_from_pixmap() to create
 * a cursor with no pixels in it.
 * 
 * Return value: a new #GdkCursor
 **/
GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Cursor xcursor;

  xcursor = XCreateFontCursor (gdk_display, cursor_type);
  private = g_new (GdkCursorPrivate, 1);
  private->xdisplay = gdk_display;
  private->xcursor = xcursor;
  cursor = (GdkCursor*) private;
  cursor->type = cursor_type;
  cursor->ref_count = 1;
  
  return cursor;
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source,
			    GdkPixmap *mask,
			    GdkColor  *fg,
			    GdkColor  *bg,
			    gint       x,
			    gint       y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Pixmap source_pixmap, mask_pixmap;
  Cursor xcursor;
  XColor xfg, xbg;

  g_return_val_if_fail (GDK_IS_PIXMAP (source), NULL);
  g_return_val_if_fail (GDK_IS_PIXMAP (mask), NULL);
  g_return_val_if_fail (fg != NULL, NULL);
  g_return_val_if_fail (bg != NULL, NULL);

  source_pixmap = GDK_PIXMAP_XID (source);
  mask_pixmap   = GDK_PIXMAP_XID (mask);

  xfg.pixel = fg->pixel;
  xfg.red = fg->red;
  xfg.blue = fg->blue;
  xfg.green = fg->green;
  xbg.pixel = bg->pixel;
  xbg.red = bg->red;
  xbg.blue = bg->blue;
  xbg.green = bg->green;
  
  xcursor = XCreatePixmapCursor (gdk_display, source_pixmap, mask_pixmap, &xfg, &xbg, x, y);
  private = g_new (GdkCursorPrivate, 1);
  private->xdisplay = gdk_display;
  private->xcursor = xcursor;
  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;
  
  return cursor;
}

void
_gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count == 0);

  private = (GdkCursorPrivate *) cursor;
  XFreeCursor (private->xdisplay, private->xcursor);

  g_free (private);
}

Display *
gdk_x11_cursor_get_xdisplay (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return ((GdkCursorPrivate *)cursor)->xdisplay;
}

Cursor
gdk_x11_cursor_get_xcursor (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, None);

  return ((GdkCursorPrivate *)cursor)->xcursor;
}
