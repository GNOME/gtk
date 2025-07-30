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
#include "gdkmemorylayoutprivate.h"
#include "gdkdxgiformatprivate.h"
#include "gdkswizzleprivate.h"
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

typedef enum {
  GDK_SHADER_DEFAULT,
  GDK_SHADER_STRAIGHT,
  GDK_SHADER_2_PLANES,
  GDK_SHADER_3_PLANES,
  GDK_SHADER_3_PLANES_10BIT_LSB,
  GDK_SHADER_3_PLANES_12BIT_LSB
} GdkShaderOp;

static inline gsize
gdk_shader_op_get_n_shaders (GdkShaderOp op)
{
  static gsize n_shaders[] = { 1, 1, 2, 3, 3, 3 };
  return n_shaders[op];
}

gsize                   gdk_memory_format_alignment         (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryAlpha          gdk_memory_format_alpha             (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_format_get_premultiplied (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_format_get_straight      (GdkMemoryFormat             format) G_GNUC_CONST;
const GdkMemoryFormat * gdk_memory_format_get_fallbacks     (GdkMemoryFormat             format) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_format_get_mipmap_format (GdkMemoryFormat             format) G_GNUC_CONST;
gboolean                gdk_memory_format_get_rgba_format   (GdkMemoryFormat             format,
                                                             GdkMemoryFormat            *out_format,
                                                             GdkSwizzle                 *out_swizzle);
GdkMemoryDepth          gdk_memory_format_get_depth         (GdkMemoryFormat             format,
                                                             gboolean                    srgb) G_GNUC_CONST;
gsize                   gdk_memory_format_get_n_planes      (GdkMemoryFormat             format) G_GNUC_CONST;
gsize                   gdk_memory_format_get_block_width   (GdkMemoryFormat             format) G_GNUC_CONST;
gsize                   gdk_memory_format_get_block_height  (GdkMemoryFormat             format) G_GNUC_CONST;
gboolean                gdk_memory_format_is_block_boundary (GdkMemoryFormat             format,
                                                             gsize                       x,
                                                             gsize                       y) G_GNUC_CONST;
gsize                   gdk_memory_format_get_plane_block_width
                                                            (GdkMemoryFormat             format,
                                                             gsize                       plane) G_GNUC_CONST;
gsize                   gdk_memory_format_get_plane_block_height
                                                            (GdkMemoryFormat             format,
                                                             gsize                       plane) G_GNUC_CONST;
gsize                   gdk_memory_format_get_plane_block_bytes
                                                            (GdkMemoryFormat             format,
                                                             gsize                       plane) G_GNUC_CONST;
gboolean                gdk_memory_depth_is_srgb            (GdkMemoryDepth              depth) G_GNUC_CONST;
GdkMemoryDepth          gdk_memory_depth_merge              (GdkMemoryDepth              depth1,
                                                             GdkMemoryDepth              depth2) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_depth_get_format         (GdkMemoryDepth              depth) G_GNUC_CONST;
GdkMemoryFormat         gdk_memory_depth_get_alpha_format   (GdkMemoryDepth              depth) G_GNUC_CONST;
const char *            gdk_memory_depth_get_name           (GdkMemoryDepth              depth);
GdkShaderOp             gdk_memory_format_get_default_shader_op
                                                            (GdkMemoryFormat             format);
gsize                   gdk_memory_format_get_shader_plane  (GdkMemoryFormat             format,
                                                             gsize                       plane,
                                                             gsize                      *width_subsample,
                                                             gsize                      *height_subsample,
                                                             gsize                      *bpp);
gboolean                gdk_memory_format_gl_format         (GdkMemoryFormat             format,
                                                             gsize                       plane,
                                                             gboolean                    gles,
                                                             GLint                      *out_internal_format,
                                                             GLint                      *out_internal_srgb_format,
                                                             GLenum                     *out_format,
                                                             GLenum                     *out_type,
                                                             GdkSwizzle                 *out_swizzle) G_GNUC_WARN_UNUSED_RESULT;
#ifdef GDK_RENDERING_VULKAN
VkFormat                gdk_memory_format_vk_format         (GdkMemoryFormat             format,
                                                             VkComponentMapping         *out_swizzle,
                                                             gboolean                   *needs_ycbcr_conversion);
VkFormat                gdk_memory_format_vk_srgb_format    (GdkMemoryFormat             format);
#endif
gboolean                gdk_memory_format_find_by_dmabuf_fourcc     (guint32                     fourcc,
                                                                     gboolean                    premultiplied,
                                                                     GdkMemoryFormat            *out_format,
                                                                     gboolean                   *out_is_yuv);
guint32                 gdk_memory_format_get_dmabuf_rgb_fourcc     (GdkMemoryFormat             format);
guint32                 gdk_memory_format_get_dmabuf_yuv_fourcc     (GdkMemoryFormat             format);
guint32                 gdk_memory_format_get_dmabuf_shader_fourcc  (GdkMemoryFormat             format,
                                                                     gsize                       plane);

gboolean                gdk_memory_format_find_by_dxgi_format       (DXGI_FORMAT                 format,
                                                                     gboolean                    premultiplied,
                                                                     GdkMemoryFormat            *out_format);
DXGI_FORMAT             gdk_memory_format_get_dxgi_format           (GdkMemoryFormat             format,
                                                                     guint                      *out_shader_4_component_mapping);
DXGI_FORMAT             gdk_memory_format_get_dxgi_srgb_format      (GdkMemoryFormat             format);
guint32                 gdk_memory_format_get_dmabuf_fourcc (GdkMemoryFormat             format);
const char *            gdk_memory_format_get_name          (GdkMemoryFormat             format);

void                    gdk_memory_convert                  (guchar                     *dest_data,
                                                             const GdkMemoryLayout      *dest_layout,
                                                             GdkColorState              *dest_cs,
                                                             const guchar               *src_data,
                                                             const GdkMemoryLayout      *src_layout,
                                                             GdkColorState              *src_cs);
void                    gdk_memory_convert_color_state      (guchar                     *data,
                                                             const GdkMemoryLayout      *layout,
                                                             GdkColorState              *src_color_state,
                                                             GdkColorState              *dest_color_state);
void                    gdk_memory_mipmap                   (guchar                     *dest,
                                                             const GdkMemoryLayout      *dest_layout,
                                                             const guchar               *src,
                                                             const GdkMemoryLayout      *src_layout,
                                                             guint                       lod_level,
                                                             gboolean                    linear);


G_END_DECLS

