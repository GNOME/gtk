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

#include "config.h"

#include "gdkmemoryformatprivate.h"

#include "gdkdmabuffourccprivate.h"
#include "gdkcolorstateprivate.h"
#include "gdkparalleltaskprivate.h"
#include "gtk/gtkcolorutilsprivate.h"
#include "gdkprofilerprivate.h"

#include "gsk/gl/fp16private.h"

#include <epoxy/gl.h>

/* Don't report quick (< 0.5 msec) runs */
#define MIN_MARK_DURATION 500000

#define ADD_MARK(before,name,fmt,...) \
  if (GDK_PROFILER_IS_RUNNING) \
    { \
      gint64 duration = GDK_PROFILER_CURRENT_TIME - before; \
      if (duration > MIN_MARK_DURATION) \
        gdk_profiler_add_markf (before, duration, name, fmt, __VA_ARGS__); \
    }

G_STATIC_ASSERT ((1 << GDK_MEMORY_DEPTH_BITS) > GDK_N_DEPTHS);

typedef struct _GdkMemoryFormatDescription GdkMemoryFormatDescription;

#define TYPED_FUNCS(name, T, R, G, B, A, bpp, scale) \
static void \
name ## _to_float (float        (*dest)[4], \
                   const guchar  *src_data, \
                   gsize          n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *src = (T *) (src_data + i * bpp); \
      dest[i][0] = (float) src[R] / scale; \
      dest[i][1] = (float) src[G] / scale; \
      dest[i][2] = (float) src[B] / scale; \
      if (A >= 0) dest[i][3] = (float) src[A] / scale; else dest[i][3] = 1.0; \
    } \
} \
\
static void \
name ## _from_float (guchar       *dest_data, \
                     const float (*src)[4], \
                     gsize         n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *dest = (T *) (dest_data + i * bpp); \
      dest[R] = CLAMP (src[i][0] * scale + 0.5, 0, scale); \
      dest[G] = CLAMP (src[i][1] * scale + 0.5, 0, scale); \
      dest[B] = CLAMP (src[i][2] * scale + 0.5, 0, scale); \
      if (A >= 0) dest[A] = CLAMP (src[i][3] * scale + 0.5, 0, scale); \
    } \
}

#define TYPED_GRAY_FUNCS(name, T, G, A, bpp, scale) \
static void \
name ## _to_float (float        (*dest)[4], \
                   const guchar  *src_data, \
                   gsize          n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *src = (T *) (src_data + i * bpp); \
      if (A >= 0) dest[i][3] = (float) src[A] / scale; else dest[i][3] = 1.0; \
      if (G >= 0) dest[i][0] = (float) src[G] / scale; else dest[i][0] = dest[i][3]; \
      dest[i][1] = dest[i][2] = dest[i][0]; \
    } \
} \
\
static void \
name ## _from_float (guchar       *dest_data, \
                     const float (*src)[4], \
                     gsize         n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *dest = (T *) (dest_data + i * bpp); \
      if (G >= 0) dest[G] = CLAMP ((src[i][0] + src[i][1] + src[i][2]) * scale / 3.f + 0.5, 0, scale); \
      if (A >= 0) dest[A] = CLAMP (src[i][3] * scale + 0.5, 0, scale); \
    } \
}

TYPED_FUNCS (b8g8r8a8_premultiplied, guchar, 2, 1, 0, 3, 4, 255)
TYPED_FUNCS (a8r8g8b8_premultiplied, guchar, 1, 2, 3, 0, 4, 255)
TYPED_FUNCS (r8g8b8a8_premultiplied, guchar, 0, 1, 2, 3, 4, 255)
TYPED_FUNCS (a8b8g8r8_premultiplied, guchar, 3, 2, 1, 0, 4, 255)
TYPED_FUNCS (b8g8r8a8, guchar, 2, 1, 0, 3, 4, 255)
TYPED_FUNCS (a8r8g8b8, guchar, 1, 2, 3, 0, 4, 255)
TYPED_FUNCS (r8g8b8a8, guchar, 0, 1, 2, 3, 4, 255)
TYPED_FUNCS (a8b8g8r8, guchar, 3, 2, 1, 0, 4, 255)

TYPED_FUNCS (r8g8b8x8, guchar, 0, 1, 2, -1, 4, 255)
TYPED_FUNCS (x8r8g8b8, guchar, 1, 2, 3, -1, 4, 255)
TYPED_FUNCS (b8g8r8x8, guchar, 2, 1, 0, -1, 4, 255)
TYPED_FUNCS (x8b8g8r8, guchar, 3, 2, 1, -1, 4, 255)

TYPED_FUNCS (r8g8b8, guchar, 0, 1, 2, -1, 3, 255)
TYPED_FUNCS (b8g8r8, guchar, 2, 1, 0, -1, 3, 255)
TYPED_FUNCS (r16g16b16, guint16, 0, 1, 2, -1, 6, 65535)
TYPED_FUNCS (r16g16b16a16, guint16, 0, 1, 2, 3, 8, 65535)

TYPED_GRAY_FUNCS (g8a8_premultiplied, guchar, 0, 1, 2, 255)
TYPED_GRAY_FUNCS (g8a8, guchar, 0, 1, 2, 255)
TYPED_GRAY_FUNCS (g8, guchar, 0, -1, 1, 255)
TYPED_GRAY_FUNCS (a8, guchar, -1, 0, 1, 255)
TYPED_GRAY_FUNCS (g16a16_premultiplied, guint16, 0, 1, 4, 65535)
TYPED_GRAY_FUNCS (g16a16, guint16, 0, 1, 4, 65535)
TYPED_GRAY_FUNCS (g16, guint16, 0, -1, 2, 65535)
TYPED_GRAY_FUNCS (a16, guint16, -1, 0, 2, 65535)

static void
r16g16b16_float_to_float (float        (*dest)[4],
                          const guchar  *src_data,
                          gsize          n)
{
  guint16 *src = (guint16 *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      half_to_float (src, dest[i], 3);
      dest[i][3] = 1.0;
      src += 3;
    }
}

static void
r16g16b16_float_from_float (guchar       *dest_data,
                            const float (*src)[4],
                            gsize         n)
{
  guint16 *dest = (guint16 *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      float_to_half (src[i], dest, 3);
      dest += 3;
    }
}

static void
r16g16b16a16_float_to_float (float        (*dest)[4],
                             const guchar  *src,
                             gsize          n)
{
  half_to_float ((const guint16 *) src, (float *) dest, 4 * n);
}

static void
r16g16b16a16_float_from_float (guchar       *dest,
                               const float (*src)[4],
                               gsize         n)
{
  float_to_half ((const float *) src, (guint16 *) dest, 4 * n);
}

static void
a16_float_to_float (float        (*dest)[4],
                    const guchar  *src_data,
                    gsize          n)
{
  const guint16 *src = (const guint16 *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      half_to_float (src, &dest[i][0], 1);
      dest[i][1] = dest[i][0];
      dest[i][2] = dest[i][0];
      dest[i][3] = dest[i][0];
      src++;
    }
}

static void
a16_float_from_float (guchar       *dest_data,
                      const float (*src)[4],
                      gsize         n)
{
  guint16 *dest = (guint16 *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      float_to_half (&src[i][3], dest, 1);
      dest ++;
    }
}

static void
r32g32b32_float_to_float (float        (*dest)[4],
                          const guchar  *src_data,
                          gsize          n)
{
  const float (*src)[3] = (const float (*)[3]) src_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[i][0] = src[i][0];
      dest[i][1] = src[i][1];
      dest[i][2] = src[i][2];
      dest[i][3] = 1.0;
    }
}

static void
r32g32b32_float_from_float (guchar       *dest_data,
                            const float (*src)[4],
                            gsize         n)
{
  float (*dest)[3] = (float (*)[3]) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[i][0] = src[i][0];
      dest[i][1] = src[i][1];
      dest[i][2] = src[i][2];
    }
}

static void
r32g32b32a32_float_to_float (float        (*dest)[4],
                             const guchar  *src,
                             gsize          n)
{
  memcpy (dest, src, sizeof (float) * n * 4);
}

static void
r32g32b32a32_float_from_float (guchar       *dest,
                               const float (*src)[4],
                               gsize         n)
{
  memcpy (dest, src, sizeof (float) * n * 4);
}

