/*
 * Copyright © 2018 Benjamin Otte
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

#ifndef __GDK_MEMORY_TEXTURE_PRIVATE_H__
#define __GDK_MEMORY_TEXTURE_PRIVATE_H__

#include "gdktextureprivate.h"

G_BEGIN_DECLS

/*
 * GdkMemoryFormat:
 *
 * #GdkMemroyFormat describes a format that bytes can have in memory.
 *
 * It describes formats by listing the contents of the memory passed to it.
 * So GDK_MEMORY_A8R8G8B8 will be 8 bits of alpha, followed by 8 bites of each 
 * blue, green and red. It is not endian-dependant, so CAIRO_FORMAT_ARGB32 is
 * represented by 2 different GdkMemoryFormats.
 * 
 * Its naming is modelled after VkFormat (see
 * https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#VkFormat
 * for details).
 */
typedef enum {
  GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
  GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
  GDK_MEMORY_B8G8R8A8,
  GDK_MEMORY_A8R8G8B8,
  GDK_MEMORY_R8G8B8A8,
  GDK_MEMORY_A8B8G8R8,
  GDK_MEMORY_R8G8B8,
  GDK_MEMORY_B8G8R8,

  GDK_MEMORY_N_FORMATS
} GdkMemoryFormat;

#define GDK_MEMORY_GDK_PIXBUF_OPAQUE GDK_MEMORY_R8G8B8
#define GDK_MEMORY_GDK_PIXBUF_ALPHA GDK_MEMORY_R8G8B8A8

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GDK_MEMORY_CAIRO_FORMAT_ARGB32 GDK_MEMORY_B8G8R8A8_PREMULTIPLIED
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define GDK_MEMORY_CAIRO_FORMAT_ARGB32 GDK_MEMORY_A8R8G8B8_PREMULTIPLIED
#else
#error "Unknown byte order."
#endif

#define GDK_TYPE_MEMORY_TEXTURE (gdk_memory_texture_get_type ())

G_DECLARE_FINAL_TYPE (GdkMemoryTexture, gdk_memory_texture, GDK, MEMORY_TEXTURE, GdkTexture)

GdkTexture *            gdk_memory_texture_new              (int                width,
                                                             int                height,
                                                             GdkMemoryFormat    format,
                                                             GBytes            *bytes,
                                                             gsize              stride);

GdkMemoryFormat         gdk_memory_texture_get_format       (GdkMemoryTexture  *self);
const guchar *          gdk_memory_texture_get_data         (GdkMemoryTexture  *self);
gsize                   gdk_memory_texture_get_stride       (GdkMemoryTexture  *self);

void                    gdk_memory_convert                  (guchar            *dest_data,
                                                             gsize              dest_stride,
                                                             GdkMemoryFormat    dest_format,
                                                             const guchar      *src_data,
                                                             gsize              src_stride,
                                                             GdkMemoryFormat    src_format,
                                                             gsize              width,
                                                             gsize              height);


G_END_DECLS

#endif /* __GDK_MEMORY_TEXTURE_PRIVATE_H__ */
