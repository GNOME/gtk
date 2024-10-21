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

#pragma once

#include "gdkenums.h"
#include "gdktypes.h"

/* epoxy needs this, see https://github.com/anholt/libepoxy/issues/299 */
#ifdef GDK_WINDOWING_WIN32
#include <windows.h>
#endif

#include <epoxy/gl.h>

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

G_BEGIN_DECLS

typedef enum {
  GDK_MEMORY_ALPHA_PREMULTIPLIED,
  GDK_MEMORY_ALPHA_STRAIGHT,
  GDK_MEMORY_ALPHA_OPAQUE
} GdkMemoryAlpha;

typedef enum {
  GDK_MEMORY_NONE,
  GDK_MEMORY_U8,
  GDK_MEMORY_U8_SRGB,
  GDK_MEMORY_U16,
  GDK_MEMORY_FLOAT16,
  GDK_MEMORY_FLOAT32,

  GDK_N_DEPTHS
} GdkMemoryDepth;

#define GDK_MEMORY_DEPTH_BITS 3

gsize                   gdk_memory_format_alignment         (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryAlpha          gdk_memory_format_alpha             (GdkMemoryFormat             format) G_GNUC_CONST;
gsize                   gdk_memory_format_bytes_per_pixel   (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_format_get_premultiplied (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_format_get_straight      (GdkMemoryFormat             format) G_GNUC_CONST;
const GdkMemoryFormat * gdk_memory_format_get_fallbacks     (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryDepth          gdk_memory_format_get_depth         (GdkMemoryFormat             format,
                                                             gboolean                    srgb) G_GNUC_CONST;
gsize                   gdk_memory_format_min_buffer_size   (GdkMemoryFormat             format,
                                                             gsize                       stride,
                                                             gsize                       width,
                                                             gsize                       height) G_GNUC_CONST;
gboolean                gdk_memory_depth_is_srgb            (GdkMemoryDepth              depth) G_GNUC_CONST;
GdkMemoryDepth          gdk_memory_depth_merge              (GdkMemoryDepth              depth1,
                                                             GdkMemoryDepth              depth2) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_depth_get_format         (GdkMemoryDepth              depth) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_depth_get_alpha_format   (GdkMemoryDepth              depth) G_GNUC_CONST;
const char *            gdk_memory_depth_get_name           (GdkMemoryDepth              depth);
void                    gdk_memory_format_gl_format         (GdkMemoryFormat             format,
                                                             gboolean                    gles,
                                                             GLint                      *out_internal_format,
                                                             GLint                      *out_internal_srgb_format,
                                                             GLenum                     *out_format,
                                                             GLenum                     *out_type,
                                                             GLint                       out_swizzle[4]);
gboolean                gdk_memory_format_gl_rgba_format    (GdkMemoryFormat             format,
                                                             gboolean                    gles,
                                                             GdkMemoryFormat            *out_actual_format,
                                                             GLint                      *out_internal_format,
                                                             GLint                      *out_internal_srgb_format,
                                                             GLenum                     *out_format,
                                                             GLenum                     *out_type,
                                                             GLint                       out_swizzle[4]);
#ifdef GDK_RENDERING_VULKAN
VkFormat                gdk_memory_format_vk_format         (GdkMemoryFormat             format,
                                                             VkComponentMapping         *out_swizzle);
VkFormat                gdk_memory_format_vk_srgb_format    (GdkMemoryFormat             format);
VkFormat                gdk_memory_format_vk_rgba_format    (GdkMemoryFormat             format,
                                                             GdkMemoryFormat            *out_rgba_format,
                                                             VkComponentMapping         *out_swizzle);
#endif
guint32                 gdk_memory_format_get_dmabuf_fourcc (GdkMemoryFormat             format);
const char *            gdk_memory_format_get_name          (GdkMemoryFormat             format);

void                    gdk_memory_convert                  (guchar                     *dest_data,
                                                             gsize                       dest_stride,
                                                             GdkMemoryFormat             dest_format,
                                                             GdkColorState              *dest_cs,
                                                             const guchar               *src_data,
                                                             gsize                       src_stride,
                                                             GdkMemoryFormat             src_format,
                                                             GdkColorState              *src_cs,
                                                             gsize                       width,
                                                             gsize                       height);
void                    gdk_memory_convert_color_state      (guchar                     *data,
                                                             gsize                       stride,
                                                             GdkMemoryFormat             format,
                                                             GdkColorState              *src_color_state,
                                                             GdkColorState              *dest_color_state,
                                                             gsize                       width,
                                                             gsize                       height);
void                    gdk_memory_mipmap                   (guchar                     *dest,
                                                             gsize                       dest_stride,
                                                             GdkMemoryFormat             dest_format,
                                                             const guchar               *src,
                                                             gsize                       src_stride,
                                                             GdkMemoryFormat             src_format,
                                                             gsize                       src_width,
                                                             gsize                       src_height,
                                                             guint                       lod_level,
                                                             gboolean                    linear);


G_END_DECLS

