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

#include "gdkcursor.h"

#include "gdkdisplay.h"
#include "gdkinternals.h"


/**
 * SECTION:cursors
 * @Short_description: Standard and pixmap cursors
 * @Title: Cursors
 *
 * These functions are used to create and destroy cursors.
 * There is a number of standard cursors, but it is also
 * possible to construct new cursors from pixbufs. There
 * may be limitations as to what kinds of cursors can be
 * constructed on a given display, see
 * gdk_display_supports_cursor_alpha(),
 * gdk_display_supports_cursor_color(),
 * gdk_display_get_default_cursor_size() and
 * gdk_display_get_maximal_cursor_size().
 *
 * Cursors by themselves are not very interesting, they must be be
 * bound to a window for users to see them. This is done with
 * gdk_window_set_cursor() or by setting the cursor member of the
 * #GdkWindowAttr struct passed to gdk_window_new().
 */


G_DEFINE_BOXED_TYPE (GdkCursor, gdk_cursor,
                     gdk_cursor_ref,
                     gdk_cursor_unref)

/**
 * gdk_cursor_ref:
 * @cursor: a #GdkCursor
 * 
 * Adds a reference to @cursor.
 * 
 * Return value: Same @cursor that was passed in
 **/
GdkCursor*
gdk_cursor_ref (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, NULL);
  g_return_val_if_fail (cursor->ref_count > 0, NULL);

  cursor->ref_count += 1;

  return cursor;
}

/**
 * gdk_cursor_unref:
 * @cursor: a #GdkCursor
 *
 * Removes a reference from @cursor, deallocating the cursor
 * if no references remain.
 * 
 **/
void
gdk_cursor_unref (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
  g_return_if_fail (cursor->ref_count > 0);

  cursor->ref_count -= 1;

  if (cursor->ref_count == 0)
    _gdk_cursor_destroy (cursor);
}

/**
 * gdk_cursor_new:
 * @cursor_type: cursor to create
 * 
 * Creates a new cursor from the set of builtin cursors for the default display.
 * See gdk_cursor_new_for_display().
 *
 * To make the cursor invisible, use %GDK_BLANK_CURSOR.
 * 
 * Return value: a new #GdkCursor
 **/
GdkCursor*
gdk_cursor_new (GdkCursorType cursor_type)
{
  return gdk_cursor_new_for_display (gdk_display_get_default(), cursor_type);
}

/**
 * gdk_cursor_get_cursor_type:
 * @cursor:  a #GdkCursor
 *
 * Returns the cursor type for this cursor.
 *
 * Return value: a #GdkCursorType
 *
 * Since: 2.22
 **/
GdkCursorType
gdk_cursor_get_cursor_type (GdkCursor *cursor)
{
  g_return_val_if_fail (cursor != NULL, GDK_BLANK_CURSOR);

  return cursor->type;
}