static void
a32_float_to_float (float        (*dest)[4],
                    const guchar  *src_data,
                    gsize          n)
{
  const float *src = (const float *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[i][0] = src[i];
      dest[i][1] = src[i];
      dest[i][2] = src[i];
      dest[i][3] = src[i];
    }
}

static void
a32_float_from_float (guchar       *dest_data,
                      const float (*src)[4],
                      gsize         n)
{
  float *dest = (float *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[i] = src[i][3];
    }
}

#define PREMULTIPLY_FUNC(name, R1, G1, B1, A1, R2, G2, B2, A2) \
static void \
name (guchar *dest, \
      const guchar *src, \
      gsize n) \
{ \
  for (; n > 0; n--) \
    { \
      guchar a = src[A1]; \
      guint16 r = (guint16)src[R1] * a + 127; \
      guint16 g = (guint16)src[G1] * a + 127; \
      guint16 b = (guint16)src[B1] * a + 127; \
      dest[R2] = (r + (r >> 8) + 1) >> 8; \
      dest[G2] = (g + (g >> 8) + 1) >> 8; \
      dest[B2] = (b + (b >> 8) + 1) >> 8; \
      dest[A2] = a; \
      dest += 4; \
      src += 4; \
    } \
}

PREMULTIPLY_FUNC(r8g8b8a8_to_r8g8b8a8_premultiplied, 0, 1, 2, 3, 0, 1, 2, 3)
PREMULTIPLY_FUNC(r8g8b8a8_to_b8g8r8a8_premultiplied, 0, 1, 2, 3, 2, 1, 0, 3)
PREMULTIPLY_FUNC(r8g8b8a8_to_a8r8g8b8_premultiplied, 0, 1, 2, 3, 1, 2, 3, 0)
PREMULTIPLY_FUNC(r8g8b8a8_to_a8b8g8r8_premultiplied, 0, 1, 2, 3, 3, 2, 1, 0)

#define ADD_ALPHA_FUNC(name, R1, G1, B1, R2, G2, B2, A2) \
static void \
name (guchar *dest, \
      const guchar *src, \
      gsize n) \
{ \
  for (; n > 0; n--) \
    { \
      dest[R2] = src[R1]; \
      dest[G2] = src[G1]; \
      dest[B2] = src[B1]; \
      dest[A2] = 255; \
      dest += 4; \
      src += 3; \
    } \
}

ADD_ALPHA_FUNC(r8g8b8_to_r8g8b8a8, 0, 1, 2, 0, 1, 2, 3)
ADD_ALPHA_FUNC(r8g8b8_to_b8g8r8a8, 0, 1, 2, 2, 1, 0, 3)
ADD_ALPHA_FUNC(r8g8b8_to_a8r8g8b8, 0, 1, 2, 1, 2, 3, 0)
ADD_ALPHA_FUNC(r8g8b8_to_a8b8g8r8, 0, 1, 2, 3, 2, 1, 0)

#define SWAP_FUNC(name, R, G, B, A) \
static void \
name (guchar *dest, \
      const guchar *src, \
      gsize n) \
{ \
  for (; n > 0; n--) \
    { \
      dest[0] = src[R]; \
      dest[1] = src[G]; \
      dest[2] = src[B]; \
      dest[3] = src[A]; \
      dest += 4; \
      src += 4; \
    } \
}

SWAP_FUNC(r8g8b8a8_to_b8g8r8a8, 2, 1, 0, 3)
SWAP_FUNC(b8g8r8a8_to_r8g8b8a8, 2, 1, 0, 3)

#define MIPMAP_FUNC(SumType, DataType, n_units) \
static void \
gdk_mipmap_ ## DataType ## _ ## n_units ## _nearest (guchar       *dest, \
                                                     gsize         dest_stride, \
                                                     const guchar *src, \
                                                     gsize         src_stride, \
                                                     gsize         src_width, \
                                                     gsize         src_height, \
                                                     guint         lod_level) \
{ \
  gsize y, x, i; \
  gsize n = 1 << lod_level; \
\
  for (y = 0; y < src_height; y += n) \
    { \
      DataType *dest_data = (DataType *) dest; \
      for (x = 0; x < src_width; x += n) \
        { \
          const DataType *src_data = (const DataType *) (src + (y + MIN (n / 2, src_height - y))  * src_stride); \
\
          for (i = 0; i < n_units; i++) \
            *dest_data++ = src_data[n_units * (x + MIN (n / 2, src_width - n_units)) + i]; \
        } \
      dest += dest_stride; \
      src += src_stride * n; \
    } \
} \
\
static void \
gdk_mipmap_ ## DataType ## _ ## n_units ## _linear (guchar       *dest, \
                                                    gsize         dest_stride, \
                                                    const guchar *src, \
                                                    gsize         src_stride, \
                                                    gsize         src_width, \
                                                    gsize         src_height, \
                                                    guint         lod_level) \
{ \
  gsize y_dest, y, x_dest, x, i; \
  gsize n = 1 << lod_level; \
\
  for (y_dest = 0; y_dest < src_height; y_dest += n) \
    { \
      DataType *dest_data = (DataType *) dest; \
      for (x_dest = 0; x_dest < src_width; x_dest += n) \
        { \
          SumType tmp[n_units] = { 0, }; \
\
          for (y = 0; y < MIN (n, src_height - y_dest); y++) \
            { \
              const DataType *src_data = (const DataType *) (src + y * src_stride); \
              for (x = 0; x < MIN (n, src_width - x_dest); x++) \
                { \
                  for (i = 0; i < n_units; i++) \
                    tmp[i] += src_data[n_units * (x_dest + x) + i]; \
                } \
            } \
\
          for (i = 0; i < n_units; i++) \
            *dest_data++ = tmp[i] / (x * y); \
        } \
      dest += dest_stride; \
      src += src_stride * n; \
    } \
}

MIPMAP_FUNC(guint32, guint8, 1)
MIPMAP_FUNC(guint32, guint8, 2)
MIPMAP_FUNC(guint32, guint8, 3)
MIPMAP_FUNC(guint32, guint8, 4)
MIPMAP_FUNC(guint32, guint16, 1)
MIPMAP_FUNC(guint32, guint16, 2)
MIPMAP_FUNC(guint32, guint16, 3)
MIPMAP_FUNC(guint32, guint16, 4)
MIPMAP_FUNC(float, float, 1)
MIPMAP_FUNC(float, float, 3)
MIPMAP_FUNC(float, float, 4)
#define half_float guint16
MIPMAP_FUNC(float, half_float, 1)
MIPMAP_FUNC(float, half_float, 3)
MIPMAP_FUNC(float, half_float, 4)
#undef half_float

struct _GdkMemoryFormatDescription
{
  const char *name;
  GdkMemoryAlpha alpha;
  GdkMemoryFormat premultiplied;
  GdkMemoryFormat straight;
  gsize bytes_per_pixel;
  gsize alignment;
  GdkMemoryDepth depth;
  const GdkMemoryFormat *fallbacks;
  struct {
    GLint internal_gl_format;
    GLint internal_gles_format;
    GLint internal_srgb_format;
    GLenum format;
    GLenum srgb_format;
    GLenum type;
    GLint swizzle[4];
    /* -1 if none exists, ie the format is already RGBA
     * or the format doesn't have 4 channels */
    GdkMemoryFormat rgba_format;
    GLint rgba_swizzle[4];
  } gl;
#ifdef GDK_RENDERING_VULKAN
  VkFormat vk_format;
  VkFormat vk_srgb_format;
#endif
#ifdef HAVE_DMABUF
  guint32 dmabuf_fourcc;
#endif
  /* no premultiplication going on here */
  void (* to_float) (float (*)[4], const guchar*, gsize);
  void (* from_float) (guchar *, const float (*)[4], gsize);
  void (* mipmap_nearest) (guchar *, gsize, const guchar *, gsize, gsize, gsize, guint);
  void (* mipmap_linear) (guchar *, gsize, const guchar *, gsize, gsize, gsize, guint);
};

#if  G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GDK_GL_UNSIGNED_BYTE_FLIPPED GL_UNSIGNED_INT_8_8_8_8
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#  define GDK_GL_UNSIGNED_BYTE_FLIPPED GL_UNSIGNED_INT_8_8_8_8_REV
#else
#  error "Define the right GL flags here"
#endif

