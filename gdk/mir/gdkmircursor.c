/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcursorprivate.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

typedef struct GdkMirCursor      GdkMirCursor;
typedef struct GdkMirCursorClass GdkMirCursorClass;

#define GDK_TYPE_MIR_CURSOR              (gdk_mir_cursor_get_type ())
#define GDK_MIR_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_CURSOR, GdkMirCursor))
#define GDK_MIR_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_CURSOR, GdkMirCursorClass))
#define GDK_IS_MIR_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_CURSOR))
#define GDK_IS_MIR_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_CURSOR))
#define GDK_MIR_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_CURSOR, GdkMirCursorClass))

struct GdkMirCursor
{
  GdkCursor parent_instance;
};

struct GdkMirCursorClass
{
  GdkCursorClass parent_class;
};

G_DEFINE_TYPE (GdkMirCursor, gdk_mir_cursor, GDK_TYPE_CURSOR)

GdkCursor *
_gdk_mir_cursor_new (GdkDisplay *display, GdkCursorType type)
{
  return g_object_new (GDK_TYPE_MIR_CURSOR, "display", display, "cursor-type", type, NULL);
}

cairo_surface_t *
gdk_mir_cursor_get_surface (GdkCursor *cursor,
                            gdouble   *x_hot,
                            gdouble   *y_hot)
{
  g_printerr ("gdk_mir_cursor_get_surface\n");
  return NULL;
}

static void
gdk_mir_cursor_init (GdkMirCursor *cursor)
{
}

static void
gdk_mir_cursor_class_init (GdkMirCursorClass *klass)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (klass);

  cursor_class->get_surface = gdk_mir_cursor_get_surface;
}
