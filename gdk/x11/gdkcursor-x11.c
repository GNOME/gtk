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
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include "gdk.h"
#include "gdkprivate.h"


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

  return cursor;
}

GdkCursor*
gdk_cursor_new_from_pixmap (GdkPixmap *source, GdkPixmap *mask, GdkColor *fg, GdkColor *bg, gint x, gint y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;
  Pixmap source_pixmap, mask_pixmap;
  Cursor xcursor;
  XColor xfg, xbg;
  
  source_pixmap = ((GdkPixmapPrivate *) source)->xwindow;
  mask_pixmap   = ((GdkPixmapPrivate *) mask)->xwindow;

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

  return cursor;
}

void
gdk_cursor_destroy (GdkCursor *cursor)
{
  GdkCursorPrivate *private;

  g_return_if_fail (cursor != NULL);

  private = (GdkCursorPrivate *) cursor;
  XFreeCursor (private->xdisplay, private->xcursor);

  g_free (private);
}