static const GdkMemoryFormatDescription memory_formats[] = {
  [GDK_MEMORY_B8G8R8A8_PREMULTIPLIED] = {
    .name = "BGRA8(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_B8G8R8A8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_BGRA,
        .internal_srgb_format = -1,
        .format = GL_BGRA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .rgba_swizzle = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ARGB8888,
#endif
    .to_float = b8g8r8a8_premultiplied_to_float,
    .from_float = b8g8r8a8_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8R8G8B8_PREMULTIPLIED] = {
    .name = "ARGB8(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8R8G8B8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_BGRA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .rgba_swizzle = { GL_GREEN, GL_BLUE, GL_ALPHA, GL_RED },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_BGRA8888,
#endif
    .to_float = a8r8g8b8_premultiplied_to_float,
    .from_float = a8r8g8b8_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] = {
    .name = "RGBA8(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_R8G8B8A8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR8888,
#endif
    .to_float = r8g8b8a8_premultiplied_to_float,
    .from_float = r8g8b8a8_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8B8G8R8_PREMULTIPLIED] = {
    .name = "ABGR8(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8B8G8R8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .rgba_swizzle = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_RGBA8888,
#endif
    .to_float = a8b8g8r8_premultiplied_to_float,
    .from_float = a8b8g8r8_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_B8G8R8A8] = {
    .name = "BGRA8",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_B8G8R8A8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_BGRA,
        .internal_srgb_format = -1,
        .format = GL_BGRA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8,
        .rgba_swizzle = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ARGB8888,
#endif
    .to_float = b8g8r8a8_to_float,
    .from_float = b8g8r8a8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8R8G8B8] = {
    .name = "ARGB8",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8R8G8B8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_BGRA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8,
        .rgba_swizzle = { GL_GREEN, GL_BLUE, GL_ALPHA, GL_RED },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_BGRA8888,
#endif
    .to_float = a8r8g8b8_to_float,
    .from_float = a8r8g8b8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8A8] = {
    .name = "RGBA8",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_R8G8B8A8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR8888,
#endif
    .to_float = r8g8b8a8_to_float,
    .from_float = r8g8b8a8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8B8G8R8] = {
    .name = "ABGR8",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8B8G8R8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8A8,
        .rgba_swizzle = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_RED },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_RGBA8888,
#endif
    .to_float = a8b8g8r8_to_float,
    .from_float = a8b8g8r8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_B8G8R8X8] = {
    .name = "BGRX8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_B8G8R8X8,
    .straight = GDK_MEMORY_B8G8R8X8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_BGRA,
        .internal_srgb_format = -1,
        .format = GL_BGRA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE },
        .rgba_format = GDK_MEMORY_R8G8B8X8,
        .rgba_swizzle = { GL_BLUE, GL_GREEN, GL_RED, GL_ONE },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_XRGB8888,
#endif
    .to_float = b8g8r8x8_to_float,
    .from_float = b8g8r8x8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_X8R8G8B8] = {
    .name = "XRGB8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X8R8G8B8,
    .straight = GDK_MEMORY_X8R8G8B8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_BGRA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE },
        .rgba_format = GDK_MEMORY_R8G8B8A8,
        .rgba_swizzle = { GL_GREEN, GL_BLUE, GL_ALPHA, GL_ONE },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_BGRX8888,
#endif
    .to_float = x8r8g8b8_to_float,
    .from_float = x8r8g8b8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8X8] = {
    .name = "RGBX8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R8G8B8X8,
    .straight = GDK_MEMORY_R8G8B8X8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
    .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_XBGR8888,
#endif
    .to_float = r8g8b8x8_to_float,
    .from_float = r8g8b8x8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_X8B8G8R8] = {
    .name = "XBGR8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X8B8G8R8,
    .straight = GDK_MEMORY_X8B8G8R8,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA8,
        .internal_gles_format = GL_RGBA8,
        .internal_srgb_format = GL_SRGB8_ALPHA8,
        .format = GL_RGBA,
        .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ONE },
        .rgba_format = GDK_MEMORY_R8G8B8A8,
        .rgba_swizzle = { GL_ALPHA, GL_BLUE, GL_GREEN, GL_ONE },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_UNDEFINED,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_RGBX8888,
#endif
    .to_float = x8b8g8r8_to_float,
    .from_float = x8b8g8r8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8] = {
    .name = "RGB8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R8G8B8,
    .straight = GDK_MEMORY_R8G8B8,
    .bytes_per_pixel = 3,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGB8,
        .internal_gles_format = GL_RGB8,
        .internal_srgb_format = GL_SRGB8,
        .format = GL_RGB,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8B8_UNORM,
    .vk_srgb_format = VK_FORMAT_R8G8B8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_BGR888,
#endif
    .to_float = r8g8b8_to_float,
    .from_float = r8g8b8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_3_nearest,
    .mipmap_linear = gdk_mipmap_guint8_3_linear,
  },
  [GDK_MEMORY_B8G8R8] = {
    .name = "BGR8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_B8G8R8,
    .straight = GDK_MEMORY_B8G8R8,
    .bytes_per_pixel = 3,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGB8,
        .internal_gles_format = GL_RGB8,
        .internal_srgb_format = GL_SRGB8,
        .format = GL_BGR,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = GDK_MEMORY_R8G8B8,
        .rgba_swizzle = { GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA },
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_B8G8R8_UNORM,
    .vk_srgb_format = VK_FORMAT_B8G8R8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_RGB888,
#endif
    .to_float = b8g8r8_to_float,
    .from_float = b8g8r8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_3_nearest,
    .mipmap_linear = gdk_mipmap_guint8_3_linear,
  },
  [GDK_MEMORY_R16G16B16] = {
    .name = "RGB16",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R16G16B16,
    .straight = GDK_MEMORY_R16G16B16,
    .bytes_per_pixel = 6,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGB16,
        .internal_gles_format = GL_RGB16,
        .internal_srgb_format = -1,
        .format = GL_RGB,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = r16g16b16_to_float,
    .from_float = r16g16b16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_3_nearest,
    .mipmap_linear = gdk_mipmap_guint16_3_linear,
  },
  [GDK_MEMORY_R16G16B16A16_PREMULTIPLIED] = {
    .name = "RGBA16(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16,
    .bytes_per_pixel = 8,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA16,
        .internal_gles_format = GL_RGBA16,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16A16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR16161616,
#endif
    .to_float = r16g16b16a16_to_float,
    .from_float = r16g16b16a16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_4_nearest,
    .mipmap_linear = gdk_mipmap_guint16_4_linear,
  },
  [GDK_MEMORY_R16G16B16A16] = {
    .name = "RGBA16",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16,
    .bytes_per_pixel = 8,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA16,
        .internal_gles_format = GL_RGBA16,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16A16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR16161616,
#endif
    .to_float = r16g16b16a16_to_float,
    .from_float = r16g16b16a16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_4_nearest,
    .mipmap_linear = gdk_mipmap_guint16_4_linear,
  },
  [GDK_MEMORY_R16G16B16_FLOAT] = {
    .name = "RGBA16f",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R16G16B16_FLOAT,
    .straight = GDK_MEMORY_R16G16B16_FLOAT,
    .bytes_per_pixel = 6,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGB16F,
        .internal_gles_format = GL_RGB16F,
        .internal_srgb_format = -1,
        .format = GL_RGB,
        .type = GL_HALF_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = r16g16b16_float_to_float,
    .from_float = r16g16b16_float_from_float,
    .mipmap_nearest = gdk_mipmap_half_float_3_nearest,
    .mipmap_linear = gdk_mipmap_half_float_3_linear,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] = {
    .name = "RGBA16f(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16_FLOAT,
    .bytes_per_pixel = 8,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA16F,
        .internal_gles_format = GL_RGBA16F,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_HALF_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16A16_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR16161616F,
#endif
    .to_float = r16g16b16a16_float_to_float,
    .from_float = r16g16b16a16_float_from_float,
    .mipmap_nearest = gdk_mipmap_half_float_4_nearest,
    .mipmap_linear = gdk_mipmap_half_float_4_linear,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT] = {
    .name = "RGBA16f",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16_FLOAT,
    .bytes_per_pixel = 8,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA16F,
        .internal_gles_format = GL_RGBA16F,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_HALF_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16B16A16_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_ABGR16161616F,
#endif
    .to_float = r16g16b16a16_float_to_float,
    .from_float = r16g16b16a16_float_from_float,
    .mipmap_nearest = gdk_mipmap_half_float_4_nearest,
    .mipmap_linear = gdk_mipmap_half_float_4_linear,
  },
  [GDK_MEMORY_R32G32B32_FLOAT] = {
    .name = "RGB32f",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R32G32B32_FLOAT,
    .straight = GDK_MEMORY_R32G32B32_FLOAT,
    12,
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGB32F,
        .internal_gles_format = GL_RGB32F,
        .internal_srgb_format = -1,
        .format = GL_RGB,
        .type = GL_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R32G32B32_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = r32g32b32_float_to_float,
    .from_float = r32g32b32_float_from_float,
    .mipmap_nearest = gdk_mipmap_float_3_nearest,
    .mipmap_linear = gdk_mipmap_float_3_linear,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] = {
    .name = "RGBA32f(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R32G32B32A32_FLOAT,
    16,
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA32F,
        .internal_gles_format = GL_RGBA32F,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R32G32B32A32_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = r32g32b32a32_float_to_float,
    .from_float = r32g32b32a32_float_from_float,
    .mipmap_nearest = gdk_mipmap_float_4_nearest,
    .mipmap_linear = gdk_mipmap_float_4_linear,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT] = {
    .name = "RGBA32f",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R32G32B32A32_FLOAT,
    16,
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RGBA32F,
        .internal_gles_format = GL_RGBA32F,
        .internal_srgb_format = -1,
        .format = GL_RGBA,
        .type = GL_FLOAT,
        .swizzle = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R32G32B32A32_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = r32g32b32a32_float_to_float,
    .from_float = r32g32b32a32_float_from_float,
    .mipmap_nearest = gdk_mipmap_float_4_nearest,
    .mipmap_linear = gdk_mipmap_float_4_linear,
  },
  [GDK_MEMORY_G8A8_PREMULTIPLIED] = {
    .name = "GA8(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_G8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_G8A8,
    .bytes_per_pixel = 2,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RG8,
        .internal_gles_format = GL_RG8,
        .internal_srgb_format = -1,
        .format = GL_RG,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_GREEN },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = g8a8_premultiplied_to_float,
    .from_float = g8a8_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_2_nearest,
    .mipmap_linear = gdk_mipmap_guint8_2_linear,
  },
  [GDK_MEMORY_G8A8] = {
    .name = "GA8",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_G8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_G8A8,
    .bytes_per_pixel = 2,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RG8,
        .internal_gles_format = GL_RG8,
        .internal_srgb_format = -1,
        .format = GL_RG,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_GREEN },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8G8_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = g8a8_to_float,
    .from_float = g8a8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_2_nearest,
    .mipmap_linear = gdk_mipmap_guint8_2_linear,
  },
  [GDK_MEMORY_G8] = {
    .name = "G8",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8,
    .straight = GDK_MEMORY_G8,
    .bytes_per_pixel = 1,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R8,
        .internal_gles_format = GL_R8,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_ONE },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8_UNORM,
    .vk_srgb_format = VK_FORMAT_R8_SRGB,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_R8,
