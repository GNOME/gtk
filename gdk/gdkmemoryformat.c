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

#include "gsk/gl/fp16private.h"

#include <epoxy/gl.h>

typedef struct _GdkMemoryFormatDescription GdkMemoryFormatDescription;

#define TYPED_FUNCS(name, T, R, G, B, A, bpp, scale) \
static void \
name ## _to_float (float        *dest, \
                   const guchar *src_data, \
                   gsize         n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *src = (T *) (src_data + i * bpp); \
      dest[0] = (float) src[R] / scale; \
      dest[1] = (float) src[G] / scale; \
      dest[2] = (float) src[B] / scale; \
      if (A >= 0) dest[3] = (float) src[A] / scale; else dest[3] = 1.0; \
      dest += 4; \
    } \
} \
\
static void \
name ## _from_float (guchar      *dest_data, \
                     const float *src, \
                     gsize        n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *dest = (T *) (dest_data + i * bpp); \
      dest[R] = CLAMP (src[0] * scale + 0.5, 0, scale); \
      dest[G] = CLAMP (src[1] * scale + 0.5, 0, scale); \
      dest[B] = CLAMP (src[2] * scale + 0.5, 0, scale); \
      if (A >= 0) dest[A] = CLAMP (src[3] * scale + 0.5, 0, scale); \
      src += 4; \
    } \
}

#define TYPED_GRAY_FUNCS(name, T, G, A, bpp, scale) \
static void \
name ## _to_float (float        *dest, \
                   const guchar *src_data, \
                   gsize         n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *src = (T *) (src_data + i * bpp); \
      if (A >= 0) dest[3] = (float) src[A] / scale; else dest[3] = 1.0; \
      if (G >= 0) dest[0] = (float) src[G] / scale; else dest[0] = dest[3]; \
      dest[1] = dest[2] = dest[0]; \
      dest += 4; \
    } \
} \
\
static void \
name ## _from_float (guchar      *dest_data, \
                     const float *src, \
                     gsize        n) \
{ \
  for (gsize i = 0; i < n; i++) \
    { \
      T *dest = (T *) (dest_data + i * bpp); \
      if (G >= 0) dest[G] = CLAMP ((src[0] + src[1] + src[2]) * scale / 3.f + 0.5, 0, scale); \
      if (A >= 0) dest[A] = CLAMP (src[3] * scale + 0.5, 0, scale); \
      src += 4; \
    } \
}

TYPED_FUNCS (b8g8r8a8_premultiplied, guchar, 2, 1, 0, 3, 4, 255)
TYPED_FUNCS (a8r8g8b8_premultiplied, guchar, 1, 2, 3, 0, 4, 255)
TYPED_FUNCS (r8g8b8a8_premultiplied, guchar, 0, 1, 2, 3, 4, 255)
TYPED_FUNCS (b8g8r8a8, guchar, 2, 1, 0, 3, 4, 255)
TYPED_FUNCS (a8r8g8b8, guchar, 1, 2, 3, 0, 4, 255)
TYPED_FUNCS (r8g8b8a8, guchar, 0, 1, 2, 3, 4, 255)
TYPED_FUNCS (a8b8g8r8, guchar, 3, 2, 1, 0, 4, 255)
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
r16g16b16_float_to_float (float        *dest,
                          const guchar *src_data,
                          gsize         n)
{
  guint16 *src = (guint16 *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      half_to_float (src, dest, 3);
      dest[3] = 1.0;
      dest += 4;
      src += 3;
    }
}

static void
r16g16b16_float_from_float (guchar      *dest_data,
                            const float *src,
                            gsize        n)
{
  guint16 *dest = (guint16 *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      float_to_half (src, dest, 3);
      dest += 3;
      src += 4;
    }
}

static void
r16g16b16a16_float_to_float (float        *dest,
                             const guchar *src,
                             gsize         n)
{
  half_to_float ((const guint16 *) src, dest, 4 * n);
}

