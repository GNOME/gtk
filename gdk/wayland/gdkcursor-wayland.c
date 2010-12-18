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

#define GDK_PIXBUF_ENABLE_BACKEND

#include <string.h>

#include "gdkprivate-wayland.h"
#include "gdkcursorprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkwayland.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#define GDK_TYPE_WAYLAND_CURSOR              (_gdk_wayland_cursor_get_type ())
#define GDK_WAYLAND_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursor))
#define GDK_WAYLAND_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursorClass))
#define GDK_IS_WAYLAND_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_CURSOR))
#define GDK_IS_WAYLAND_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_CURSOR))
#define GDK_WAYLAND_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursorClass))

typedef struct _GdkWaylandCursor GdkWaylandCursor;
typedef struct _GdkWaylandCursorClass GdkWaylandCursorClass;

struct _GdkWaylandCursor
{
  GdkCursor cursor;
  gchar *name;
  guint serial;
};

struct _GdkWaylandCursorClass
{
  GdkCursorClass cursor_class;
};

G_DEFINE_TYPE (GdkWaylandCursor, _gdk_wayland_cursor, GDK_TYPE_CURSOR)

static guint theme_serial = 0;

static void
gdk_wayland_cursor_finalize (GObject *object)
{
  GdkWaylandCursor *cursor = GDK_WAYLAND_CURSOR (object);

  g_free (cursor->name);

  G_OBJECT_CLASS (_gdk_wayland_cursor_parent_class)->finalize (object);
}

static GdkPixbuf*
gdk_wayland_cursor_get_image (GdkCursor *cursor)
{
  return NULL;
}

static void
_gdk_wayland_cursor_class_init (GdkWaylandCursorClass *wayland_cursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (wayland_cursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (wayland_cursor_class);

  object_class->finalize = gdk_wayland_cursor_finalize;

  cursor_class->get_image = gdk_wayland_cursor_get_image;
}

static void
_gdk_wayland_cursor_init (GdkWaylandCursor *cursor)
{
}

GdkCursor*
_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
					  GdkCursorType  cursor_type)
{
  GdkWaylandCursor *private;

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->name = NULL;
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

GdkCursor*
_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
					  const gchar *name)
{
  GdkWaylandCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->name = g_strdup (name);
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_pixbuf (GdkDisplay *display,
					    GdkPixbuf  *pixbuf,
					    gint        x,
					    gint        y)
{
  GdkWaylandCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);

  private->name = NULL;
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

void
_gdk_wayland_display_get_default_cursor_size (GdkDisplay *display,
					      guint       *width,
					      guint       *height)
{
  /* FIXME: wayland settings? */
  *width = 64;
  *height = 64;
}

void
_gdk_wayland_display_get_maximal_cursor_size (GdkDisplay *display,
					      guint       *width,
					      guint       *height)
{
  *width = 256;
  *height = 256;
}

gboolean
_gdk_wayland_display_supports_cursor_alpha (GdkDisplay *display)
{
  return TRUE;
}

gboolean
_gdk_wayland_display_supports_cursor_color (GdkDisplay *display)
{
  return TRUE;
}