#endif
    .to_float = g8_to_float,
    .from_float = g8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_1_nearest,
    .mipmap_linear = gdk_mipmap_guint8_1_linear,
  },
  [GDK_MEMORY_G16A16_PREMULTIPLIED] = {
    .name = "GA16(p)",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_G16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_G16A16,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RG16,
        .internal_gles_format = GL_RG16,
        .internal_srgb_format = -1,
        .format = GL_RG,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_GREEN },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = g16a16_premultiplied_to_float,
    .from_float = g16a16_premultiplied_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_2_nearest,
    .mipmap_linear = gdk_mipmap_guint16_2_linear,
  },
  [GDK_MEMORY_G16A16] = {
    .name = "GA16",
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_G16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_G16A16,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16,
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_RG16,
        .internal_gles_format = GL_RG16,
        .internal_srgb_format = -1,
        .format = GL_RG,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_GREEN },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16G16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = g16a16_to_float,
    .from_float = g16a16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_2_nearest,
    .mipmap_linear = gdk_mipmap_guint16_2_linear,
  },
  [GDK_MEMORY_G16] = {
    .name = "G16",
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16,
    .straight = GDK_MEMORY_G16,
    .bytes_per_pixel = 2,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R16,
        .internal_gles_format = GL_R16,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_ONE },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = DRM_FORMAT_R16,
#endif
    .to_float = g16_to_float,
    .from_float = g16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_1_nearest,
    .mipmap_linear = gdk_mipmap_guint16_1_linear,
  },
  [GDK_MEMORY_A8] = {
    .name = "A8",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8,
    .straight = GDK_MEMORY_A8,
    .bytes_per_pixel = 1,
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R8,
        .internal_gles_format = GL_R8,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_UNSIGNED_BYTE,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_RED },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R8_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = a8_to_float,
    .from_float = a8_from_float,
    .mipmap_nearest = gdk_mipmap_guint8_1_nearest,
    .mipmap_linear = gdk_mipmap_guint8_1_linear,
  },
  [GDK_MEMORY_A16] = {
    .name = "A16",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A16,
    .straight = GDK_MEMORY_A16,
    .bytes_per_pixel = 2,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R16,
        .internal_gles_format = GL_R16,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_UNSIGNED_SHORT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_RED },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16_UNORM,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = a16_to_float,
    .from_float = a16_from_float,
    .mipmap_nearest = gdk_mipmap_guint16_1_nearest,
    .mipmap_linear = gdk_mipmap_guint16_1_linear,
  },
  [GDK_MEMORY_A16_FLOAT] = {
    .name = "A16f",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A16_FLOAT,
    .straight = GDK_MEMORY_A16_FLOAT,
    .bytes_per_pixel = 2,
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R16F,
        .internal_gles_format = GL_R16F,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_HALF_FLOAT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_RED },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R16_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = a16_float_to_float,
    .from_float = a16_float_from_float,
    .mipmap_nearest = gdk_mipmap_half_float_1_nearest,
    .mipmap_linear = gdk_mipmap_half_float_1_linear,
  },
  [GDK_MEMORY_A32_FLOAT] = {
    .name = "A32f",
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A32_FLOAT,
    .straight = GDK_MEMORY_A32_FLOAT,
    .bytes_per_pixel = 4,
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .gl = {
        .internal_gl_format = GL_R32F,
        .internal_gles_format = GL_R32F,
        .internal_srgb_format = -1,
        .format = GL_RED,
        .type = GL_FLOAT,
        .swizzle = { GL_RED, GL_RED, GL_RED, GL_RED },
        .rgba_format = -1,
    },
#ifdef GDK_RENDERING_VULKAN
    .vk_format = VK_FORMAT_R32_SFLOAT,
    .vk_srgb_format = VK_FORMAT_UNDEFINED,
#endif
#ifdef HAVE_DMABUF
    .dmabuf_fourcc = 0,
#endif
    .to_float = a32_float_to_float,
    .from_float = a32_float_from_float,
    .mipmap_nearest = gdk_mipmap_float_1_nearest,
    .mipmap_linear = gdk_mipmap_float_1_linear,
  }
};

/* if this fails, somebody forgot to add formats above */
G_STATIC_ASSERT (G_N_ELEMENTS (memory_formats) == GDK_MEMORY_N_FORMATS);

gsize
gdk_memory_format_bytes_per_pixel (GdkMemoryFormat format)
{
  return memory_formats[format].bytes_per_pixel;
}

GdkMemoryAlpha
gdk_memory_format_alpha (GdkMemoryFormat format)
{
  return memory_formats[format].alpha;
}

GdkMemoryFormat
gdk_memory_format_get_premultiplied (GdkMemoryFormat format)
{
  return memory_formats[format].premultiplied;
}

GdkMemoryFormat
gdk_memory_format_get_straight (GdkMemoryFormat format)
{
  return memory_formats[format].straight;
}

gsize
gdk_memory_format_alignment (GdkMemoryFormat format)
{
  return memory_formats[format].alignment;
}

