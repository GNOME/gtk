/*
 * Copyright © 2021 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GDK_MEMORY_CONVERT_PRIVATE_H__
#define __GDK_MEMORY_CONVERT_PRIVATE_H__

#include "gdkenums.h"
#include "gdkcolorspace.h"

G_BEGIN_DECLS

typedef enum {
  GDK_MEMORY_ALPHA_PREMULTIPLIED,
  GDK_MEMORY_ALPHA_STRAIGHT,
  GDK_MEMORY_ALPHA_OPAQUE
} GdkMemoryAlpha;

gsize                   gdk_memory_format_alignment         (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryAlpha          gdk_memory_format_alpha             (GdkMemoryFormat             format) G_GNUC_CONST;
gsize                   gdk_memory_format_bytes_per_pixel   (GdkMemoryFormat             format) G_GNUC_CONST;
gboolean                gdk_memory_format_prefers_high_depth(GdkMemoryFormat             format) G_GNUC_CONST;
gboolean                gdk_memory_format_gl_format         (GdkMemoryFormat             format,
                                                             gboolean                    gles,
                                                             guint                      *out_internal_format,
                                                             guint                      *out_format,
                                                             guint                      *out_type);

void                    gdk_memory_convert                  (guchar                     *dest_data,
                                                             gsize                       dest_stride,
                                                             GdkMemoryFormat             dest_format,
                                                             GdkColorSpace              *dest_color_space,
                                                             const guchar               *src_data,
                                                             gsize                       src_stride,
                                                             GdkMemoryFormat             src_format,
                                                             GdkColorSpace              *src_color_space,
                                                             gsize                       width,
                                                             gsize                       height);


G_END_DECLS

#endif /* __GDK_MEMORY_CONVERT_PRIVATE_H__ */
