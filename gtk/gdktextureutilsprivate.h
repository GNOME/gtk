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

GdkTexture *gdk_texture_new_from_stream             (GInputStream  *stream,
                                                     GCancellable  *cancellable,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_stream_at_scale    (GInputStream  *stream,
                                                     int            width,
                                                     int            height,
                                                     GCancellable  *cancellable,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_resource_at_scale  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_filename_at_scale  (const char    *filename,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);

GdkTexture *gdk_texture_new_from_filename_symbolic  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_file_symbolic      (GFile         *file,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);
GdkTexture *gdk_texture_new_from_resource_symbolic  (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     GError       **error);

GdkPaintable *gdk_paintable_new_from_filename       (const char    *filename);
GdkPaintable *gdk_paintable_new_from_resource       (const char    *path);
GdkPaintable *gdk_paintable_new_from_file           (GFile         *file);

G_END_DECLS