/*
 * gdk_memory_format_get_fallbacks:
 * @format: a format
 *
 * Gets a list of fallback formats to use for @format.
 *
 * These formats are RGBA formats that ideally have a higher depth
 * than the given format. They will always include a guaranteed
 * supported format though, even if it is of lower quality (unless
 * @format is already guaranteed supported).
 *
 * Fallbacks will use the same alpha format, ie a premultiplied
 * format will never fall back to a straight alpha format and
 * vice versa. Either may fall back to an opaque format. Opaque
 * formats will fall back to premultiplied formats only.
 *
 * Use gdk_memory_format_get_premultiplied() and
 * gdk_memory_format_get_straight() to transition between
 * premultiplied and straight alpha if you need to.
 *
 * Use gdk_memory_format_gl_rgba_format() to get an equivalent RGBA
 * format and swizzle.
 *
 * The expected order of operation when looking for supported formats
 * is the following:
 *
 * 1. Try the format itself
 * 2. If swizzling is supported, try the RGBA format with swizzling
 * 3. If swizzling is not supported, try the RGBA without swizzling,
 *    and with CPU conversion
 * 4. Try fallback formats
 * 
 * Returns: A list of fallbacks, terminated with -1
 **/
const GdkMemoryFormat *
gdk_memory_format_get_fallbacks (GdkMemoryFormat format)
{
  return memory_formats[format].fallbacks;
}

gsize
gdk_memory_format_min_buffer_size (GdkMemoryFormat format,
                                   gsize           stride,
                                   gsize           width,
                                   gsize           height)
{
  return stride * (height - 1) + width * gdk_memory_format_bytes_per_pixel (format);
}

/*<private>
 * gdk_memory_format_get_depth:
 * @format: a memory format
 * @srgb: whether an SRGB depth should be returned.
 *
 * Gets the depth of the individual channels of the format.
 * See gsk_render_node_prefers_high_depth() for more
 * information on this.
 *
 * Usually renderers want to use higher depth for render
 * targets to match these formats.
 *
 * Returns: The depth of this format
 **/
GdkMemoryDepth
gdk_memory_format_get_depth (GdkMemoryFormat format,
                             gboolean        srgb)
{
  GdkMemoryDepth depth;

  depth = memory_formats[format].depth;
  if (depth == GDK_MEMORY_U8 && srgb)
    depth = GDK_MEMORY_U8_SRGB;

  return depth;
}

const char *
gdk_memory_depth_get_name (GdkMemoryDepth depth)
{
  const char *names[] = { "none", "u8", "u8-srgb", "u16", "f16", "f32" };

  return names[depth];
}

/*<private>
 * gdk_memory_depth_merge:
 * @depth1: the first depth
 * @depth2: the second depth
 *
 * Returns a depth that can accommodate both given depths
 * without any loss of precision.
 *
 * Returns: The merged depth
 **/
GdkMemoryDepth
gdk_memory_depth_merge (GdkMemoryDepth depth1,
                        GdkMemoryDepth depth2)
{
  static const GdkMemoryDepth merged_depths[GDK_N_DEPTHS][GDK_N_DEPTHS] = {
                         /*  NONE                U8                  U8_SRGB             U16                 FLOAT16             FLOAT32 */
    [GDK_MEMORY_NONE]    = { GDK_MEMORY_NONE,    GDK_MEMORY_U8,      GDK_MEMORY_U8_SRGB, GDK_MEMORY_U16,     GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT32 },
    [GDK_MEMORY_U8]      = { GDK_MEMORY_U8,      GDK_MEMORY_U8,      GDK_MEMORY_FLOAT16, GDK_MEMORY_U16,     GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT32 },
    [GDK_MEMORY_U8_SRGB] = { GDK_MEMORY_U8_SRGB, GDK_MEMORY_FLOAT16, GDK_MEMORY_U8_SRGB, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT32 },
    [GDK_MEMORY_U16]     = { GDK_MEMORY_U16,     GDK_MEMORY_U16,     GDK_MEMORY_FLOAT32, GDK_MEMORY_U16,     GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32 },
    [GDK_MEMORY_FLOAT16] = { GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT16, GDK_MEMORY_FLOAT32 },
    [GDK_MEMORY_FLOAT32] = { GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32, GDK_MEMORY_FLOAT32 },
  };

  g_assert (depth1 < GDK_N_DEPTHS);
  g_assert (depth2 < GDK_N_DEPTHS);

  return merged_depths[depth1][depth2];
}

/*
 * gdk_memory_depth_get_format:
 * @depth: the depth
 *
 * Gets the preferred format to use for rendering at the
 * given depth
 *
 * Returns: the format
 **/