static void
r16g16b16a16_float_from_float (guchar      *dest,
                               const float *src,
                               gsize        n)
{
  float_to_half (src, (guint16 *) dest, 4 * n);
}

static void
a16_float_to_float (float        *dest,
                    const guchar *src_data,
                    gsize         n)
{
  const guint16 *src = (const guint16 *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      half_to_float (src, dest, 1);
      dest[1] = dest[0];
      dest[2] = dest[0];
      dest[3] = dest[0];
      src++;
      dest += 4;
    }
}

static void
a16_float_from_float (guchar      *dest_data,
                      const float *src,
                      gsize        n)
{
  guint16 *dest = (guint16 *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      float_to_half (&src[3], dest, 1);
      dest ++;
      src += 4;
    }
}

static void
r32g32b32_float_to_float (float        *dest,
                          const guchar *src_data,
                          gsize         n)
{
  float *src = (float *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[0] = src[0];
      dest[1] = src[1];
      dest[2] = src[2];
      dest[3] = 1.0;
      dest += 4;
      src += 3;
    }
}

static void
r32g32b32_float_from_float (guchar      *dest_data,
                            const float *src,
                            gsize        n)
{
  float *dest = (float *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[0] = src[0];
      dest[1] = src[1];
      dest[2] = src[2];
      dest += 3;
      src += 4;
    }
}

static void
r32g32b32a32_float_to_float (float        *dest,
                             const guchar *src,
                             gsize         n)
{
  memcpy (dest, src, sizeof (float) * n * 4);
}

static void
r32g32b32a32_float_from_float (guchar      *dest,
                               const float *src,
                               gsize        n)
{
  memcpy (dest, src, sizeof (float) * n * 4);
}

static void
a32_float_to_float (float        *dest,
                    const guchar *src_data,
                    gsize         n)
{
  const float *src = (const float *) src_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[0] = src[0];
      dest[1] = src[0];
      dest[2] = src[0];
      dest[3] = src[0];
      src++;
      dest += 4;
    }
}

