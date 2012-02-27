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

#include <string.h>
#include <errno.h>

struct _GdkBroadwayCursor
{
  GdkCursor cursor;
};

struct _GdkBroadwayCursorClass
{
  GdkCursorClass cursor_class;
};

/*** GdkBroadwayCursor ***/

G_DEFINE_TYPE (GdkBroadwayCursor, gdk_broadway_cursor, GDK_TYPE_CURSOR)

static GdkPixbuf* gdk_broadway_cursor_get_image (GdkCursor *cursor);

static void
gdk_broadway_cursor_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_broadway_cursor_parent_class)->finalize (object);
}

static void
gdk_broadway_cursor_class_init (GdkBroadwayCursorClass *xcursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (xcursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (xcursor_class);

  object_class->finalize = gdk_broadway_cursor_finalize;

  cursor_class->get_image = gdk_broadway_cursor_get_image;
}

static void
gdk_broadway_cursor_init (GdkBroadwayCursor *cursor)
{
}

/* Called by gdk_display_broadway_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_broadway_cursor_display_finalize (GdkDisplay *display)
{
}

GdkCursor*
_gdk_broadway_display_get_cursor_for_type (GdkDisplay    *display,
					   GdkCursorType  cursor_type)
{
  GdkBroadwayCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_object_new (GDK_TYPE_BROADWAY_CURSOR,
                          "cursor-type", cursor_type,
                          "display", display,
			  NULL);

  return GDK_CURSOR (private);
}

static GdkPixbuf*
gdk_broadway_cursor_get_image (GdkCursor *cursor)
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
_gdk_broadway_display_get_cursor_for_pixbuf (GdkDisplay *display,
					     GdkPixbuf  *pixbuf,
					     gint        x,
					     gint        y)
{
  GdkBroadwayCursor *private;
  GdkCursor *cursor;

  private = g_object_new (GDK_TYPE_BROADWAY_CURSOR, 
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  cursor = (GdkCursor *) private;

  return cursor;
}

GdkCursor*
_gdk_broadway_display_get_cursor_for_name (GdkDisplay  *display,
					   const gchar *name)
{
  GdkBroadwayCursor *private;

  private = g_object_new (GDK_TYPE_BROADWAY_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);

  return GDK_CURSOR (private);
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
