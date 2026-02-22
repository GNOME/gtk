/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Red Hat, Inc.
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

#pragma once

#include <gdk/gdk.h>
#include <gsk/gsk.h>

G_BEGIN_DECLS

/* Used for icons in the file chooser */
GdkTexture *gdk_texture_new_from_filename_at_scale  (const char    *filename,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);

/* Used in the gtk4-encode-symbolic tool */
GdkTexture *gdk_texture_new_from_filename_symbolic  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);

GdkPaintable *gdk_paintable_new_from_filename       (const char    *filename,
                                                     GError       **error);
GdkPaintable *gdk_paintable_new_from_resource       (const char    *path);
GdkPaintable *gdk_paintable_new_from_file           (GFile         *file,
                                                     GError       **error);
GdkPaintable *gdk_paintable_new_from_stream         (GInputStream  *stream,
                                                     GCancellable  *cancellable,
                                                     GError       **error);

G_END_DECLS