GdkMemoryFormat
gdk_memory_depth_get_format (GdkMemoryDepth depth)
{
  switch (depth)
    {
      case GDK_MEMORY_NONE:
      case GDK_MEMORY_U8:
      case GDK_MEMORY_U8_SRGB:
        return GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      case GDK_MEMORY_U16:
        return GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
      case GDK_MEMORY_FLOAT16:
        return GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
      case GDK_MEMORY_FLOAT32:
        return GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
      case GDK_N_DEPTHS:
      default:
        g_return_val_if_reached (GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
    }
}

/*
 * gdk_memory_depth_get_alpha_format:
 * @depth: the depth
 *
 * Gets the preferred format to use for rendering masks and other
 * alpha-only content.
 *
 * Returns: the format
 **/
GdkMemoryFormat
gdk_memory_depth_get_alpha_format (GdkMemoryDepth depth)
{
  switch (depth)
    {
      case GDK_MEMORY_NONE:
      case GDK_MEMORY_U8:
      case GDK_MEMORY_U8_SRGB:
        return GDK_MEMORY_A8;
      case GDK_MEMORY_U16:
        return GDK_MEMORY_A16;
      case GDK_MEMORY_FLOAT16:
        return GDK_MEMORY_A16_FLOAT;
      case GDK_MEMORY_FLOAT32:
        return GDK_MEMORY_A32_FLOAT;
      case GDK_N_DEPTHS:
      default:
        g_return_val_if_reached (GDK_MEMORY_A8);
    }
}

gboolean
gdk_memory_depth_is_srgb (GdkMemoryDepth depth)
{
  /* Putting a switch here instead of a simple check
   * so the compiler makes us look here
   * when adding new formats */

  switch (depth)
    {
      case GDK_MEMORY_U8_SRGB:
        return TRUE;

      case GDK_MEMORY_NONE:
      case GDK_MEMORY_U8:
      case GDK_MEMORY_U16:
      case GDK_MEMORY_FLOAT16:
      case GDK_MEMORY_FLOAT32:
        return FALSE;

      case GDK_N_DEPTHS:
      default:
        g_return_val_if_reached (GDK_MEMORY_A8);
    }
}

void
gdk_memory_format_gl_format (GdkMemoryFormat  format,
                             gboolean         gles,
                             GLint           *out_internal_format,
                             GLint           *out_internal_srgb_format,
                             GLenum          *out_format,
                             GLenum          *out_type,
                             GLint            out_swizzle[4])
{
  if (gles)
    *out_internal_format = memory_formats[format].gl.internal_gles_format;
  else
    *out_internal_format = memory_formats[format].gl.internal_gl_format;
  *out_internal_srgb_format = memory_formats[format].gl.internal_srgb_format;
  *out_format = memory_formats[format].gl.format;
  *out_type = memory_formats[format].gl.type;
  memcpy (out_swizzle, memory_formats[format].gl.swizzle, sizeof(GLint) * 4);
}

/*
 * gdk_memory_format_gl_rgba_format:
 * @format: The format to query
 * @gles: TRUE for GLES, FALSE for GL
 * @out_actual_format: The actual RGBA format
 * @out_internal_format: the GL internal format
 * @out_internal_srgb_format: the GL internal format to use for automatic
 *   sRGB<=>linear conversion
 * @out_format: the GL format
 * @out_type: the GL type
 * @out_swizzle: The swizzle to use 
 *
 * Maps the given format to a GL format that uses RGBA and uses swizzling,
 * as opposed to trying to find a GL format that is swapped in the right
 * direction.
 *
 * This format is guaranteed equivalent in memory layout to the original
 * format, so uploading/downloading code can treat them the same.
 *
 * Returns: %TRUE if the format exists and is different from the given format.
 **/
gboolean
gdk_memory_format_gl_rgba_format (GdkMemoryFormat  format,
                                  gboolean         gles,
                                  GdkMemoryFormat *out_actual_format,
                                  GLint           *out_internal_format,
                                  GLint           *out_internal_srgb_format,
                                  GLenum          *out_format,
                                  GLenum          *out_type,
                                  GLint            out_swizzle[4])
{
  GdkMemoryFormat actual = memory_formats[format].gl.rgba_format;

  if (actual == -1)
    return FALSE;

  *out_actual_format = actual;
  if (gles)
    *out_internal_format = memory_formats[actual].gl.internal_gles_format;
  else
    *out_internal_format = memory_formats[actual].gl.internal_gl_format;
  *out_internal_srgb_format = memory_formats[actual].gl.internal_srgb_format;
  *out_format = memory_formats[actual].gl.format;
  *out_type = memory_formats[actual].gl.type;
  memcpy (out_swizzle, memory_formats[format].gl.rgba_swizzle, sizeof(GLint) * 4);

  return TRUE;
}

#ifdef GDK_RENDERING_VULKAN

static VkComponentSwizzle
vk_swizzle_from_gl_swizzle_one (GLint swizzle)
{
  switch (swizzle)
    {
      case GL_RED:
        return VK_COMPONENT_SWIZZLE_R;
      case GL_GREEN:
        return VK_COMPONENT_SWIZZLE_G;
      case GL_BLUE:
        return VK_COMPONENT_SWIZZLE_B;
      case GL_ALPHA:
        return VK_COMPONENT_SWIZZLE_A;
      case GL_ZERO:
        return VK_COMPONENT_SWIZZLE_ZERO;
      case GL_ONE:
        return VK_COMPONENT_SWIZZLE_ONE;
      default:
        g_assert_not_reached ();
        return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
}

static void
vk_swizzle_from_gl_swizzle (VkComponentMapping *vk_swizzle,
                            const GLint         gl_swizzle[4])
{
  vk_swizzle->r = vk_swizzle_from_gl_swizzle_one (gl_swizzle[0]);
  vk_swizzle->g = vk_swizzle_from_gl_swizzle_one (gl_swizzle[1]);
  vk_swizzle->b = vk_swizzle_from_gl_swizzle_one (gl_swizzle[2]);
  vk_swizzle->a = vk_swizzle_from_gl_swizzle_one (gl_swizzle[3]);
}

/* Vulkan version of gdk_memory_format_gl_format()
 * Returns VK_FORMAT_UNDEFINED on failure */
VkFormat
gdk_memory_format_vk_format (GdkMemoryFormat     format,
                             VkComponentMapping *out_swizzle)
{
  if (out_swizzle)
    vk_swizzle_from_gl_swizzle (out_swizzle, memory_formats[format].gl.swizzle);
  return memory_formats[format].vk_format;
}

/* Gets the matching SRGB version of a VkFormat
 * Returns VK_FORMAT_UNDEFINED if none exists */
VkFormat
gdk_memory_format_vk_srgb_format (GdkMemoryFormat format)
{
  return memory_formats[format].vk_srgb_format;
}

/* Vulkan version of gdk_memory_format_gl_rgba_format()
 * Returns VK_FORMAT_UNDEFINED on failure */
VkFormat
gdk_memory_format_vk_rgba_format (GdkMemoryFormat     format,
                                  GdkMemoryFormat    *out_rgba_format,
                                  VkComponentMapping *out_swizzle)
{
  GdkMemoryFormat actual = memory_formats[format].gl.rgba_format;

  if (actual == -1)
    return VK_FORMAT_UNDEFINED;

  if (out_rgba_format)
    *out_rgba_format = actual;
  if (out_swizzle)
    vk_swizzle_from_gl_swizzle (out_swizzle, memory_formats[format].gl.rgba_swizzle);
  return memory_formats[actual].vk_format;
}
#endif

/*
 * gdk_memory_format_get_dmabuf_fourcc:
 * @format: The memory format
 *
 * Gets the dmabuf fourcc for a given memory format.
 *
 * The format is an exact match, so data can be copied between the
 * dmabuf and data of the format. This is different from the
 * memoryformat returned by a GdkDmabufTexture, which is just the
 * closest match.
 *
 * Not all formats have a corresponding dmabuf format.
 * In those cases 0 will be returned.
 *
 * If dmabuf support is not compiled in, always returns 0.
 *
 * Returns: the fourcc or 0
 **/
guint32
gdk_memory_format_get_dmabuf_fourcc (GdkMemoryFormat format)
{
#ifdef HAVE_DMABUF
  return memory_formats[format].dmabuf_fourcc;
#else
  return 0;
#endif
}

const char *
gdk_memory_format_get_name (GdkMemoryFormat format)
{
  return memory_formats[format].name;
}

static void
premultiply (float (*rgba)[4],
             gsize  n)
{
  for (gsize i = 0; i < n; i++)
    {
      rgba[i][0] *= rgba[i][3];
      rgba[i][1] *= rgba[i][3];
      rgba[i][2] *= rgba[i][3];
    }
}

static void
unpremultiply (float (*rgba)[4],
               gsize   n)
{
  for (gsize i = 0; i < n; i++)
    {
      if (rgba[i][3] > 1/255.0)
        {
          rgba[i][0] /= rgba[i][3];
          rgba[i][1] /= rgba[i][3];
          rgba[i][2] /= rgba[i][3];
        }
    }
}

typedef void (* FastConversionFunc) (guchar       *dest,
                                     const guchar *src,
                                     gsize         n);

static FastConversionFunc
get_fast_conversion_func (GdkMemoryFormat dest_format,
                          GdkMemoryFormat src_format)
{
  if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    return r8g8b8a8_to_r8g8b8a8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    return r8g8b8a8_to_b8g8r8a8_premultiplied;
  else if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    return r8g8b8a8_to_b8g8r8a8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    return r8g8b8a8_to_r8g8b8a8_premultiplied;
  else if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    return r8g8b8a8_to_a8r8g8b8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    return r8g8b8a8_to_a8b8g8r8_premultiplied;
  else if ((src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_R8G8B8A8) ||
           (src_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED))
    return b8g8r8a8_to_r8g8b8a8;
  else if ((src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_B8G8R8A8) ||
           (src_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED))
    return r8g8b8a8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    return r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    return r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    return r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    return r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    return r8g8b8_to_a8r8g8b8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    return r8g8b8_to_a8b8g8r8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_R8G8B8A8)
    return r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_R8G8B8A8)
    return r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_B8G8R8A8)
    return r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_B8G8R8A8)
    return r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_A8R8G8B8)
    return r8g8b8_to_a8r8g8b8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_A8R8G8B8)
    return r8g8b8_to_a8b8g8r8;

  return NULL;
}

typedef struct _MemoryConvert MemoryConvert;

struct _MemoryConvert
{
  guchar              *dest_data;
  gsize                dest_stride;
  GdkMemoryFormat      dest_format;
  GdkColorState       *dest_cs;
  const guchar        *src_data;
  gsize                src_stride;
  GdkMemoryFormat      src_format;
  GdkColorState       *src_cs;
  gsize                width;
  gsize                height;

  /* atomic */ int     rows_done;
};

