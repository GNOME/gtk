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

#include "config.h"

/* needs to be first because any header might include gdk-pixbuf.h otherwise */
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"

#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

#include <string.h>
#include <errno.h>


/* Called by gdk_display_broadway_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_broadway_cursor_display_finalize (GdkDisplay *display)
{
}

GdkCursor*
gdk_cursor_new_for_display (GdkDisplay    *display,
			    GdkCursorType  cursor_type)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;

  cursor = (GdkCursor *) private;
  cursor->type = cursor_type;
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

  g_free (private);
}


/**
 * gdk_cursor_get_display:
 * @cursor: a #GdkCursor.
 *
 * Returns the display on which the #GdkCursor is defined.
 *
 * Returns: the #GdkDisplay associated to @cursor
 *
 * Since: 2.2
 */

GdkDisplay *
gdk_cursor_get_display (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return ((GdkCursorPrivate *)cursor)->display;
}

GdkPixbuf*
gdk_cursor_get_image (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return NULL;
}

void
_gdk_broadway_cursor_update_theme (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
}

GdkCursor *
gdk_cursor_new_from_pixbuf (GdkDisplay *display,
			    GdkPixbuf  *pixbuf,
			    gint        x,
			    gint        y)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;

  return cursor;
}

GdkCursor*
gdk_cursor_new_from_name (GdkDisplay  *display,
			  const gchar *name)
{
  GdkCursorPrivate *private;
  GdkCursor *cursor;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_new (GdkCursorPrivate, 1);
  private->display = display;

  cursor = (GdkCursor *) private;
  cursor->type = GDK_CURSOR_IS_PIXMAP;
  cursor->ref_count = 1;

  return cursor;
}

gboolean
gdk_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean
gdk_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

guint
gdk_display_get_default_cursor_size (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return 20;
}

void
gdk_display_get_maximal_cursor_size (GdkDisplay *display,
				     guint       *width,
				     guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  *width = 128;
  *height = 128;
}