static void
a32_float_from_float (guchar      *dest_data,
                      const float *src,
                      gsize        n)
{
  float *dest = (float *) dest_data;
  for (gsize i = 0; i < n; i++)
    {
      dest[0] = src[3];
      dest ++;
      src += 4;
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

struct _GdkMemoryFormatDescription
{
  GdkMemoryAlpha alpha;
  gsize bytes_per_pixel;
  gsize alignment;
  GdkMemoryDepth depth;
  struct {
    guint gl_major;
    guint gl_minor;
    guint gles_major;
    guint gles_minor;
  } min_gl_version;
  struct {
    guint internal_format;
    guint format;
    guint type;
    GLint swizzle[4];
  } gl;
  /* no premultiplication going on here */
  void (* to_float) (float *, const guchar*, gsize);
  void (* from_float) (guchar *, const float *, gsize);
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
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    b8g8r8a8_premultiplied_to_float,
    b8g8r8a8_premultiplied_from_float,
  },
  [GDK_MEMORY_A8R8G8B8_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGBA8, GL_BGRA, GDK_GL_UNSIGNED_BYTE_FLIPPED, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    a8r8g8b8_premultiplied_to_float,
    a8r8g8b8_premultiplied_from_float,
  },
  [GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 0, 0 },
    { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r8g8b8a8_premultiplied_to_float,
    r8g8b8a8_premultiplied_from_float,
  },
  [GDK_MEMORY_B8G8R8A8] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    b8g8r8a8_to_float,
    b8g8r8a8_from_float,
  },
  [GDK_MEMORY_A8R8G8B8] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGBA8, GL_RGBA, GDK_GL_UNSIGNED_BYTE_FLIPPED, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    a8r8g8b8_to_float,
    a8r8g8b8_from_float,
  },
  [GDK_MEMORY_R8G8B8A8] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 0, 0 },
    { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r8g8b8a8_to_float,
    r8g8b8a8_from_float,
  },
  [GDK_MEMORY_A8B8G8R8] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    4,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGBA8, GL_BGRA, GDK_GL_UNSIGNED_BYTE_FLIPPED, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    a8b8g8r8_to_float,
    a8b8g8r8_from_float,
  },
  [GDK_MEMORY_R8G8B8] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    3,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 0, 0 },
    { GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    r8g8b8_to_float,
    r8g8b8_from_float,
  },
  [GDK_MEMORY_B8G8R8] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    3,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, G_MAXUINT, G_MAXUINT },
    { GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    b8g8r8_to_float,
    b8g8r8_from_float,
  },
  [GDK_MEMORY_R16G16B16] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    6,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    r16g16b16_to_float,
    r16g16b16_from_float,
  },
  [GDK_MEMORY_R16G16B16A16_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    8,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r16g16b16a16_to_float,
    r16g16b16a16_from_float,
  },
  [GDK_MEMORY_R16G16B16A16] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    8,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r16g16b16a16_to_float,
    r16g16b16a16_from_float,
  },
  [GDK_MEMORY_R16G16B16_FLOAT] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    6,
    G_ALIGNOF (guint16),
    GDK_MEMORY_FLOAT16,
    { 0, 0, 3, 0 },
    { GL_RGB16F, GL_RGB, GL_HALF_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    r16g16b16_float_to_float,
    r16g16b16_float_from_float,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    8,
    G_ALIGNOF (guint16),
    GDK_MEMORY_FLOAT16,
    { 0, 0, 3, 0 },
    { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r16g16b16a16_float_to_float,
    r16g16b16a16_float_from_float,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    8,
    G_ALIGNOF (guint16),
    GDK_MEMORY_FLOAT16,
    { 0, 0, 3, 0 },
    { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r16g16b16a16_float_to_float,
    r16g16b16a16_float_from_float,
  },
  [GDK_MEMORY_R32G32B32_FLOAT] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    12,
    G_ALIGNOF (float),
    GDK_MEMORY_FLOAT32,
    { 0, 0, 3, 0 },
    { GL_RGB32F, GL_RGB, GL_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    r32g32b32_float_to_float,
    r32g32b32_float_from_float,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    16,
    G_ALIGNOF (float),
    TRUE,
    { 0, 0, 3, 0 },
    { GL_RGBA32F, GL_RGBA, GL_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r32g32b32a32_float_to_float,
    r32g32b32a32_float_from_float,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    16,
    G_ALIGNOF (float),
    GDK_MEMORY_FLOAT32,
    { 0, 0, 3, 0 },
    { GL_RGBA32F, GL_RGBA, GL_FLOAT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    r32g32b32a32_float_to_float,
    r32g32b32a32_float_from_float,
  },
  [GDK_MEMORY_G8A8_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    2,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 3, 0 },
    { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    g8a8_premultiplied_to_float,
    g8a8_premultiplied_from_float,
  },
  [GDK_MEMORY_G8A8] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    2,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 3, 0 },
    { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    g8a8_to_float,
    g8a8_from_float,
  },
  [GDK_MEMORY_G8] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    1,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 3, 0 },
    { GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    g8_to_float,
    g8_from_float,
  },
  [GDK_MEMORY_G16A16_PREMULTIPLIED] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    4,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    g16a16_premultiplied_to_float,
    g16a16_premultiplied_from_float,
  },
  [GDK_MEMORY_G16A16] = {
    GDK_MEMORY_ALPHA_STRAIGHT,
    4,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    g16a16_to_float,
    g16a16_from_float,
  },
  [GDK_MEMORY_G16] = {
    GDK_MEMORY_ALPHA_OPAQUE,
    2,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    g16_to_float,
    g16_from_float,
  },
  [GDK_MEMORY_A8] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    1,
    G_ALIGNOF (guchar),
    GDK_MEMORY_U8,
    { 0, 0, 3, 0 },
    { GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_RED } },
    a8_to_float,
    a8_from_float,
  },
  [GDK_MEMORY_A16] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    2,
    G_ALIGNOF (guint16),
    GDK_MEMORY_U16,
    { 0, 0, 3, 0 },
    { GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_RED } },
    a16_to_float,
    a16_from_float,
  },
  [GDK_MEMORY_A16_FLOAT] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    2,
    G_ALIGNOF (guint16),
    GDK_MEMORY_FLOAT16,
    { 0, 0, 3, 0 },
    { GL_R16F, GL_RED, GL_HALF_FLOAT, { GL_RED, GL_RED, GL_RED, GL_RED } },
    a16_float_to_float,
    a16_float_from_float,
  },
  [GDK_MEMORY_A32_FLOAT] = {
    GDK_MEMORY_ALPHA_PREMULTIPLIED,
    4,
    G_ALIGNOF (float),
    GDK_MEMORY_FLOAT32,
    { 0, 0, 3, 0 },
    { GL_R32F, GL_RED, GL_FLOAT, { GL_RED, GL_RED, GL_RED, GL_RED } },
    a32_float_to_float,
    a32_float_from_float,
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

gsize
gdk_memory_format_alignment (GdkMemoryFormat format)
{
  return memory_formats[format].alignment;
}

/*<private>
 * gdk_memory_format_get_depth:
 * @format: a memory format
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
gdk_memory_format_get_depth (GdkMemoryFormat format)
{
  return memory_formats[format].depth;
}

/*<private>
 * gdk_memory_depth_merge:
 * @depth1: the first depth
 * @depth2: the second depth
 *
 * Returns a depth that can accomodate both given depths
 * without any loss of precision.
 *
 * Returns: The merged depth
 **/
GdkMemoryDepth
gdk_memory_depth_merge (GdkMemoryDepth depth1,
                        GdkMemoryDepth depth2)
{
  switch (depth1)
    {
      case GDK_MEMORY_U8:
        return depth2;

      case GDK_MEMORY_FLOAT32:
        return GDK_MEMORY_FLOAT32;

      case GDK_MEMORY_U16:
      case GDK_MEMORY_FLOAT16:
        if (depth2 == depth1 || depth2 == GDK_MEMORY_U8)
          return depth1;
        else
          return GDK_MEMORY_FLOAT32;

      default:
        g_assert_not_reached ();
        return GDK_MEMORY_U8;
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
      case GDK_MEMORY_U8:
        return GDK_MEMORY_A8;
      case GDK_MEMORY_U16:
        return GDK_MEMORY_A16;
      case GDK_MEMORY_FLOAT16:
        return GDK_MEMORY_A16_FLOAT;
      case GDK_MEMORY_FLOAT32:
        return GDK_MEMORY_A32_FLOAT;
      default:
        g_return_val_if_reached (GDK_MEMORY_A8);
    }
}

gboolean
gdk_memory_format_gl_format (GdkMemoryFormat  format,
                             gboolean         gles,
                             guint            gl_major,
                             guint            gl_minor,
                             guint           *out_internal_format,
                             guint           *out_format,
                             guint           *out_type,
                             GLint            out_swizzle[4])
{
  *out_internal_format = memory_formats[format].gl.internal_format;
  *out_format = memory_formats[format].gl.format;
  *out_type = memory_formats[format].gl.type;
  memcpy (out_swizzle, memory_formats[format].gl.swizzle, sizeof(GLint) * 4);

  if (gles)
    {
      if (memory_formats[format].min_gl_version.gles_major > gl_major ||
          (memory_formats[format].min_gl_version.gles_major == gl_major &&
           memory_formats[format].min_gl_version.gles_minor > gl_minor))
        return FALSE;
    }
  else
    {
      if (memory_formats[format].min_gl_version.gl_major > gl_major ||
          (memory_formats[format].min_gl_version.gl_major == gl_major &&
           memory_formats[format].min_gl_version.gl_minor > gl_minor))
        return FALSE;
    }

  return TRUE;
}

static void
premultiply (float *rgba,
             gsize  n)
{
  for (gsize i = 0; i < n; i++)
    {
      rgba[0] *= rgba[3];
      rgba[1] *= rgba[3];
      rgba[2] *= rgba[3];
      rgba += 4;
    }
}

static void
unpremultiply (float *rgba,
               gsize  n)
{
  for (gsize i = 0; i < n; i++)
    {
      if (rgba[3] > 1/255.0)
        {
          rgba[0] /= rgba[3];
          rgba[1] /= rgba[3];
          rgba[2] /= rgba[3];
        }
      rgba += 4;
    }
}

void
gdk_memory_convert (guchar              *dest_data,
                    gsize                dest_stride,
                    GdkMemoryFormat      dest_format,
                    const guchar        *src_data,
                    gsize                src_stride,
                    GdkMemoryFormat      src_format,
                    gsize                width,
                    gsize                height)
{
  const GdkMemoryFormatDescription *dest_desc = &memory_formats[dest_format];
  const GdkMemoryFormatDescription *src_desc = &memory_formats[src_format];
  float *tmp;
  gsize y;
  void (*func) (guchar *, const guchar *, gsize) = NULL;

  g_assert (dest_format < GDK_MEMORY_N_FORMATS);
  g_assert (src_format < GDK_MEMORY_N_FORMATS);

  if (src_format == dest_format)
    {
      gsize bytes_per_row = src_desc->bytes_per_pixel * width;

      if (bytes_per_row == src_stride && bytes_per_row == dest_stride)
        {
          memcpy (dest_data, src_data, bytes_per_row * height);
        }
      else
        {
          for (y = 0; y < height; y++)
            {
              memcpy (dest_data, src_data, bytes_per_row);
              src_data += src_stride;
              dest_data += dest_stride;
            }
        }
      return;
    }

  if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    func = r8g8b8a8_to_r8g8b8a8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    func = r8g8b8a8_to_b8g8r8a8_premultiplied;
  else if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    func = r8g8b8a8_to_b8g8r8a8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    func = r8g8b8a8_to_r8g8b8a8_premultiplied;
  else if (src_format == GDK_MEMORY_R8G8B8A8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    func = r8g8b8a8_to_a8r8g8b8_premultiplied;
  else if (src_format == GDK_MEMORY_B8G8R8A8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    func = r8g8b8a8_to_a8b8g8r8_premultiplied;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    func = r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_R8G8B8A8_PREMULTIPLIED)
    func = r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    func = r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED)
    func = r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    func = r8g8b8_to_a8r8g8b8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_A8R8G8B8_PREMULTIPLIED)
    func = r8g8b8_to_a8b8g8r8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_R8G8B8A8)
    func = r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_R8G8B8A8)
    func = r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_B8G8R8A8)
    func = r8g8b8_to_b8g8r8a8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_B8G8R8A8)
    func = r8g8b8_to_r8g8b8a8;
  else if (src_format == GDK_MEMORY_R8G8B8 && dest_format == GDK_MEMORY_A8R8G8B8)
    func = r8g8b8_to_a8r8g8b8;
  else if (src_format == GDK_MEMORY_B8G8R8 && dest_format == GDK_MEMORY_A8R8G8B8)
    func = r8g8b8_to_a8b8g8r8;

  if (func != NULL)
    {
      for (y = 0; y < height; y++)
        {
          func (dest_data, src_data, width);
          src_data += src_stride;
          dest_data += dest_stride;
        }
      return;
    }

  tmp = g_new (float, width * 4);

  for (y = 0; y < height; y++)
    {
      src_desc->to_float (tmp, src_data, width);
      if (src_desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED && dest_desc->alpha == GDK_MEMORY_ALPHA_STRAIGHT)
        unpremultiply (tmp, width);
      else if (src_desc->alpha == GDK_MEMORY_ALPHA_STRAIGHT && dest_desc->alpha != GDK_MEMORY_ALPHA_STRAIGHT)
        premultiply (tmp, width);
      dest_desc->from_float (dest_data, tmp, width);
      src_data += src_stride;
      dest_data += dest_stride;
    }

  g_free (tmp);
}