static void
gdk_memory_convert_generic (gpointer data)
{
  MemoryConvert *mc = data;
  const GdkMemoryFormatDescription *dest_desc = &memory_formats[mc->dest_format];
  const GdkMemoryFormatDescription *src_desc = &memory_formats[mc->src_format];
  float (*tmp)[4];
  GdkFloatColorConvert convert_func = NULL;
  GdkFloatColorConvert convert_func2 = NULL;
  gboolean needs_premultiply, needs_unpremultiply;
  gsize y, n;
  gint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  if (gdk_color_state_equal (mc->src_cs, mc->dest_cs))
    {
      FastConversionFunc func;

      func = get_fast_conversion_func (mc->dest_format, mc->src_format);

      if (func != NULL)
        {
          n = 1;

          for (y = g_atomic_int_add (&mc->rows_done, n);
               y < mc->height;
               y = g_atomic_int_add (&mc->rows_done, n))
            {
              const guchar *src_data = mc->src_data + y * mc->src_stride;
              guchar *dest_data = mc->dest_data + y * mc->dest_stride;

              func (dest_data, src_data, mc->width);
            }
          return;
        }
    }
  else
    {
      convert_func = gdk_color_state_get_convert_to (mc->src_cs, mc->dest_cs);

      if (!convert_func)
        convert_func2 = gdk_color_state_get_convert_from (mc->dest_cs, mc->src_cs);

      if (!convert_func && !convert_func2)
        {
          GdkColorState *connection = GDK_COLOR_STATE_REC2100_LINEAR;
          convert_func = gdk_color_state_get_convert_to (mc->src_cs, connection);
          convert_func2 = gdk_color_state_get_convert_from (mc->dest_cs, connection);
        }
    }

  if (convert_func)
    {
      needs_unpremultiply = src_desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED;
      needs_premultiply = src_desc->alpha != GDK_MEMORY_ALPHA_OPAQUE && dest_desc->alpha != GDK_MEMORY_ALPHA_STRAIGHT;
    }
  else
    {
      needs_unpremultiply = src_desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED && dest_desc->alpha == GDK_MEMORY_ALPHA_STRAIGHT;
      needs_premultiply = src_desc->alpha == GDK_MEMORY_ALPHA_STRAIGHT && dest_desc->alpha != GDK_MEMORY_ALPHA_STRAIGHT;
    }

  tmp = g_malloc (sizeof (*tmp) * mc->width);
  n = 1;

  for (y = g_atomic_int_add (&mc->rows_done, n), rows = 0;
       y < mc->height;
       y = g_atomic_int_add (&mc->rows_done, n), rows++)
    {
      const guchar *src_data = mc->src_data + y * mc->src_stride;
      guchar *dest_data = mc->dest_data + y * mc->dest_stride;

      src_desc->to_float (tmp, src_data, mc->width);

      if (needs_unpremultiply)
        unpremultiply (tmp, mc->width);

      if (convert_func)
        convert_func (mc->src_cs, tmp, mc->width);

      if (convert_func2)
        convert_func2 (mc->dest_cs, tmp, mc->width);

      if (needs_premultiply)
        premultiply (tmp, mc->width);

      dest_desc->from_float (dest_data, tmp, mc->width);
    }

  g_free (tmp);

  ADD_MARK (before,
            "Memory convert (thread)", "size %lux%lu, %lu rows",
            mc->width, mc->height, rows);
}

void
gdk_memory_convert (guchar              *dest_data,
                    gsize                dest_stride,
                    GdkMemoryFormat      dest_format,
                    GdkColorState       *dest_cs,
                    const guchar        *src_data,
                    gsize                src_stride,
                    GdkMemoryFormat      src_format,
                    GdkColorState       *src_cs,
                    gsize                width,
                    gsize                height)
{
  MemoryConvert mc = {
    .dest_data = dest_data,
    .dest_stride = dest_stride,
    .dest_format = dest_format,
    .dest_cs = dest_cs,
    .src_data = src_data,
    .src_stride = src_stride,
    .src_format = src_format,
    .src_cs = src_cs,
    .width = width,
    .height = height,
  };

  g_assert (dest_format < GDK_MEMORY_N_FORMATS);
  g_assert (src_format < GDK_MEMORY_N_FORMATS);
  /* We don't allow overlap here. If you want to do in-place color state conversions,
   * use gdk_memory_convert_color_state.
   */
  g_assert (dest_data + gdk_memory_format_min_buffer_size (dest_format, dest_stride, width, height) <= src_data ||
            src_data + gdk_memory_format_min_buffer_size (src_format, src_stride, width, height) <= dest_data);

  if (src_format == dest_format && gdk_color_state_equal (dest_cs, src_cs))
    {
      const GdkMemoryFormatDescription *src_desc = &memory_formats[src_format];
      gsize bytes_per_row = src_desc->bytes_per_pixel * width;

      if (bytes_per_row == src_stride && bytes_per_row == dest_stride)
        {
          memcpy (dest_data, src_data, bytes_per_row * height);
        }
      else
        {
          gsize y;

          for (y = 0; y < height; y++)
            {
              memcpy (dest_data, src_data, bytes_per_row);
              src_data += src_stride;
              dest_data += dest_stride;
            }
        }
      return;
    }

  gdk_parallel_task_run (gdk_memory_convert_generic, &mc);
}

typedef struct _MemoryConvertColorState MemoryConvertColorState;

struct _MemoryConvertColorState
{
  guchar *data;
  gsize stride;
  GdkMemoryFormat format;
  GdkColorState *src_cs;
  GdkColorState *dest_cs;
  gsize width;
  gsize height;

  /* atomic */ int rows_done;
};

static const guchar srgb_lookup[] = {
  0, 12, 21, 28, 33, 38, 42, 46, 49, 52, 55, 58, 61, 63, 66, 68,
  70, 73, 75, 77, 79, 81, 82, 84, 86, 88, 89, 91, 93, 94, 96, 97,
  99, 100, 102, 103, 104, 106, 107, 109, 110, 111, 112, 114, 115, 116, 117, 118,
  120, 121, 122, 123, 124, 125, 126, 127, 129, 130, 131, 132, 133, 134, 135, 136,
  137, 138, 139, 140, 141, 142, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
  151, 152, 153, 154, 155, 156, 157, 157, 158, 159, 160, 161, 161, 162, 163, 164,
  165, 165, 166, 167, 168, 168, 169, 170, 171, 171, 172, 173, 174, 174, 175, 176,
  176, 177, 178, 179, 179, 180, 181, 181, 182, 183, 183, 184, 185, 185, 186, 187,
  187, 188, 189, 189, 190, 191, 191, 192, 193, 193, 194, 194, 195, 196, 196, 197,
  197, 198, 199, 199, 200, 201, 201, 202, 202, 203, 204, 204, 205, 205, 206, 206,
  207, 208, 208, 209, 209, 210, 210, 211, 212, 212, 213, 213, 214, 214, 215, 215,
  216, 217, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224,
  224, 225, 226, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232,
  233, 233, 234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 240,
  240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 245, 246, 246, 247, 247,
  248, 248, 249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255
};

static const guchar srgb_inverse_lookup[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
  3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7,
  7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
  13, 13, 13, 14, 14, 15, 15, 16, 16, 16, 17, 17, 18, 18, 19, 19,
  20, 20, 21, 22, 22, 23, 23, 24, 24, 25, 26, 26, 27, 27, 28, 29,
  29, 30, 31, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 38, 39, 40,
  41, 42, 42, 43, 44, 45, 46, 47, 47, 48, 49, 50, 51, 52, 53, 54,
  55, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70,
  71, 72, 73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 85, 87, 88,
  89, 90, 92, 93, 94, 95, 97, 98, 99, 101, 102, 103, 105, 106, 107, 109,
  110, 112, 113, 114, 116, 117, 119, 120, 122, 123, 125, 126, 128, 129, 131, 132,
  134, 135, 137, 139, 140, 142, 144, 145, 147, 148, 150, 152, 153, 155, 157, 159,
  160, 162, 164, 166, 167, 169, 171, 173, 175, 176, 178, 180, 182, 184, 186, 188,
  190, 192, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 218, 220,
  222, 224, 226, 228, 230, 232, 235, 237, 239, 241, 243, 245, 248, 250, 252, 255
};

static void
convert_srgb_to_srgb_linear (guchar *data,
                             gsize   n)
{
  for (gsize i = 0; i < n; i++)
    {
      guint16 r = data[0];
      guint16 g = data[1];
      guint16 b = data[2];
      guchar a = data[3];

      if (a != 0)
        {
          r = (r * 255 + a / 2) / a;
          g = (g * 255 + a / 2) / a;
          b = (b * 255 + a / 2) / a;

          r = srgb_inverse_lookup[r];
          g = srgb_inverse_lookup[g];
          b = srgb_inverse_lookup[b];

          r = r * a + 127;
          g = g * a + 127;
          b = b * a + 127;
          data[0] = (r + (r >> 8) + 1) >> 8;
          data[1] = (g + (g >> 8) + 1) >> 8;
          data[2] = (b + (b >> 8) + 1) >> 8;
        }
      data += 4;
    }
}

static void
convert_srgb_linear_to_srgb (guchar *data,
                             gsize   n)
{
  for (gsize i = 0; i < n; i++)
    {
      guint16 r = data[0];
      guint16 g = data[1];
      guint16 b = data[2];
      guchar a = data[3];

      if (a != 0)
        {
          r = (r * 255 + a / 2) / a;
          g = (g * 255 + a / 2) / a;
          b = (b * 255 + a / 2) / a;

          r = srgb_lookup[r];
          g = srgb_lookup[g];
          b = srgb_lookup[b];

          r = r * a + 127;
          g = g * a + 127;
          b = b * a + 127;
          data[0] = (r + (r >> 8) + 1) >> 8;
          data[1] = (g + (g >> 8) + 1) >> 8;
          data[2] = (b + (b >> 8) + 1) >> 8;
        }

      data += 4;
    }
}

