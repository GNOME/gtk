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

#pragma once

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_CURSOR              (gdk_cursor_get_type ())
#define GDK_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_CURSOR, GdkCursor))
#define GDK_IS_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_CURSOR))

/* Cursors
 */

GDK_AVAILABLE_IN_ALL
GType      gdk_cursor_get_type           (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GdkCursor* gdk_cursor_new_from_texture   (GdkTexture      *texture,
                                          int              hotspot_x,
                                          int              hotspot_y,
                                          GdkCursor       *fallback);
GDK_AVAILABLE_IN_ALL
GdkCursor*  gdk_cursor_new_from_name     (const char      *name,
                                          GdkCursor       *fallback);

/**
 * GdkCursorGetTextureCallback:
 * @cursor: the `GdkCursor`
 * @cursor_size: the nominal cursor size, in application pixels
 * @scale: the device scale
 * @width: (out): return location for the actual cursor width,
 *   in application pixels
 * @height: (out): return location for the actual cursor height,
 *   in application pixels
 * @hotspot_x: (out): return location for the hotspot X position,
 *   in application pixels
 * @hotspot_y: (out): return location for the hotspot Y position,
 *   in application pixels
 * @data: User data for the callback
 *
 * The type of callback used by a dynamic `GdkCursor` to generate
 * a texture for the cursor image at the given @cursor_size
 * and @scale.
 *
 * The actual cursor size in application pixels may be different
 * from @cursor_size x @cursor_size, and will be returned in
 * @width, @height. The returned texture should have a size that
 * corresponds to the actual cursor size, in device pixels (i.e.
 * application pixels, multiplied by @scale).
 *
 * This function may fail and return `NULL`, in which case
 * the fallback cursor will be used.
 *
 * Returns: (nullable) (transfer full): the cursor image, or
 *   `NULL` if none could be produced.
 */
typedef GdkTexture * (* GdkCursorGetTextureCallback) (GdkCursor   *cursor,
                                                      int          cursor_size,
                                                      double       scale,
                                                      int         *width,
                                                      int         *height,
                                                      int         *hotspot_x,
                                                      int         *hotspot_y,
                                                      gpointer     data);

GDK_AVAILABLE_IN_4_16
GdkCursor * gdk_cursor_new_from_callback (GdkCursorGetTextureCallback  callback,
                                          gpointer                     data,
                                          GDestroyNotify               destroy,
                                          GdkCursor                   *fallback);

GDK_AVAILABLE_IN_ALL
GdkCursor * gdk_cursor_get_fallback      (GdkCursor       *cursor);
GDK_AVAILABLE_IN_ALL
const char *gdk_cursor_get_name          (GdkCursor       *cursor);
GDK_AVAILABLE_IN_ALL
GdkTexture *gdk_cursor_get_texture       (GdkCursor       *cursor);
GDK_AVAILABLE_IN_ALL
int         gdk_cursor_get_hotspot_x     (GdkCursor       *cursor);
GDK_AVAILABLE_IN_ALL
int         gdk_cursor_get_hotspot_y     (GdkCursor       *cursor);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GdkCursor, g_object_unref)

G_END_DECLS

