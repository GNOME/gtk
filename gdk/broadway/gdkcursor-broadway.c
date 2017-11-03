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

#include "config.h"

/* needs to be first because any header might include gdk-pixbuf.h otherwise */
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"

#include "gdkprivate-broadway.h"
#include "gdkdisplay-broadway.h"

/* Called by gdk_display_broadway_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_broadway_cursor_display_finalize (GdkDisplay *display)
{
}

void
_gdk_broadway_cursor_update_theme (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
}

gboolean
_gdk_broadway_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean
_gdk_broadway_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

void
_gdk_broadway_display_get_default_cursor_size (GdkDisplay *display,
					       guint      *width,
					       guint      *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  *width = *height = 20;
}

void
_gdk_broadway_display_get_maximal_cursor_size (GdkDisplay *display,
					       guint       *width,
					       guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  *width = 128;
  *height = 128;
}
