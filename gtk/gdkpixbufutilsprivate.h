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

#ifndef __GDK_PIXBUF_UTILS_PRIVATE_H__
#define __GDK_PIXBUF_UTILS_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

GdkPixbuf *_gdk_pixbuf_new_from_stream (GInputStream  *stream,
                                        const char    *format,
                                        GCancellable  *cancellable,
                                        GError       **error);
GdkPixbuf *_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
                                                 const char    *format,
                                                 int            width,
                                                 int            height,
                                                 gboolean       aspect,
                                                 GCancellable  *cancellable,
                                                 GError       **error);
GdkPixbuf *_gdk_pixbuf_new_from_stream_scaled   (GInputStream  *stream,
                                                 const char    *format,
                                                 double         scale,
                                                 GCancellable  *cancellable,
                                                 GError       **error);
GdkPixbuf *_gdk_pixbuf_new_from_resource (const char   *resource_path,
                                          const char   *format,
                                          GError      **error);
GdkPixbuf *_gdk_pixbuf_new_from_resource_at_scale (const char   *resource_path,
                                                   const char   *format,
                                                   int           width,
                                                   int           height,
                                                   gboolean      preserve_aspect,
                                                   GError      **error);
GdkPixbuf *_gdk_pixbuf_new_from_resource_scaled (const char   *resource_path,
                                                 const char   *format,
                                                 double        scale,
                                                 GError      **error);

GdkPixbuf *gtk_color_symbolic_pixbuf              (GdkPixbuf     *symbolic,
                                                                                                                           const GdkRGBA *fg_color,
                                                   const GdkRGBA *success_color,
                                                   const GdkRGBA *warning_color,
                                                   const GdkRGBA *error_color);

GdkPixbuf *gtk_make_symbolic_pixbuf_from_data     (const char    *data,
                                                   gsize          len,
                                                   int            width,
                                                   int            height,
                                                   double         scale,
                                                   GError       **error);
GdkPixbuf *gtk_make_symbolic_pixbuf_from_file     (GFile         *file,
                                                   int            width,
                                                   int            height,
                                                   double         scale,
                                                   GError       **error);
GdkPixbuf *gtk_make_symbolic_pixbuf_from_path     (const char    *path,
                                                   int            width,
                                                   int            height,
                                                   double         scale,
                                                   GError       **error);
GdkPixbuf *gtk_make_symbolic_pixbuf_from_resource (const char    *path,
                                                   int            width,
                                                   int            height,
                                                   double         scale,
                                                   GError       **error);
GdkTexture *gtk_make_symbolic_texture_from_file     (GFile         *file,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                     GError       **error);
GdkTexture *gtk_make_symbolic_texture_from_resource (const char    *path,
                                                     int            width,
                                                     int            height,
                                                     double         scale,
                                                   GError       **error);
G_END_DECLS

#endif  /* __GDK_PIXBUF_UTILS_PRIVATE_H__ */