static void
gdk_memory_convert_color_state_srgb_to_srgb_linear (gpointer data)
{
  MemoryConvertColorState *mc = data;
  int y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  for (y = g_atomic_int_add (&mc->rows_done, 1), rows = 0;
       y < mc->height;
       y = g_atomic_int_add (&mc->rows_done, 1), rows++)
    {
      convert_srgb_to_srgb_linear (mc->data + y * mc->stride, mc->width);
    }

  ADD_MARK (before,
            "Color state convert srgb->srgb-linear (thread)", "size %lux%lu, %lu rows",
            mc->width, mc->height, rows);
}

static void
gdk_memory_convert_color_state_srgb_linear_to_srgb (gpointer data)
{
  MemoryConvertColorState *mc = data;
  int y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  for (y = g_atomic_int_add (&mc->rows_done, 1), rows = 0;
       y < mc->height;
       y = g_atomic_int_add (&mc->rows_done, 1), rows++)
    {
      convert_srgb_linear_to_srgb (mc->data + y * mc->stride, mc->width);
    }

  ADD_MARK (before,
            "Color state convert srgb-linear->srgb (thread)", "size %lux%lu, %lu rows",
            mc->width, mc->height, rows);
}

static void
gdk_memory_convert_color_state_generic (gpointer user_data)
{
  MemoryConvertColorState *mc = user_data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mc->format];
  GdkFloatColorConvert convert_func = NULL;
  GdkFloatColorConvert convert_func2 = NULL;
  float (*tmp)[4];
  int y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  convert_func = gdk_color_state_get_convert_to (mc->src_cs, mc->dest_cs);

  if (!convert_func)
    {
      convert_func2 = gdk_color_state_get_convert_from (mc->dest_cs, mc->src_cs);
    }

  if (!convert_func && !convert_func2)
    {
      GdkColorState *connection = GDK_COLOR_STATE_REC2100_LINEAR;
      convert_func = gdk_color_state_get_convert_to (mc->src_cs, connection);
      convert_func2 = gdk_color_state_get_convert_from (mc->dest_cs, connection);
    }

  tmp = g_malloc (sizeof (*tmp) * mc->width);

  for (y = g_atomic_int_add (&mc->rows_done, 1), rows = 0;
       y < mc->height;
       y = g_atomic_int_add (&mc->rows_done, 1), rows++)
    {
      guchar *data = mc->data + y * mc->stride;

      desc->to_float (tmp, data, mc->width);

      if (desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED)
        unpremultiply (tmp, mc->width);

      if (convert_func)
        convert_func (mc->src_cs, tmp, mc->width);

      if (convert_func2)
        convert_func2 (mc->dest_cs, tmp, mc->width);

      if (desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED)
        premultiply (tmp, mc->width);

      desc->from_float (data, tmp, mc->width);
    }

  g_free (tmp);

  ADD_MARK (before,
            "Color state convert (thread)", "size %lux%lu, %lu rows",
            mc->width, mc->height, rows);
}

void
gdk_memory_convert_color_state (guchar          *data,
                                gsize            stride,
                                GdkMemoryFormat  format,
                                GdkColorState   *src_color_state,
                                GdkColorState   *dest_color_state,
                                gsize            width,
                                gsize            height)
{
  MemoryConvertColorState mc = {
    .data = data,
    .stride = stride,
    .format = format,
    .src_cs = src_color_state,
    .dest_cs = dest_color_state,
    .width = width,
    .height = height,
  };

  if (gdk_color_state_equal (src_color_state, dest_color_state))
    return;

  if (format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED &&
      src_color_state == GDK_COLOR_STATE_SRGB &&
      dest_color_state == GDK_COLOR_STATE_SRGB_LINEAR)
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_srgb_to_srgb_linear, &mc);
    }
  else if (format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED &&
           src_color_state == GDK_COLOR_STATE_SRGB_LINEAR &&
           dest_color_state == GDK_COLOR_STATE_SRGB)
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_srgb_linear_to_srgb, &mc);
    }
  else
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_generic, &mc);
    }
}

typedef struct _MipmapData MipmapData;

struct _MipmapData
{
  guchar          *dest;
  gsize            dest_stride;
  GdkMemoryFormat  dest_format;
  const guchar    *src;
  gsize            src_stride;
  GdkMemoryFormat  src_format;
  gsize            src_width;
  gsize            src_height;
  guint            lod_level;
  gboolean         linear;

  gint             rows_done;
};

static void
gdk_memory_mipmap_same_format_nearest (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_format];
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      guchar *dest = mipmap->dest + (y >> mipmap->lod_level) * mipmap->dest_stride;
      const guchar *src = mipmap->src + y * mipmap->src_stride;

      desc->mipmap_nearest (dest, mipmap->dest_stride,
                            src, mipmap->src_stride,
                            mipmap->src_width, MIN (n, mipmap->src_height - y),
                            mipmap->lod_level);
    }

  ADD_MARK (before,
            "Mipmap nearest (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_width, mipmap->src_height, mipmap->lod_level, rows);
}

static void
gdk_memory_mipmap_same_format_linear (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_format];
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      guchar *dest = mipmap->dest + (y >> mipmap->lod_level) * mipmap->dest_stride;
      const guchar *src = mipmap->src + y * mipmap->src_stride;

      desc->mipmap_linear (dest, mipmap->dest_stride,
                           src, mipmap->src_stride,
                           mipmap->src_width, MIN (n, mipmap->src_height - y),
                           mipmap->lod_level);
    }

  ADD_MARK (before,
            "Mipmap linear (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_width, mipmap->src_height, mipmap->lod_level, rows);
}

static void
gdk_memory_mipmap_generic (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_format];
  FastConversionFunc func;
  gsize dest_width;
  gsize size;
  guchar *tmp;
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;
  dest_width = (mipmap->src_width + n - 1) >> mipmap->lod_level;
  size = gdk_memory_format_bytes_per_pixel (mipmap->src_format) * dest_width;
  tmp = g_malloc (size);
  func = get_fast_conversion_func (mipmap->dest_format, mipmap->src_format);

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      guchar *dest = mipmap->dest + (y >> mipmap->lod_level) * mipmap->dest_stride;
      const guchar *src = mipmap->src + y * mipmap->src_stride;

      if (mipmap->linear)
        desc->mipmap_linear (tmp, (size + 7) & 7,
                             src, mipmap->src_stride,
                             mipmap->src_width, MIN (n, mipmap->src_height - y),
                             mipmap->lod_level);
      else
        desc->mipmap_nearest (tmp, (size + 7) & 7,
                              src, mipmap->src_stride,
                              mipmap->src_width, MIN (n, mipmap->src_height - y),
                              mipmap->lod_level);
      if (func)
        func (dest, tmp, dest_width);
      else
        gdk_memory_convert (dest, mipmap->dest_stride, mipmap->dest_format, GDK_COLOR_STATE_SRGB,
                            tmp, (size + 7) & 7, mipmap->src_format, GDK_COLOR_STATE_SRGB,
                            dest_width, 1);
    }

  g_free (tmp);

  ADD_MARK (before,
            "Mipmap generic (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_width, mipmap->src_height, mipmap->lod_level, rows);
}

void
gdk_memory_mipmap (guchar          *dest,
                   gsize            dest_stride,
                   GdkMemoryFormat  dest_format,
                   const guchar    *src,
                   gsize            src_stride,
                   GdkMemoryFormat  src_format,
                   gsize            src_width,
                   gsize            src_height,
                   guint            lod_level,
                   gboolean         linear)
{
  MipmapData mipmap = {
    .dest = dest,
    .dest_stride = dest_stride,
    .dest_format = dest_format,
    .src = src,
    .src_stride = src_stride,
    .src_format = src_format,
    .src_width = src_width,
    .src_height = src_height,
    .lod_level = lod_level,
    .linear = linear,
    .rows_done = 0,
  };

  g_assert (lod_level > 0);

  if (dest_format == src_format)
    {
      if (linear)
        gdk_parallel_task_run (gdk_memory_mipmap_same_format_linear, &mipmap);
      else
        gdk_parallel_task_run (gdk_memory_mipmap_same_format_nearest, &mipmap);
    }
  else
    {
      gdk_parallel_task_run (gdk_memory_mipmap_generic, &mipmap);
    }
}

