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
name ## _to_float (float                (*dest)[4], \
                   const guchar          *src_data, \
                   const GdkMemoryLayout *src_layout, \
                   gsize                  y) \
{ \
  src_data += gdk_memory_layout_offset (src_layout, 0, 0, y); \
  for (gsize i = 0; i < src_layout->width; i++) \
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
name ## _from_float (guchar                *dest_data, \
                     const GdkMemoryLayout *dest_layout, \
                     const float          (*src)[4], \
                     gsize                  y) \
{ \
  dest_data += gdk_memory_layout_offset (dest_layout, 0, 0, y); \
  for (gsize i = 0; i < dest_layout->width; i++) \
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
name ## _to_float (float                (*dest)[4], \
                   const guchar          *src_data, \
                   const GdkMemoryLayout *src_layout, \
                   gsize                  y) \
{ \
  src_data += gdk_memory_layout_offset (src_layout, 0, 0, y); \
  for (gsize i = 0; i < src_layout->width; i++) \
    { \
      T *src = (T *) (src_data + i * bpp); \
      if (A >= 0) dest[i][3] = (float) src[A] / scale; else dest[i][3] = 1.0; \
      if (G >= 0) dest[i][0] = (float) src[G] / scale; else dest[i][0] = dest[i][3]; \
      dest[i][1] = dest[i][2] = dest[i][0]; \
    } \
} \
\
static void \
name ## _from_float (guchar                *dest_data, \
                     const GdkMemoryLayout *dest_layout, \
                     const float          (*src)[4], \
                     gsize                  y) \
{ \
  dest_data += gdk_memory_layout_offset (dest_layout, 0, 0, y); \
  for (gsize i = 0; i < dest_layout->width; i++) \
    { \
      T *dest = (T *) (dest_data + i * bpp); \
      if (G >= 0) dest[G] = CLAMP ((src[i][0] + src[i][1] + src[i][2]) * scale / 3.f + 0.5, 0, scale); \
      if (A >= 0) dest[A] = CLAMP (src[i][3] * scale + 0.5, 0, scale); \
    } \
}

#define NV12_FUNCS(name, T, shift, scale, uv_swapped, x_subsample, y_subsample) \
static void \
name ## _to_float (float                (*dest)[4], \
                   const guchar          *src_data, \
                   const GdkMemoryLayout *src_layout, \
                   gsize                  y) \
{ \
  const T *y_data = (const T *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y)); \
  const T *uv_data = (const T *) (src_data + gdk_memory_layout_offset (src_layout, 1, 0, y - y % y_subsample)); \
  for (gsize x = 0; x < src_layout->width; x++) \
    { \
      dest[x][1] = (float) (y_data[x] >> shift) / scale; \
      dest[x][2] = (float) (uv_data[x / x_subsample * 2 + (uv_swapped ? 1 : 0)] >> shift) / scale; \
      dest[x][0] = (float) (uv_data[x / x_subsample * 2 + (uv_swapped ? 0 : 1)] >> shift) / scale; \
      dest[x][3] = 1.0f; \
    } \
} \
\
static void \
name ## _from_float (guchar                *dest_data, \
                     const GdkMemoryLayout *dest_layout, \
                     const float          (*src)[4], \
                     gsize                  y) \
{ \
  T *uv_data = (T *) (dest_data + gdk_memory_layout_offset (dest_layout, 1, 0, y)); \
  T tmp; \
\
  for (gsize ys = 0; ys < y_subsample; ys++) \
    {\
      T *y_data = (T *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y + ys)); \
      for (gsize x = 0; x < dest_layout->width; x++) \
        { \
          tmp = CLAMP (src[ys * dest_layout->width + x][1] * scale + 0.5, 0, scale); \
          y_data[x] = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp; \
        } \
    }\
\
  for (gsize x = 0; x < dest_layout->width; x += x_subsample) \
    { \
      float u = 0, v = 0; \
\
      for (gsize ys = 0; ys < y_subsample; ys++) \
        for (gsize xs = 0; xs < x_subsample; xs++) \
          { \
            u += src[ys * dest_layout->width + x + xs][2]; \
            v += src[ys * dest_layout->width + x + xs][0]; \
          } \
      u /= x_subsample * y_subsample; \
      v /= x_subsample * y_subsample; \
      tmp = CLAMP (u * scale + 0.5, 0, scale); \
      uv_data[x / x_subsample * 2 + (uv_swapped ? 1 : 0)] = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp; \
      tmp = CLAMP (v * scale + 0.5, 0, scale); \
      uv_data[x / x_subsample * 2 + (uv_swapped ? 0 : 1)] = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp; \
    } \
}\
\
static void \
name ## _mipmap_nearest (guchar                *dest, \
                         const guchar          *src, \
                         const GdkMemoryLayout *src_layout,\
                         gsize                  y,\
                         guint                  lod_level) \
{ \
  gsize x, pos; \
  gsize n = 1 << lod_level; \
  T *dest_data = (T *) dest; \
  const T *y_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 0, 0, MIN (y + n / 2, src_layout->height - 1))); \
  const T *uv_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 1, 0, MIN (y + n / 2, src_layout->height - 1) / y_subsample * y_subsample)); \
\
  for (x = 0; x < src_layout->width; x += n) \
    { \
      pos = MIN (x + n / 2, src_layout->width - 1); \
      *dest_data++ = uv_data[pos / x_subsample * 2 + (uv_swapped ? 0 : 1)]; \
      *dest_data++ = y_data[pos]; \
      *dest_data++ = uv_data[pos / x_subsample * 2 + (uv_swapped ? 1 : 0)]; \
    } \
} \
\
static void \
name ## _mipmap_linear (guchar                *dest, \
                        const guchar          *src, \
                        const GdkMemoryLayout *src_layout,\
                        gsize                  y_start,\
                        guint                  lod_level) \
{ \
  gsize x_start, x, y; \
  gsize n = 1 << lod_level; \
  T *dest_data = (T *) dest; \
  T tmp;\
\
  for (x_start = 0; x_start < src_layout->width; x_start += n) \
    { \
      guint32 y_ = 0, u_ = 0, v_ = 0; \
\
      x = 0; /* silence MSVC */\
\
      for (y = 0; y < MIN (n, src_layout->height - y_start); y++) \
        { \
          const T *y_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 0, 0, y + y_start));\
          const T *uv_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 1, 0, (y + y_start) / y_subsample * y_subsample));\
          for (x = 0; x < MIN (n, src_layout->width - x_start); x++) \
            { \
              y_ += y_data[x_start + x] >> shift; \
              u_ += uv_data[(x_start + x) / x_subsample * 2 + (uv_swapped ? 1 : 0)] >> shift; \
              v_ += uv_data[(x_start + x) / x_subsample * 2 + (uv_swapped ? 0 : 1)] >> shift; \
            } \
        } \
\
      tmp = v_ / (x * y); \
      *dest_data++ = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp;\
      tmp = y_ / (x * y); \
      *dest_data++ = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp;\
      tmp = u_ / (x * y); \
      *dest_data++ = shift ? (tmp << shift) | (tmp >> (sizeof (T) * 8 - 2 * shift)) : tmp;\
    } \
} \

#define YUV3_FUNCS(name, T, scale, lshift, uv_swapped, x_subsample, y_subsample) \
static void \
name ## _to_float (float                (*dest)[4], \
                   const guchar          *src_data, \
                   const GdkMemoryLayout *src_layout, \
                   gsize                  y) \
{ \
  const T *y_data = (const T *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y)); \
  const T *u_data = (const T *) (src_data + gdk_memory_layout_offset (src_layout, uv_swapped ? 2 : 1, 0, y - y % y_subsample)); \
  const T *v_data = (const T *) (src_data + gdk_memory_layout_offset (src_layout, uv_swapped ? 1 : 2, 0, y - y % y_subsample)); \
  for (gsize x = 0; x < src_layout->width; x++) \
    { \
      dest[x][1] = CLAMP ((float) y_data[x] / (float) scale, 0.0f, 1.0f); \
      dest[x][2] = CLAMP ((float) u_data[x / x_subsample] / (float) scale, 0.0f, 1.0f); \
      dest[x][0] = CLAMP ((float) v_data[x / x_subsample] / (float) scale, 0.0f, 1.0f); \
      dest[x][3] = 1.0f; \
    } \
} \
\
static void \
name ## _from_float (guchar                *dest_data, \
                     const GdkMemoryLayout *dest_layout, \
                     const float          (*src)[4], \
                     gsize                  y) \
{ \
  T *u_data = (T *) (dest_data + gdk_memory_layout_offset (dest_layout, uv_swapped ? 2 : 1, 0, y)); \
  T *v_data = (T *) (dest_data + gdk_memory_layout_offset (dest_layout, uv_swapped ? 1 : 2, 0, y)); \
\
  for (gsize ys = 0; ys < y_subsample; ys++) \
    {\
      T *y_data = (T *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y + ys)); \
      for (gsize x = 0; x < dest_layout->width; x++) \
        { \
          y_data[x] = CLAMP (src[ys * dest_layout->width + x][1] * scale + 0.5, 0, scale); \
        } \
    }\
\
  for (gsize x = 0; x < dest_layout->width; x += x_subsample) \
    { \
      float u = 0, v = 0; \
\
      for (gsize ys = 0; ys < y_subsample; ys++) \
        for (gsize xs = 0; xs < x_subsample; xs++) \
          { \
            u += src[ys * dest_layout->width + x + xs][2]; \
            v += src[ys * dest_layout->width + x + xs][0]; \
          } \
      u /= x_subsample * y_subsample; \
      v /= x_subsample * y_subsample; \
      u_data[x / x_subsample] = CLAMP (u * scale + 0.5, 0, scale); \
      v_data[x / x_subsample] = CLAMP (v * scale + 0.5, 0, scale); \
    } \
}\
\
static void \
name ## _mipmap_nearest (guchar                *dest, \
                         const guchar          *src, \
                         const GdkMemoryLayout *src_layout,\
                         gsize                  y,\
                         guint                  lod_level) \
{ \
  gsize x, pos; \
  gsize n = 1 << lod_level; \
  T *dest_data = (T *) dest; \
  guint tmp; \
  gsize real_y = MIN (y + n / 2, src_layout->height - 1); \
  const T *y_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 0, 0, real_y)); \
  const T *u_data = (const T *) (src + gdk_memory_layout_offset (src_layout, uv_swapped ? 2 : 1, 0, real_y - real_y % y_subsample)); \
  const T *v_data = (const T *) (src + gdk_memory_layout_offset (src_layout, uv_swapped ? 1 : 2, 0, real_y - real_y % y_subsample)); \
\
  for (x = 0; x < src_layout->width; x += n) \
    { \
      pos = MIN (x + n / 2, src_layout->width - 1); \
      tmp = v_data[pos / x_subsample]; \
      *dest_data++ = lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
      tmp = y_data[pos]; \
      *dest_data++ = lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
      tmp = u_data[pos / x_subsample]; \
      *dest_data++ = lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
    } \
} \
\
static void \
name ## _mipmap_linear (guchar                *dest, \
                        const guchar          *src, \
                        const GdkMemoryLayout *src_layout,\
                        gsize                  y_start,\
                        guint                  lod_level) \
{ \
  gsize x_start, x, y; \
  gsize n = 1 << lod_level; \
  T *dest_data = (T *) dest; \
  guint tmp; \
\
  for (x_start = 0; x_start < src_layout->width; x_start += n) \
    { \
      guint32 y_ = 0, u_ = 0, v_ = 0; \
\
      x = 0; /* silence MSVC */\
\
      for (y = 0; y < MIN (n, src_layout->height - y_start); y++) \
        { \
          const T *y_data = (const T *) (src + gdk_memory_layout_offset (src_layout, 0, 0, y + y_start));\
          const T *u_data = (const T *) (src + gdk_memory_layout_offset (src_layout, uv_swapped ? 2 : 1, 0, (y + y_start) / y_subsample * y_subsample));\
          const T *v_data = (const T *) (src + gdk_memory_layout_offset (src_layout, uv_swapped ? 1 : 2, 0, (y + y_start) / y_subsample * y_subsample));\
          for (x = 0; x < MIN (n, src_layout->width - x_start); x++) \
            { \
              tmp = y_data[x_start + x]; \
              y_ += lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
              tmp = u_data[(x_start + x) / x_subsample]; \
              u_ += lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
              tmp = v_data[(x_start + x) / x_subsample]; \
              v_ += lshift ? (tmp << lshift) | (tmp >> (sizeof (T) * 8 - lshift)) : tmp; \
            } \
        } \
\
      *dest_data++ = v_ / (x * y); \
      *dest_data++ = y_ / (x * y); \
      *dest_data++ = u_ / (x * y); \
    } \
} \

#define YUYV_FUNCS(name, yi, ui, vi) \
static void \
name ## _to_float (float                (*dest)[4], \
                   const guchar          *src_data, \
                   const GdkMemoryLayout *src_layout, \
                   gsize                  y) \
{ \
  const guchar *src = (const guchar *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y)); \
\
  for (gsize x = 0; x < src_layout->width; x += 2) \
    { \
      dest[x][1] = (float) src[2 * x + yi] / 255.f; \
      dest[x][2] = (float) src[2 * x + ui] / 255.f; \
      dest[x][0] = (float) src[2 * x + vi] / 255.f; \
      dest[x][3] = 1.0f; \
      dest[x + 1][1] = (float) src[2 * x + yi + 2] / 255.f; \
      dest[x + 1][2] = (float) src[2 * x + ui] / 255.f; \
      dest[x + 1][0] = (float) src[2 * x + vi] / 255.f; \
      dest[x + 1][3] = 1.0f; \
    } \
} \
\
static void \
name ## _from_float (guchar                *dest_data, \
                     const GdkMemoryLayout *dest_layout, \
                     const float          (*src)[4], \
                     gsize                  y) \
{ \
  guchar *dest = (guchar *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y)); \
\
  for (gsize x = 0; x < dest_layout->width; x += 2) \
    { \
      dest[2 * x + yi] = CLAMP (src[x][1] * 255 + 0.5, 0, 255); \
      dest[2 * x + yi + 2] = CLAMP (src[x + 1][1] * 255 + 0.5, 0, 255); \
      dest[2 * x + ui] = CLAMP ((src[x][2] + src[x + 1][2]) / 2 * 255 + 0.5, 0, 255); \
      dest[2 * x + vi] = CLAMP ((src[x][0] + src[x + 1][0]) / 2 * 255 + 0.5, 0, 255); \
    } \
}\
\
static void \
name ## _mipmap_nearest (guchar                *dest, \
                         const guchar          *src, \
                         const GdkMemoryLayout *src_layout,\
                         gsize                  y,\
                         guint                  lod_level) \
{ \
  gsize x, pos; \
  gsize n = 1 << lod_level; \
  guchar *dest_data = (guchar *) dest; \
  const guchar *src_data = (const guchar *) (src + gdk_memory_layout_offset (src_layout, 0, 0, MIN (y + n / 2, src_layout->height - 1))); \
\
  for (x = 0; x < src_layout->width; x += n) \
    { \
      pos = MIN (x + n / 2, src_layout->width - 1); \
      *dest_data++ = src_data[2 * (pos & ~1) + vi]; \
      *dest_data++ = src_data[2 * pos + yi]; \
      *dest_data++ = src_data[2 * (pos & ~1) + ui]; \
    } \
} \
\
static void \
name ## _mipmap_linear (guchar                *dest, \
                        const guchar          *src, \
                        const GdkMemoryLayout *src_layout,\
                        gsize                  y_start,\
                        guint                  lod_level) \
{ \
  gsize x_start, x, y; \
  gsize n = 1 << lod_level; \
  guchar *dest_data = (guchar *) dest; \
\
  for (x_start = 0; x_start < src_layout->width; x_start += n) \
    { \
      guint32 y_ = 0, u_ = 0, v_ = 0; \
\
      x = 0; /* silence MSVC */\
\
      for (y = 0; y < MIN (n, src_layout->height - y_start); y++) \
        { \
          const guchar *src_data = (const guchar *) (src + gdk_memory_layout_offset (src_layout, 0, 0, y + y_start)); \
          for (x = 0; x < MIN (n, src_layout->width - x_start); x += 2) \
            { \
              y_ += src_data[2 * (x + x_start) + yi] + src_data[2 * (x + x_start) + yi + 2]; \
              u_ += src_data[2 * (x_start + x) + ui]; \
              v_ += src_data[2 * (x_start + x) + vi]; \
            } \
        } \
\
      *dest_data++ = v_ * 2 / (x * y); \
      *dest_data++ = y_ / (x * y); \
      *dest_data++ = u_ * 2 / (x * y); \
    } \
} \

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

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4333)
#endif
NV12_FUNCS (nv12, guchar, 0, 255, FALSE, 2, 2)
NV12_FUNCS (nv21, guchar, 0, 255, TRUE, 2, 2)
NV12_FUNCS (nv16, guchar, 0, 255, FALSE, 2, 1)
NV12_FUNCS (nv61, guchar, 0, 255, TRUE, 2, 1)
NV12_FUNCS (nv24, guchar, 0, 255, FALSE, 1, 1)
NV12_FUNCS (nv42, guchar, 0, 255, TRUE, 1, 1)
NV12_FUNCS (p010, guint16, 6, 1023, FALSE, 2, 2)
NV12_FUNCS (p012, guint16, 4, 4095, FALSE, 2, 2)
NV12_FUNCS (p016, guint16, 0, 65535, FALSE, 2, 2)
#ifdef _MSC_VER
#pragma warning( pop )
#endif

YUV3_FUNCS (yuv410, guchar, 255, 0, FALSE, 4, 4)
YUV3_FUNCS (yvu410, guchar, 255, 0, TRUE, 4, 4)
YUV3_FUNCS (yuv411, guchar, 255, 0, FALSE, 4, 1)
YUV3_FUNCS (yvu411, guchar, 255, 0, TRUE, 4, 1)
YUV3_FUNCS (yuv420, guchar, 255, 0, FALSE, 2, 2)
YUV3_FUNCS (yvu420, guchar, 255, 0, TRUE, 2, 2)
YUV3_FUNCS (yuv422, guchar, 255, 0, FALSE, 2, 1)
YUV3_FUNCS (yvu422, guchar, 255, 0, TRUE, 2, 1)
YUV3_FUNCS (yuv444, guchar, 255, 0, FALSE, 1, 1)
YUV3_FUNCS (yvu444, guchar, 255, 0, TRUE, 1, 1)
YUV3_FUNCS (s010, guint16, 1023, 6, FALSE, 2, 2)
YUV3_FUNCS (s210, guint16, 1023, 6, FALSE, 2, 1)
YUV3_FUNCS (s410, guint16, 1023, 6, FALSE, 1, 1)
YUV3_FUNCS (s012, guint16, 4095, 4, FALSE, 2, 2)
YUV3_FUNCS (s212, guint16, 4095, 4, FALSE, 2, 1)
YUV3_FUNCS (s412, guint16, 4095, 4, FALSE, 1, 1)
YUV3_FUNCS (s016, guint16, 65535, 0, FALSE, 2, 2)
YUV3_FUNCS (s216, guint16, 65535, 0, FALSE, 2, 1)
YUV3_FUNCS (s416, guint16, 65535, 0, FALSE, 1, 1)

YUYV_FUNCS (yuyv, 0, 1, 3)
YUYV_FUNCS (yvyu, 0, 3, 1)
YUYV_FUNCS (uyvy, 1, 0, 2)
YUYV_FUNCS (vyuy, 1, 2, 0)

static void
r16g16b16_float_to_float (float                 (*dest)[4],
                          const guchar          *src_data,
                          const GdkMemoryLayout *src_layout,
                          gsize                  y)
{
  const guint16 *src = (const guint16 *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y));

  for (gsize i = 0; i < src_layout->width; i++)
    {
      half_to_float (src, dest[i], 3);
      dest[i][3] = 1.0;
      src += 3;
    }
}

static void
r16g16b16_float_from_float (guchar                *dest_data,
                            const GdkMemoryLayout *dest_layout,
                            const float          (*src)[4],
                            gsize                  y)
{
  guint16 *dest = (guint16 *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y));

  for (gsize i = 0; i < dest_layout->width; i++)
    {
      float_to_half (src[i], dest, 3);
      dest += 3;
    }
}

static void
r16g16b16a16_float_to_float (float                (*dest)[4],
                             const guchar          *src_data,
                             const GdkMemoryLayout *src_layout,
                             gsize                  y)
{
  const guint16 *src = (const guint16 *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y));

  half_to_float ((const guint16 *) src, (float *) dest, 4 * src_layout->width);
}

static void
r16g16b16a16_float_from_float (guchar                *dest,
                               const GdkMemoryLayout *dest_layout,
                               const float          (*src)[4],
                               gsize                  y)
{
  float_to_half ((const float *) src,
                 (guint16 *) (dest + gdk_memory_layout_offset (dest_layout, 0, 0, y)),
                 4 * dest_layout->width);
}

static void
a16_float_to_float (float                (*dest)[4],
                    const guchar          *src_data,
                    const GdkMemoryLayout *src_layout,
                    gsize                  y)
{
  const guint16 *src = (const guint16 *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y));

  for (gsize i = 0; i < src_layout->width; i++)
    {
      half_to_float (src, &dest[i][0], 1);
      dest[i][1] = dest[i][0];
      dest[i][2] = dest[i][0];
      dest[i][3] = dest[i][0];
      src++;
    }
}

static void
a16_float_from_float (guchar                *dest_data,
                      const GdkMemoryLayout *dest_layout,
                      const float          (*src)[4],
                      gsize                  y)
{
  guint16 *dest = (guint16 *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y));

  for (gsize i = 0; i < dest_layout->width; i++)
    {
      float_to_half (&src[i][3], dest, 1);
      dest ++;
    }
}

static void
r32g32b32_float_to_float (float                (*dest)[4],
                          const guchar          *src_data,
                          const GdkMemoryLayout *src_layout,
                          gsize                  y)
{
  const float (*src)[3] = (const float (*)[3]) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y));

  for (gsize i = 0; i < src_layout->width; i++)
    {
      dest[i][0] = src[i][0];
      dest[i][1] = src[i][1];
      dest[i][2] = src[i][2];
      dest[i][3] = 1.0;
    }
}

static void
r32g32b32_float_from_float (guchar                *dest_data,
                            const GdkMemoryLayout *dest_layout,
                            const float          (*src)[4],
                            gsize                  y)
{
  float (*dest)[3] = (float (*)[3]) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y));

  for (gsize i = 0; i < dest_layout->width; i++)
    {
      dest[i][0] = src[i][0];
      dest[i][1] = src[i][1];
      dest[i][2] = src[i][2];
    }
}

static void
r32g32b32a32_float_to_float (float                (*dest)[4],
                             const guchar          *src_data,
                             const GdkMemoryLayout *src_layout,
                             gsize                  y)
{
  memcpy (dest,
          src_data + gdk_memory_layout_offset (src_layout, 0, 0, y),
          sizeof (float) * src_layout->width * 4);
}

static void
r32g32b32a32_float_from_float (guchar                *dest,
                               const GdkMemoryLayout *dest_layout,
                               const float          (*src)[4],
                               gsize                  y)
{
  memcpy (dest + gdk_memory_layout_offset (dest_layout, 0, 0, y),
          src,
          sizeof (float) * dest_layout->width * 4);
}

static void
a32_float_to_float (float                (*dest)[4],
                    const guchar          *src_data,
                    const GdkMemoryLayout *src_layout,
                    gsize                  y)
{
  const float *src = (const float *) (src_data + gdk_memory_layout_offset (src_layout, 0, 0, y));

  for (gsize i = 0; i < src_layout->width; i++)
    {
      dest[i][0] = src[i];
      dest[i][1] = src[i];
      dest[i][2] = src[i];
      dest[i][3] = src[i];
    }
}

static void
a32_float_from_float (guchar                *dest_data,
                      const GdkMemoryLayout *dest_layout,
                      const float          (*src)[4],
                      gsize                  y)
{
  float *dest = (float *) (dest_data + gdk_memory_layout_offset (dest_layout, 0, 0, y));

  for (gsize i = 0; i < dest_layout->width; i++)
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
gdk_mipmap_ ## DataType ## _ ## n_units ## _nearest (guchar                *dest, \
                                                     const guchar          *src, \
                                                     const GdkMemoryLayout *src_layout,\
                                                     gsize                  y,\
                                                     guint                  lod_level) \
{ \
  gsize x, i; \
  gsize n = 1 << lod_level; \
  DataType *dest_data = (DataType *) dest; \
  const DataType *src_data = (const DataType *) (src + gdk_memory_layout_offset (src_layout, 0, 0, MIN (y + n / 2, src_layout->height - 1))); \
\
  for (x = 0; x < src_layout->width; x += n) \
    { \
      for (i = 0; i < n_units; i++) \
        *dest_data++ = src_data[n_units * MIN (x + n / 2, src_layout->width - 1) + i]; \
    } \
} \
\
static void \
gdk_mipmap_ ## DataType ## _ ## n_units ## _linear (guchar                *dest, \
                                                    const guchar          *src, \
                                                    const GdkMemoryLayout *src_layout,\
                                                    gsize                  y_start,\
                                                    guint                  lod_level) \
{ \
  gsize y, x_dest, x, i; \
  gsize n = 1 << lod_level; \
  DataType *dest_data = (DataType *) dest; \
\
  for (x_dest = 0; x_dest < src_layout->width; x_dest += n) \
    { \
      SumType tmp[n_units] = { 0, }; \
\
      x = 0; /* silence MSVC */\
\
      for (y = 0; y < MIN (n, src_layout->height - y_start); y++) \
        { \
          const DataType *src_data = (const DataType *) (src + gdk_memory_layout_offset (src_layout, 0, 0, y + y_start)); \
          for (x = 0; x < MIN (n, src_layout->width - x_dest); x++) \
            { \
              for (i = 0; i < n_units; i++) \
                tmp[i] += FROM (src_data[n_units * (x_dest + x) + i]); \
            } \
        } \
\
      for (i = 0; i < n_units; i++) \
        *dest_data++ = TO (tmp[i] / (x * y)); \
    } \
}

#define FROM(x) (x)
#define TO(x) (x)
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
#undef FROM
#undef TO
#define half_float guint16
#define FROM half_to_float_one
#define TO float_to_half_one
MIPMAP_FUNC(float, half_float, 1)
MIPMAP_FUNC(float, half_float, 3)
MIPMAP_FUNC(float, half_float, 4)
#undef half_float
#undef FROM
#undef TO

struct _GdkMemoryFormatDescription
{
  const char *name;
  gsize n_planes;
  struct {
    gsize width;
    gsize height;
  } block_size;
  struct {
    struct {
      gsize width;
      gsize height;
    } block_size;
    gsize block_bytes;
  } planes[GDK_MEMORY_MAX_PLANES];
  GdkMemoryAlpha alpha;
  GdkMemoryFormat premultiplied;
  GdkMemoryFormat straight;
  struct {
    /* -1 if none exists, ie the format is already RGBA */
    GdkMemoryFormat format;
    GdkSwizzle swizzle;
  } rgba;
  gsize alignment;
  GdkMemoryDepth depth;
  const GdkMemoryFormat *fallbacks;
  GdkShaderOp default_shader_op;
  struct {
    guint plane;
    GdkSwizzle swizzle;
    struct {
      GLint internal_format;
      GLint internal_srgb_format;
      GLenum format;
      GLenum type;
    } gl;
    guint32 dmabuf_fourcc;
  } shader[3];
#ifdef GDK_RENDERING_VULKAN
  struct {
    VkFormat vk_format;
    VkFormat vk_srgb_format;
    GdkSwizzle ycbcr_swizzle; /* -1 if format is not YCbcr */
  } vulkan;
#endif
  struct {
    DXGI_FORMAT dxgi_format;
    DXGI_FORMAT dxgi_srgb_format;
  } win32;
  struct {
    guint32 rgb_fourcc;
    guint32 yuv_fourcc;
  } dmabuf;
  /* no premultiplication going on here */
  void (* to_float) (float (*)[4], const guchar *, const GdkMemoryLayout *, gsize);
  void (* from_float) (guchar *, const GdkMemoryLayout *, const float (*)[4], gsize);
  GdkMemoryFormat mipmap_format; /* must be single plane continuous format with 1x1 block size */
  void (* mipmap_nearest) (guchar *, const guchar *, const GdkMemoryLayout *, gsize, guint);
  void (* mipmap_linear) (guchar *, const guchar *, const GdkMemoryLayout *, gsize, guint);
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
    .name = "BGRA8p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_B8G8R8A8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .swizzle = GDK_SWIZZLE (B, G, R, A)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_BGRA,
                .internal_srgb_format = 0,
                .format = GL_BGRA,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
      .dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM,
      .dxgi_srgb_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ARGB8888,
        .yuv_fourcc = DRM_FORMAT_AYUV,
    },
    .to_float = b8g8r8a8_premultiplied_to_float,
    .from_float = b8g8r8a8_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8R8G8B8_PREMULTIPLIED] = {
    .name = "ARGB8p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8R8G8B8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .swizzle = GDK_SWIZZLE (G, B, A, R)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_BGRA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
      .dxgi_format = DXGI_FORMAT_UNKNOWN,
      .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_BGRA8888,
        .yuv_fourcc = 0,
    },
    .to_float = a8r8g8b8_premultiplied_to_float,
    .from_float = a8r8g8b8_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8A8_PREMULTIPLIED] = {
    .name = "RGBA8p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_R8G8B8A8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
      .dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .dxgi_srgb_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR8888,
        .yuv_fourcc = DRM_FORMAT_AVUY8888,
    },
    .to_float = r8g8b8a8_premultiplied_to_float,
    .from_float = r8g8b8a8_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8B8G8R8_PREMULTIPLIED] = {
    .name = "ABGR8p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8B8G8R8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .swizzle = GDK_SWIZZLE (A, B, G, R)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
      .dxgi_format = DXGI_FORMAT_UNKNOWN,
      .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_RGBA8888,
        .yuv_fourcc = 0,
    },
    .to_float = a8b8g8r8_premultiplied_to_float,
    .from_float = a8b8g8r8_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_B8G8R8A8] = {
    .name = "BGRA8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_B8G8R8A8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        .swizzle = GDK_SWIZZLE (R, G, B, A)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_BGRA,
                .internal_srgb_format = -1,
                .format = GL_BGRA,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ARGB8888,
        .yuv_fourcc = DRM_FORMAT_AYUV,
    },
    .to_float = b8g8r8a8_to_float,
    .from_float = b8g8r8a8_from_float,
    .mipmap_format = GDK_MEMORY_B8G8R8A8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8R8G8B8] = {
    .name = "ARGB8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8R8G8B8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8,
        .swizzle = GDK_SWIZZLE (G, B, A, R)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_BGRA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_BGRA8888,
        .yuv_fourcc = 0,
    },
    .to_float = a8r8g8b8_to_float,
    .from_float = a8r8g8b8_from_float,
    .mipmap_format = GDK_MEMORY_A8R8G8B8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8A8] = {
    .name = "RGBA8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_R8G8B8A8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR8888,
        .yuv_fourcc = DRM_FORMAT_AVUY8888,
    },
    .to_float = r8g8b8a8_to_float,
    .from_float = r8g8b8a8_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8A8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_A8B8G8R8] = {
    .name = "ABGR8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_A8B8G8R8_PREMULTIPLIED,
    .straight = GDK_MEMORY_A8B8G8R8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8,
        .swizzle = GDK_SWIZZLE (A, B, G, R)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_RGBA8888,
        .yuv_fourcc = 0,
    },
    .to_float = a8b8g8r8_to_float,
    .from_float = a8b8g8r8_from_float,
    .mipmap_format = GDK_MEMORY_A8B8G8R8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_B8G8R8X8] = {
    .name = "BGRX8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_B8G8R8X8,
    .straight = GDK_MEMORY_B8G8R8X8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8,
        .swizzle = GDK_SWIZZLE (B, G, R, 1)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, G, B, 1),
            .gl = {
                .internal_format = GL_BGRA,
                .internal_srgb_format = -1,
                .format = GL_BGRA,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_B8G8R8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_XRGB8888,
        .yuv_fourcc = DRM_FORMAT_XYUV8888,
    },
    .to_float = b8g8r8x8_to_float,
    .from_float = b8g8r8x8_from_float,
    .mipmap_format = GDK_MEMORY_B8G8R8X8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_X8R8G8B8] = {
    .name = "XRGB8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X8R8G8B8,
    .straight = GDK_MEMORY_X8R8G8B8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8,
        .swizzle = GDK_SWIZZLE (G, B, A, 1)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8X8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, G, B, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_BGRA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_BGRX8888,
        .yuv_fourcc = 0,
    },
    .to_float = x8r8g8b8_to_float,
    .from_float = x8r8g8b8_from_float,
    .mipmap_format = GDK_MEMORY_X8R8G8B8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8X8] = {
    .name = "RGBX8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R8G8B8X8,
    .straight = GDK_MEMORY_R8G8B8X8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, G, B, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8B8A8_UNORM,
        .vk_srgb_format = VK_FORMAT_R8G8B8A8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_XBGR8888,
        .yuv_fourcc = DRM_FORMAT_XVUY8888,
    },
    .to_float = r8g8b8x8_to_float,
    .from_float = r8g8b8x8_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8X8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_X8B8G8R8] = {
    .name = "XBGR8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X8B8G8R8,
    .straight = GDK_MEMORY_X8B8G8R8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8A8,
        .swizzle = GDK_SWIZZLE (A, B, G, 1)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8X8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, G, B, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .internal_srgb_format = GL_SRGB8_ALPHA8,
                .format = GL_RGBA,
                .type = GDK_GL_UNSIGNED_BYTE_FLIPPED,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_RGBX8888,
        .yuv_fourcc = 0,
    },
    .to_float = x8b8g8r8_to_float,
    .from_float = x8b8g8r8_from_float,
    .mipmap_format = GDK_MEMORY_X8B8G8R8,
    .mipmap_nearest = gdk_mipmap_guint8_4_nearest,
    .mipmap_linear = gdk_mipmap_guint8_4_linear,
  },
  [GDK_MEMORY_R8G8B8] = {
    .name = "RGB8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 3,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R8G8B8,
    .straight = GDK_MEMORY_R8G8B8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8X8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGB8,
                .internal_srgb_format = GL_SRGB8,
                .format = GL_RGB,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8B8_UNORM,
        .vk_srgb_format = VK_FORMAT_R8G8B8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_BGR888,
        .yuv_fourcc = DRM_FORMAT_VUY888,
    },
    .to_float = r8g8b8_to_float,
    .from_float = r8g8b8_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = gdk_mipmap_guint8_3_nearest,
    .mipmap_linear = gdk_mipmap_guint8_3_linear,
  },
  [GDK_MEMORY_B8G8R8] = {
    .name = "BGR8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 3,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_B8G8R8,
    .straight = GDK_MEMORY_B8G8R8,
    .rgba = {
        .format = GDK_MEMORY_R8G8B8,
        .swizzle = GDK_SWIZZLE (B, G, R, 1)
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGB8,
                .internal_srgb_format = GL_SRGB8,
                .format = GL_BGR,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8_UNORM,
        .vk_srgb_format = VK_FORMAT_B8G8R8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_RGB888,
        .yuv_fourcc = 0,
    },
    .to_float = b8g8r8_to_float,
    .from_float = b8g8r8_from_float,
    .mipmap_format = GDK_MEMORY_B8G8R8,
    .mipmap_nearest = gdk_mipmap_guint8_3_nearest,
    .mipmap_linear = gdk_mipmap_guint8_3_linear,
  },
  [GDK_MEMORY_R16G16B16] = {
    .name = "RGB16",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 6,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R16G16B16,
    .straight = GDK_MEMORY_R16G16B16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGB16,
                .internal_srgb_format = -1,
                .format = GL_RGB,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16_to_float,
    .from_float = r16g16b16_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = gdk_mipmap_guint16_3_nearest,
    .mipmap_linear = gdk_mipmap_guint16_3_linear,
  },
  [GDK_MEMORY_R16G16B16A16_PREMULTIPLIED] = {
    .name = "RGBA16p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 8,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA16,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16A16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16B16A16_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR16161616,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16a16_to_float,
    .from_float = r16g16b16a16_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint16_4_nearest,
    .mipmap_linear = gdk_mipmap_guint16_4_linear,
  },
  [GDK_MEMORY_R16G16B16A16] = {
    .name = "RGBA16",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 8,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA16,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16A16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16B16A16_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR16161616,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16a16_to_float,
    .from_float = r16g16b16a16_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16A16,
    .mipmap_nearest = gdk_mipmap_guint16_4_nearest,
    .mipmap_linear = gdk_mipmap_guint16_4_linear,
  },
  [GDK_MEMORY_R16G16B16_FLOAT] = {
    .name = "RGB16f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 6,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R16G16B16_FLOAT,
    .straight = GDK_MEMORY_R16G16B16_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGB16F,
                .internal_srgb_format = -1,
                .format = GL_RGB,
                .type = GL_HALF_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16_float_to_float,
    .from_float = r16g16b16_float_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16_FLOAT,
    .mipmap_nearest = gdk_mipmap_half_float_3_nearest,
    .mipmap_linear = gdk_mipmap_half_float_3_linear,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED] = {
    .name = "RGBA16fp",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 8,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA16F,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_HALF_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR16161616F,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16a16_float_to_float,
    .from_float = r16g16b16a16_float_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_half_float_4_nearest,
    .mipmap_linear = gdk_mipmap_half_float_4_linear,
  },
  [GDK_MEMORY_R16G16B16A16_FLOAT] = {
    .name = "RGBA16f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 8,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R16G16B16A16_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA16F,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_HALF_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_ABGR16161616F,
        .yuv_fourcc = 0,
    },
    .to_float = r16g16b16a16_float_to_float,
    .from_float = r16g16b16a16_float_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16A16_FLOAT,
    .mipmap_nearest = gdk_mipmap_half_float_4_nearest,
    .mipmap_linear = gdk_mipmap_half_float_4_linear,
  },
  [GDK_MEMORY_R32G32B32_FLOAT] = {
    .name = "RGB32f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 12,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R32G32B32_FLOAT,
    .straight = GDK_MEMORY_R32G32B32_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGB32F,
                .internal_srgb_format = -1,
                .format = GL_RGB,
                .type = GL_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R32G32B32_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R32G32B32_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = r32g32b32_float_to_float,
    .from_float = r32g32b32_float_from_float,
    .mipmap_format = GDK_MEMORY_R32G32B32_FLOAT,
    .mipmap_nearest = gdk_mipmap_float_3_nearest,
    .mipmap_linear = gdk_mipmap_float_3_linear,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED] = {
    .name = "RGBA32fp",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 16,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R32G32B32A32_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA32F,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = r32g32b32a32_float_to_float,
    .from_float = r32g32b32a32_float_from_float,
    .mipmap_format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_float_4_nearest,
    .mipmap_linear = gdk_mipmap_float_4_linear,
  },
  [GDK_MEMORY_R32G32B32A32_FLOAT] = {
    .name = "RGBA32f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 16,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
    .straight = GDK_MEMORY_R32G32B32A32_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RGBA32F,
                .internal_srgb_format = -1,
                .format = GL_RGBA,
                .type = GL_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = r32g32b32a32_float_to_float,
    .from_float = r32g32b32a32_float_from_float,
    .mipmap_format = GDK_MEMORY_R32G32B32A32_FLOAT,
    .mipmap_nearest = gdk_mipmap_float_4_nearest,
    .mipmap_linear = gdk_mipmap_float_4_linear,
  },
  [GDK_MEMORY_G8A8_PREMULTIPLIED] = {
    .name = "GA8p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_G8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_G8A8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, G),
            .gl = {
                .internal_format = GL_RG8,
                .internal_srgb_format = -1,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R8G8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = g8a8_premultiplied_to_float,
    .from_float = g8a8_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_G8A8_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint8_2_nearest,
    .mipmap_linear = gdk_mipmap_guint8_2_linear,
  },
  [GDK_MEMORY_G8A8] = {
    .name = "GA8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_G8A8_PREMULTIPLIED,
    .straight = GDK_MEMORY_G8A8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, G),
            .gl = {
                .internal_format = GL_RG8,
                .internal_srgb_format = -1,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8G8_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R8G8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = g8a8_to_float,
    .from_float = g8a8_from_float,
    .mipmap_format = GDK_MEMORY_G8A8,
    .mipmap_nearest = gdk_mipmap_guint8_2_nearest,
    .mipmap_linear = gdk_mipmap_guint8_2_linear,
  },
  [GDK_MEMORY_G8] = {
    .name = "G8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8,
    .straight = GDK_MEMORY_G8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, 1),
            .gl = {
                .internal_format = GL_R8,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8_UNORM,
        .vk_srgb_format = VK_FORMAT_R8_SRGB,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_R8,
        .yuv_fourcc = 0,
    },
    .to_float = g8_to_float,
    .from_float = g8_from_float,
    .mipmap_format = GDK_MEMORY_G8,
    .mipmap_nearest = gdk_mipmap_guint8_1_nearest,
    .mipmap_linear = gdk_mipmap_guint8_1_linear,
  },
  [GDK_MEMORY_G16A16_PREMULTIPLIED] = {
    .name = "GA16p",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_G16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_G16A16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, G),
            .gl = {
                .internal_format = GL_RG16,
                .internal_srgb_format = -1,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = g16a16_premultiplied_to_float,
    .from_float = g16a16_premultiplied_from_float,
    .mipmap_format = GDK_MEMORY_G16A16_PREMULTIPLIED,
    .mipmap_nearest = gdk_mipmap_guint16_2_nearest,
    .mipmap_linear = gdk_mipmap_guint16_2_linear,
  },
  [GDK_MEMORY_G16A16] = {
    .name = "GA16",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_STRAIGHT,
    .premultiplied = GDK_MEMORY_G16A16_PREMULTIPLIED,
    .straight = GDK_MEMORY_G16A16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16,
        GDK_MEMORY_R32G32B32A32_FLOAT,
        GDK_MEMORY_R16G16B16A16_FLOAT,
        GDK_MEMORY_R8G8B8A8,
        -1,
    },
    .default_shader_op = GDK_SHADER_STRAIGHT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, G),
            .gl = {
                .internal_format = GL_RG16,
                .internal_srgb_format = -1,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16G16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16G16_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = g16a16_to_float,
    .from_float = g16a16_from_float,
    .mipmap_format = GDK_MEMORY_G16A16,
    .mipmap_nearest = gdk_mipmap_guint16_2_nearest,
    .mipmap_linear = gdk_mipmap_guint16_2_linear,
  },
  [GDK_MEMORY_G16] = {
    .name = "G16",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16,
    .straight = GDK_MEMORY_G16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, 1),
            .gl = {
                .internal_format = GL_R16,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
      .dxgi_format = DXGI_FORMAT_R16_UNORM,
      .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = DRM_FORMAT_R16,
        .yuv_fourcc = 0,
    },
    .to_float = g16_to_float,
    .from_float = g16_from_float,
    .mipmap_format = GDK_MEMORY_G16,
    .mipmap_nearest = gdk_mipmap_guint16_1_nearest,
    .mipmap_linear = gdk_mipmap_guint16_1_linear,
  },
  [GDK_MEMORY_A8] = {
    .name = "A8",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A8,
    .straight = GDK_MEMORY_A8,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guchar),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, R),
            .gl = {
                .internal_format = GL_R8,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R8_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_A8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = a8_to_float,
    .from_float = a8_from_float,
    .mipmap_format = GDK_MEMORY_A8,
    .mipmap_nearest = gdk_mipmap_guint8_1_nearest,
    .mipmap_linear = gdk_mipmap_guint8_1_linear,
  },
  [GDK_MEMORY_A16] = {
    .name = "A16",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A16,
    .straight = GDK_MEMORY_A16,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, R),
            .gl = {
                .internal_format = GL_R16,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = a16_to_float,
    .from_float = a16_from_float,
    .mipmap_format = GDK_MEMORY_A16,
    .mipmap_nearest = gdk_mipmap_guint16_1_nearest,
    .mipmap_linear = gdk_mipmap_guint16_1_linear,
  },
  [GDK_MEMORY_A16_FLOAT] = {
    .name = "A16f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A16_FLOAT,
    .straight = GDK_MEMORY_A16_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_FLOAT16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, R),
            .gl = {
                .internal_format = GL_R16F,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_HALF_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R16_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R16_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = a16_float_to_float,
    .from_float = a16_float_from_float,
    .mipmap_format = GDK_MEMORY_A16_FLOAT,
    .mipmap_nearest = gdk_mipmap_half_float_1_nearest,
    .mipmap_linear = gdk_mipmap_half_float_1_linear,
  },
  [GDK_MEMORY_A32_FLOAT] = {
    .name = "A32f",
    .n_planes = 1,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_PREMULTIPLIED,
    .premultiplied = GDK_MEMORY_A32_FLOAT,
    .straight = GDK_MEMORY_A32_FLOAT,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (float),
    .depth = GDK_MEMORY_FLOAT32,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_DEFAULT,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, R, R, R),
            .gl = {
                .internal_format = GL_R32F,
                .internal_srgb_format = -1,
                .format = GL_RED,
                .type = GL_FLOAT,
            },
            .dmabuf_fourcc = 0,
        }
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_R32_SFLOAT,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_R32_FLOAT,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = 0,
    },
    .to_float = a32_float_to_float,
    .from_float = a32_float_from_float,
    .mipmap_format = GDK_MEMORY_A32_FLOAT,
    .mipmap_nearest = gdk_mipmap_float_1_nearest,
    .mipmap_linear = gdk_mipmap_float_1_linear,
  },
  [GDK_MEMORY_G8_B8R8_420] = {
    .name = "NV12",
    .n_planes = 2,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8R8_420,
    .straight = GDK_MEMORY_G8_B8R8_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_GR88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_NV12,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV12,
    },
    .to_float = nv12_to_float,
    .from_float = nv12_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv12_mipmap_nearest,
    .mipmap_linear = nv12_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8B8_420] = {
    .name = "NV21",
    .n_planes = 2,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8B8_420,
    .straight = GDK_MEMORY_G8_R8B8_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE (G, R, B, A),
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_RG88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV21,
    },
    .to_float = nv21_to_float,
    .from_float = nv21_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv21_mipmap_nearest,
    .mipmap_linear = nv21_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8R8_422] = {
    .name = "NV16",
    .n_planes = 2,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8R8_422,
    .straight = GDK_MEMORY_G8_B8R8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_GR88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV16,
    },
    .to_float = nv16_to_float,
    .from_float = nv16_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv16_mipmap_nearest,
    .mipmap_linear = nv16_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8B8_422] = {
    .name = "NV61",
    .n_planes = 2,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8B8_422,
    .straight = GDK_MEMORY_G8_R8B8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE (G, R, B, A),
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_RG88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV61,
    },
    .to_float = nv61_to_float,
    .from_float = nv61_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv61_mipmap_nearest,
    .mipmap_linear = nv61_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8R8_444] = {
    .name = "NV24",
    .n_planes = 2,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8R8_444,
    .straight = GDK_MEMORY_G8_B8R8_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_GR88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV24,
    },
    .to_float = nv24_to_float,
    .from_float = nv24_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv24_mipmap_nearest,
    .mipmap_linear = nv24_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8B8_444] = {
    .name = "NV42",
    .n_planes = 2,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8B8_444,
    .straight = GDK_MEMORY_G8_R8B8_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE (G, R, B, A),
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_RG88,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_NV42,
    },
    .to_float = nv42_to_float,
    .from_float = nv42_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = nv42_mipmap_nearest,
    .mipmap_linear = nv42_mipmap_linear,
  },
  [GDK_MEMORY_G10X6_B10X6R10X6_420] = {
    .name = "P010",
    .n_planes = 2,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G10X6_B10X6R10X6_420,
    .straight = GDK_MEMORY_G10X6_B10X6R10X6_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint32),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG16,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_GR1616,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_P010,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_P010,
    },
    .to_float = p010_to_float,
    .from_float = p010_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = p010_mipmap_nearest,
    .mipmap_linear = p010_mipmap_linear,
  },
  [GDK_MEMORY_G12X4_B12X4R12X4_420] = {
    .name = "P012",
    .n_planes = 2,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G12X4_B12X4R12X4_420,
    .straight = GDK_MEMORY_G12X4_B12X4R12X4_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint32),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG16,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_GR1616,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_P012,
    },
    .to_float = p012_to_float,
    .from_float = p012_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = p012_mipmap_nearest,
    .mipmap_linear = p012_mipmap_linear,
  },
  [GDK_MEMORY_G16_B16R16_420] = {
    .name = "P016",
    .n_planes = 2,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16_B16R16_420,
    .straight = GDK_MEMORY_G16_B16R16_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint32),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG16,
                .format = GL_RG,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_GR1616,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_P016,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_P016,
    },
    .to_float = p016_to_float,
    .from_float = p016_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = p016_mipmap_nearest,
    .mipmap_linear = p016_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8_R8_410] = {
    .name = "YUV410",
    .n_planes = 3,
    .block_size = { 4, 4 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 4 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 4 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8_R8_410,
    .straight = GDK_MEMORY_G8_B8_R8_410,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUV410,
    },
    .to_float = yuv410_to_float,
    .from_float = yuv410_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuv410_mipmap_nearest,
    .mipmap_linear = yuv410_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8_B8_410] = {
    .name = "YVU410",
    .n_planes = 3,
    .block_size = { 4, 4 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 4 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 4 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8_B8_410,
    .straight = GDK_MEMORY_G8_R8_B8_410,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVU410,
    },
    .to_float = yvu410_to_float,
    .from_float = yvu410_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvu410_mipmap_nearest,
    .mipmap_linear = yvu410_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8_R8_411] = {
    .name = "YUV411",
    .n_planes = 3,
    .block_size = { 4, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8_R8_411,
    .straight = GDK_MEMORY_G8_B8_R8_411,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUV411,
    },
    .to_float = yuv411_to_float,
    .from_float = yuv411_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuv411_mipmap_nearest,
    .mipmap_linear = yuv411_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8_B8_411] = {
    .name = "YVU411",
    .n_planes = 3,
    .block_size = { 4, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 4, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8_B8_411,
    .straight = GDK_MEMORY_G8_R8_B8_411,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVU411,
    },
    .to_float = yvu411_to_float,
    .from_float = yvu411_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvu411_mipmap_nearest,
    .mipmap_linear = yvu411_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8_R8_420] = {
    .name = "YUV420",
    .n_planes = 3,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8_R8_420,
    .straight = GDK_MEMORY_G8_B8_R8_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUV420,
    },
    .to_float = yuv420_to_float,
    .from_float = yuv420_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuv420_mipmap_nearest,
    .mipmap_linear = yuv420_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8_B8_420] = {
    .name = "YVU420",
    .n_planes = 3,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8_B8_420,
    .straight = GDK_MEMORY_G8_R8_B8_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVU420,
    },
    .to_float = yvu420_to_float,
    .from_float = yvu420_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvu420_mipmap_nearest,
    .mipmap_linear = yvu420_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8_R8_422] = {
    .name = "YUV422",
    .n_planes = 3,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8_R8_422,
    .straight = GDK_MEMORY_G8_B8_R8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUV422,
    },
    .to_float = yuv422_to_float,
    .from_float = yuv422_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuv422_mipmap_nearest,
    .mipmap_linear = yuv422_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8_B8_422] = {
    .name = "YVU422",
    .n_planes = 3,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8_B8_422,
    .straight = GDK_MEMORY_G8_R8_B8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVU422,
    },
    .to_float = yvu422_to_float,
    .from_float = yvu422_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvu422_mipmap_nearest,
    .mipmap_linear = yvu422_mipmap_linear,
  },
  [GDK_MEMORY_G8_B8_R8_444] = {
    .name = "YUV444",
    .n_planes = 3,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_B8_R8_444,
    .straight = GDK_MEMORY_G8_B8_R8_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUV444,
    },
    .to_float = yuv444_to_float,
    .from_float = yuv444_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuv444_mipmap_nearest,
    .mipmap_linear = yuv444_mipmap_linear,
  },
  [GDK_MEMORY_G8_R8_B8_444] = {
    .name = "YVU444",
    .n_planes = 3,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 1,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8_R8_B8_444,
    .straight = GDK_MEMORY_G8_R8_B8_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R8,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = DRM_FORMAT_R8,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVU444,
    },
    .to_float = yvu444_to_float,
    .from_float = yvu444_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvu444_mipmap_nearest,
    .mipmap_linear = yvu444_mipmap_linear,
  },
  [GDK_MEMORY_G8B8G8R8_422] = {
    .name = "YUYV",
    .n_planes = 1,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 2, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8B8G8R8_422,
    .straight = GDK_MEMORY_G8B8G8R8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (G, A, 0, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_YUY2,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YUYV,
    },
    .to_float = yuyv_to_float,
    .from_float = yuyv_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yuyv_mipmap_nearest,
    .mipmap_linear = yuyv_mipmap_linear,
  },
  [GDK_MEMORY_G8R8G8B8_422] = {
    .name = "YVYU",
    .n_planes = 1,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 2, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G8R8G8B8_422,
    .straight = GDK_MEMORY_G8R8G8B8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (A, G, 0, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G8B8G8R8_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_YVYU,
    },
    .to_float = yvyu_to_float,
    .from_float = yvyu_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = yvyu_mipmap_nearest,
    .mipmap_linear = yvyu_mipmap_linear,
  },
  [GDK_MEMORY_B8G8R8G8_422] = {
    .name = "UYVY",
    .n_planes = 1,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 2, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_B8G8R8G8_422,
    .straight = GDK_MEMORY_B8G8R8G8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (G, R, B, A),
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (R, B, 0, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
         .dxgi_format = DXGI_FORMAT_R8G8_B8G8_UNORM,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_UYVY,
    },
    .to_float = uyvy_to_float,
    .from_float = uyvy_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = uyvy_mipmap_nearest,
    .mipmap_linear = uyvy_mipmap_linear,
  },
  [GDK_MEMORY_R8G8B8G8_422] = {
    .name = "VYUY",
    .n_planes = 1,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 2, 1 },
            .block_bytes = 4,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_R8G8B8G8_422,
    .straight = GDK_MEMORY_R8G8B8G8_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint8),
    .depth = GDK_MEMORY_U8,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R8G8B8,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_2_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (G, R, B, A),
            .gl = {
                .internal_format = GL_RG8,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE (B, R, 0, 1),
            .gl = {
                .internal_format = GL_RGBA8,
                .format = GL_RGBA,
                .type = GL_UNSIGNED_BYTE
            },
            .dmabuf_fourcc = 0,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_B8G8R8G8_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE (B, G, R, A),
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_VYUY,
    },
    .to_float = vyuy_to_float,
    .from_float = vyuy_from_float,
    .mipmap_format = GDK_MEMORY_R8G8B8,
    .mipmap_nearest = vyuy_mipmap_nearest,
    .mipmap_linear = vyuy_mipmap_linear,
  },
  [GDK_MEMORY_X6G10_X6B10_X6R10_420] = {
    .name = "S010",
    .n_planes = 3,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X6G10_X6B10_X6R10_420,
    .straight = GDK_MEMORY_X6G10_X6B10_X6R10_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_10BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S010,
    },
    .to_float = s010_to_float,
    .from_float = s010_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s010_mipmap_nearest,
    .mipmap_linear = s010_mipmap_linear,
  },
  [GDK_MEMORY_X6G10_X6B10_X6R10_422] = {
    .name = "S210",
    .n_planes = 3,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X6G10_X6B10_X6R10_422,
    .straight = GDK_MEMORY_X6G10_X6B10_X6R10_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_10BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S210,
    },
    .to_float = s210_to_float,
    .from_float = s210_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s210_mipmap_nearest,
    .mipmap_linear = s210_mipmap_linear,
  },
  [GDK_MEMORY_X6G10_X6B10_X6R10_444] = {
    .name = "S410",
    .n_planes = 3,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X6G10_X6B10_X6R10_444,
    .straight = GDK_MEMORY_X6G10_X6B10_X6R10_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_10BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S410,
    },
    .to_float = s410_to_float,
    .from_float = s410_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s410_mipmap_nearest,
    .mipmap_linear = s410_mipmap_linear,
  },
  [GDK_MEMORY_X4G12_X4B12_X4R12_420] = {
    .name = "S012",
    .n_planes = 3,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X4G12_X4B12_X4R12_420,
    .straight = GDK_MEMORY_X4G12_X4B12_X4R12_420,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_12BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S012,
    },
    .to_float = s012_to_float,
    .from_float = s012_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s012_mipmap_nearest,
    .mipmap_linear = s012_mipmap_linear,
  },
  [GDK_MEMORY_X4G12_X4B12_X4R12_422] = {
    .name = "S212",
    .n_planes = 3,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X4G12_X4B12_X4R12_422,
    .straight = GDK_MEMORY_X4G12_X4B12_X4R12_422,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_12BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S212,
    },
    .to_float = s212_to_float,
    .from_float = s212_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s212_mipmap_nearest,
    .mipmap_linear = s212_mipmap_linear,
  },
  [GDK_MEMORY_X4G12_X4B12_X4R12_444] = {
    .name = "S412",
    .n_planes = 3,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_X4G12_X4B12_X4R12_444,
    .straight = GDK_MEMORY_X4G12_X4B12_X4R12_444,
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .alignment = G_ALIGNOF (guint16),
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES_12BIT_LSB,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_UNDEFINED,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = -1,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S412,
    },
    .to_float = s412_to_float,
    .from_float = s412_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s412_mipmap_nearest,
    .mipmap_linear = s412_mipmap_linear,
  },
  [GDK_MEMORY_G16_B16_R16_420] = {
    .name = "S016",
    .n_planes = 3,
    .block_size = { 2, 2 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 2 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16_B16_R16_420,
    .straight = GDK_MEMORY_G16_B16_R16_420,
    .alignment = G_ALIGNOF (guint16),
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S016,
    },
    .to_float = s016_to_float,
    .from_float = s016_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s016_mipmap_nearest,
    .mipmap_linear = s016_mipmap_linear,
  },
  [GDK_MEMORY_G16_B16_R16_422] = {
    .name = "S216",
    .n_planes = 3,
    .block_size = { 2, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 2, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16_B16_R16_422,
    .straight = GDK_MEMORY_G16_B16_R16_422,
    .alignment = G_ALIGNOF (guint16),
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S216,
    },
    .to_float = s216_to_float,
    .from_float = s216_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s216_mipmap_nearest,
    .mipmap_linear = s216_mipmap_linear,
  },
  [GDK_MEMORY_G16_B16_R16_444] = {
    .name = "S416",
    .n_planes = 3,
    .block_size = { 1, 1 },
    .planes = {
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
        {
            .block_size = { 1, 1 },
            .block_bytes = 2,
        },
    },
    .alpha = GDK_MEMORY_ALPHA_OPAQUE,
    .premultiplied = GDK_MEMORY_G16_B16_R16_444,
    .straight = GDK_MEMORY_G16_B16_R16_444,
    .alignment = G_ALIGNOF (guint16),
    .rgba = {
        .format = -1,
        .swizzle = GDK_SWIZZLE_IDENTITY
    },
    .depth = GDK_MEMORY_U16,
    .fallbacks = (GdkMemoryFormat[]) {
        GDK_MEMORY_R16G16B16,
        GDK_MEMORY_R16G16B16A16_PREMULTIPLIED,
        GDK_MEMORY_R8G8B8A8_PREMULTIPLIED,
        -1,
    },
    .default_shader_op = GDK_SHADER_3_PLANES,
    .shader = {
        {
            .plane = 0,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 1,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
        {
            .plane = 2,
            .swizzle = GDK_SWIZZLE_IDENTITY,
            .gl = {
                .internal_format = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_SHORT
            },
            .dmabuf_fourcc = DRM_FORMAT_R16,
        },
    },
#ifdef GDK_RENDERING_VULKAN
    .vulkan = {
        .vk_format = VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
        .vk_srgb_format = VK_FORMAT_UNDEFINED,
        .ycbcr_swizzle = GDK_SWIZZLE_IDENTITY,
    },
#endif
    .win32 = {
        .dxgi_format = DXGI_FORMAT_UNKNOWN,
        .dxgi_srgb_format = DXGI_FORMAT_UNKNOWN,
    },
    .dmabuf = {
        .rgb_fourcc = 0,
        .yuv_fourcc = DRM_FORMAT_S416,
    },
    .to_float = s416_to_float,
    .from_float = s416_from_float,
    .mipmap_format = GDK_MEMORY_R16G16B16,
    .mipmap_nearest = s416_mipmap_nearest,
    .mipmap_linear = s416_mipmap_linear,
  },
};

/* if this fails, somebody forgot to add formats above */
G_STATIC_ASSERT (G_N_ELEMENTS (memory_formats) == GDK_MEMORY_N_FORMATS);

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

gboolean
gdk_memory_format_get_rgba_format (GdkMemoryFormat  format,
                                   GdkMemoryFormat *out_format,
                                   GdkSwizzle      *out_swizzle)
{
  GdkMemoryFormat actual = memory_formats[format].rgba.format;

  if (actual == -1)
    return FALSE;

  *out_swizzle = memory_formats[format].rgba.swizzle;
  *out_format = actual;

  return TRUE;
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

GdkMemoryFormat
gdk_memory_format_get_mipmap_format (GdkMemoryFormat format)
{
  return memory_formats[format].mipmap_format;
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

/*<private>
 * gdk_memory_format_get_plane_block_width:
 * @format: a memory format
 * @plane: the plane to query. Must be a valid plane of the
 *   format
 *
 * Each texture plane is made up of blocks. The properties of
 * those blocks are defined by the format.
 *
 * This function returns the number of pixels occupied by one
 * block of data in the x direction.
 *
 * Note that this is different from
 * gdk_memory_format_get_block_width() in that planes may have
 * multiple blocks covering a single image block.
 * Plane blocks can however never be larger than image blocks.
 *
 * Returns: the width of a block
 **/
gsize
gdk_memory_format_get_plane_block_width (GdkMemoryFormat format,
                                         gsize           plane)
{
  return memory_formats[format].planes[plane].block_size.width;
}

/*<private>
 * gdk_memory_format_get_plane_block_height:
 * @format: a memory format
 * @plane: the plane to query. Must be a valid plane of the
 *   format
 *
 * Each texture plane is made up of blocks. The properties of
 * those blocks are defined by the format.
 *
 * This function returns the number of pixels occupied by one
 * block of data in the y direction.
 *
 * Note that this is different from
 * gdk_memory_format_get_block_height() in that planes may have
 * multiple blocks covering a single image block.
 * Plane blocks can however never be larger than image blocks.
 *
 * Returns: the height of a block
 **/
gsize
gdk_memory_format_get_plane_block_height (GdkMemoryFormat format,
                                          gsize           plane)
{
  return memory_formats[format].planes[plane].block_size.height;
}

/**
 * gdk_memory_format_get_plane_block_bytes:
 * @format: a memory format
 * @plane: the plane to query. Must be a valid plane of the
 *   format
 *
 * Each texture plane is made up of blocks. The properties of
 * those blocks are defined by the format.
 *
 * This function returns the number of bytes in memory occupied
 * pixels occupied by one block of data.
 *
 * Returns: The number of bytes required for one block
 **/
gsize
gdk_memory_format_get_plane_block_bytes (GdkMemoryFormat format,
                                         gsize           plane)
{
  return memory_formats[format].planes[plane].block_bytes;
}

/*<private>
 * gdk_memory_format_get_n_planes:
 * @format: a memory format
 *
 * Gets the number of planes that describe this format. Usually,
 * this number 1 but for video formats in particular, it
 * can be up to GDK_MEMORY_MAX_PLANES.
 *
 * Returns: The number of planes
 **/
gsize
gdk_memory_format_get_n_planes (GdkMemoryFormat format)
{
  return memory_formats[format].n_planes;
}

/**
 * gdk_memory_format_get_block_width:
 * @format: a memory format
 *
 * Memory and in turn textures are made up of blocks. Each block can
 * cover more than one pixel in both directions.
 *
 * This is mainly the case for compressed and subsampled formats, normal
 * formats will have a 1x1 block size.
 *
 * All allocations in this format must have a width that is a multiple
 * of the block width.
 *
 * Returns: The width of a block in pixels
 **/
gsize
gdk_memory_format_get_block_width (GdkMemoryFormat format)
{
  return memory_formats[format].block_size.width;
}

/**
 * gdk_memory_format_get_block_height:
 * @format: a memory format
 *
 * Memory and in turn textures are made up of blocks. Each block can
 * cover more than one pixel in both directions.
 *
 * This is mainly the case for compressed and subsampled formats, normal
 * formats will have a 1x1 block size.
 *
 * All allocations in this format must have a height that is a multiple
 * of the block height.
 *
 * Returns: The height of a block in pixels
 **/
gsize
gdk_memory_format_get_block_height (GdkMemoryFormat format)
{
  return memory_formats[format].block_size.height;
}

gboolean
gdk_memory_format_is_block_boundary (GdkMemoryFormat format,
                                     gsize           x,
                                     gsize           y)
{
  return x % memory_formats[format].block_size.width == 0 &&
         y % memory_formats[format].block_size.height == 0;
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

gboolean
gdk_memory_format_gl_format (GdkMemoryFormat  format,
                             gsize            plane,
                             gboolean         gles,
                             GLint           *out_internal_format,
                             GLint           *out_internal_srgb_format,
                             GLenum          *out_format,
                             GLenum          *out_type,
                             GdkSwizzle      *out_swizzle)
{
  if (memory_formats[format].shader[plane].gl.internal_format == 0)
    return FALSE;

  if (!gles && memory_formats[format].shader[plane].gl.internal_format == GL_BGRA)
    *out_internal_format = GL_RGBA8;
  else
    *out_internal_format = memory_formats[format].shader[plane].gl.internal_format;
  *out_internal_srgb_format = memory_formats[format].shader[plane].gl.internal_srgb_format;
  *out_format = memory_formats[format].shader[plane].gl.format;
  *out_type = memory_formats[format].shader[plane].gl.type;
  *out_swizzle = memory_formats[format].shader[plane].swizzle;

  return TRUE;
}

#ifdef GDK_RENDERING_VULKAN

/* Vulkan version of gdk_memory_format_gl_format()
 * Returns VK_FORMAT_UNDEFINED on failure */
VkFormat
gdk_memory_format_vk_format (GdkMemoryFormat     format,
                             VkComponentMapping *out_swizzle,
                             gboolean           *needs_ycbcr_conversion)
{
  if (memory_formats[format].vulkan.ycbcr_swizzle == -1)
    {
      if (out_swizzle)
        gdk_swizzle_to_vk_component_mapping (memory_formats[format].shader[0].swizzle, out_swizzle);
      if (needs_ycbcr_conversion)
        *needs_ycbcr_conversion = FALSE;
    }
  else
    {
      if (out_swizzle)
        gdk_swizzle_to_vk_component_mapping (memory_formats[format].vulkan.ycbcr_swizzle, out_swizzle);
      if (needs_ycbcr_conversion)
        *needs_ycbcr_conversion = TRUE;
    }

  return memory_formats[format].vulkan.vk_format;
}

/* Gets the matching SRGB version of a VkFormat
 * Returns VK_FORMAT_UNDEFINED if none exists */
VkFormat
gdk_memory_format_vk_srgb_format (GdkMemoryFormat format)
{
  return memory_formats[format].vulkan.vk_srgb_format;
}
#endif

gboolean
gdk_memory_format_find_by_dxgi_format (DXGI_FORMAT      format,
                                       gboolean         premultiplied,
                                       GdkMemoryFormat *out_format)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (memory_formats); i++)
    {
      if (memory_formats[i].win32.dxgi_format == format)
        {
          if (premultiplied)
            *out_format = memory_formats[i].premultiplied;
          else
            *out_format = memory_formats[i].straight;
          return TRUE;
        }
      if (memory_formats[i].win32.dxgi_srgb_format == format)
        {
          if (premultiplied)
            *out_format = memory_formats[i].premultiplied;
          else
            *out_format = memory_formats[i].straight;
          return TRUE;
        }
    }

  *out_format = GDK_MEMORY_N_FORMATS;
  return FALSE;
}

/* DXGI version of gdk_memory_format_gl_format()
 * Returns DXGI_FORMAT_UNKNOWN on failure */
DXGI_FORMAT
gdk_memory_format_get_dxgi_format (GdkMemoryFormat  format,
                                   guint           *out_shader_4_component_mapping)
{
  if (out_shader_4_component_mapping)
    *out_shader_4_component_mapping = gdk_swizzle_to_d3d12 (memory_formats[format].shader[0].swizzle);
  return memory_formats[format].win32.dxgi_format;
}

/* Gets the matching SRGB version of a DXGI_FORMAT
 * Returns DXGI_FORMAT_UNKNOWN if none exists */
DXGI_FORMAT
gdk_memory_format_get_dxgi_srgb_format (GdkMemoryFormat format)
{
  return memory_formats[format].win32.dxgi_srgb_format;
}

gboolean
gdk_memory_format_find_by_dmabuf_fourcc (guint32          fourcc,
                                         gboolean         premultiplied,
                                         GdkMemoryFormat *out_format,
                                         gboolean        *out_is_yuv)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (memory_formats); i++)
    {
      if (memory_formats[i].dmabuf.rgb_fourcc == fourcc)
        {
          if (premultiplied)
            *out_format = memory_formats[i].premultiplied;
          else
            *out_format = memory_formats[i].straight;
          *out_is_yuv = FALSE;
          return TRUE;
        }
      if (memory_formats[i].dmabuf.yuv_fourcc == fourcc)
        {
          if (premultiplied)
            *out_format = memory_formats[i].premultiplied;
          else
            *out_format = memory_formats[i].straight;
          *out_is_yuv = TRUE;
          return TRUE;
        }
    }

  *out_format = GDK_MEMORY_N_FORMATS;
  return FALSE;
}

/*
 * gdk_memory_format_get_dmabuf_rgb_fourcc:
 * @format: The memory format
 *
 * Gets the dmabuf fourcc for RGB data in a given memory format.
 *
 * The format is an exact match, so data can be copied between the
 * dmabuf and data of the format. This is different from the
 * memoryformat returned by a GdkDmabufTexture, which is just the
 * closest match.
 *
 * Not all formats have a corresponding RGB dmabuf format.
 * In those cases 0 will be returned.
 *
 * Returns: the RGB fourcc or 0
 **/
guint32
gdk_memory_format_get_dmabuf_rgb_fourcc (GdkMemoryFormat format)
{
  return memory_formats[format].dmabuf.rgb_fourcc;
}

/*<private>
 * gdk_memory_format_get_dmabuf_yuv_fourcc:
 * @format: The memory format
 *
 * Gets the dmabuf fourcc for YUV data in a given memory format.
 *
 * The format is an exact match, so data can be copied between the
 * dmabuf and data of the format. This is different from the
 * memoryformat returned by a GdkDmabufTexture, which is just the
 * closest match.
 *
 * Not all formats have a corresponding YUV dmabuf format.
 * In those cases 0 will be returned.
 *
 * Returns: the YUV fourcc or 0
 **/
guint32
gdk_memory_format_get_dmabuf_yuv_fourcc (GdkMemoryFormat format)
{
  return memory_formats[format].dmabuf.yuv_fourcc;
}

/*<private>
 * gdk_memory_format_get_dmabuf_shader_fourcc:
 * @format: The memory format
 * @plane: nth plane
 *
 * Gets the dmabuf fourcc for multiplane shader mappings in a given memory
 * format.
 *
 * This function is intended to be used in combination with 
 * gdk_memory_format_get_shader_plane(), the `plane` argument passed to
 * that function should match the `plane` argument passed to this function.
 *
 * Not all formats have matching dmabuf shader formats. In those cases
 * 0 will be returned for all planes.
 *
 * If the format is not multiplanar, then this function will always return 0
 * as that would jsut be duplication with the functionality of
 * gdk_memory_format_get_dmabuf_rgb/yuv_fourcc() and they can be used instead.
 *
 * Returns: the plane's fourcc or 0
 **/
guint32
gdk_memory_format_get_dmabuf_shader_fourcc (GdkMemoryFormat format,
                                            gsize           plane)
{
  return memory_formats[format].shader[plane].dmabuf_fourcc;
}

const char *
gdk_memory_format_get_name (GdkMemoryFormat format)
{
  return memory_formats[format].name;
}

GdkShaderOp
gdk_memory_format_get_default_shader_op (GdkMemoryFormat format)
{
  return memory_formats[format].default_shader_op;
}

gsize
gdk_memory_format_get_shader_plane (GdkMemoryFormat  format,
                                    gsize            plane,
                                    gsize           *width_subsample,
                                    gsize           *height_subsample,
                                    gsize           *bpp)
{
  guint p = memory_formats[format].shader[plane].plane;

  if (plane == 0 &&
      (format == GDK_MEMORY_G8B8G8R8_422 ||
       format == GDK_MEMORY_G8R8G8B8_422 ||
       format == GDK_MEMORY_R8G8B8G8_422 ||
       format == GDK_MEMORY_B8G8R8G8_422))
    {
      *width_subsample = 1;
      *height_subsample = 1;
      *bpp = 2;
    }
  else
    {
      *width_subsample = memory_formats[format].planes[p].block_size.width;
      *height_subsample = memory_formats[format].planes[p].block_size.height;
      *bpp = memory_formats[format].planes[p].block_bytes;
    }

  return p;
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
  GdkMemoryLayout      dest_layout;
  GdkColorState       *dest_cs;
  const guchar        *src_data;
  GdkMemoryLayout      src_layout;
  GdkColorState       *src_cs;
  gsize                chunk_size;

  /* atomic */ int     rows_done;
};

static void
gdk_memory_convert_generic (gpointer data)
{
  MemoryConvert *mc = data;
  const GdkMemoryFormatDescription *dest_desc = &memory_formats[mc->dest_layout.format];
  const GdkMemoryFormatDescription *src_desc = &memory_formats[mc->src_layout.format];
  float (*tmp)[4];
  GdkFloatColorConvert convert_func = NULL;
  GdkFloatColorConvert convert_func2 = NULL;
  gboolean needs_premultiply, needs_unpremultiply;
  gsize y0, y;
  gint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  if (gdk_color_state_equal (mc->src_cs, mc->dest_cs))
    {
      FastConversionFunc func;

      func = get_fast_conversion_func (mc->dest_layout.format, mc->src_layout.format);

      if (func != NULL)
        {
          for (y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size), rows = 0;
               y0 < mc->dest_layout.height;
               y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size))
            {
              for (y = y0; y < MIN (y0 + mc->chunk_size, mc->dest_layout.height); y++, rows++)
                {
                  const guchar *src_data = mc->src_data + gdk_memory_layout_offset (&mc->src_layout, 0, 0, y);
                  guchar *dest_data = mc->dest_data + gdk_memory_layout_offset (&mc->dest_layout, 0, 0, y);

                  func (dest_data, src_data, mc->dest_layout.width);
                }
            }

          ADD_MARK (before,
                    "Memory convert (thread)", "size %lux%lu, %lu rows",
                    mc->dest_layout.width, mc->dest_layout.height, rows);
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

  tmp = g_malloc (sizeof (*tmp) * mc->dest_layout.width * dest_desc->block_size.height);

  for (y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size), rows = 0;
       y0 < mc->dest_layout.height;
       y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size))
    {
      for (y = y0; y < MIN (y0 + mc->chunk_size, mc->dest_layout.height); y++, rows++)
        {
          float (*row)[4] = &tmp[mc->dest_layout.width * (y % dest_desc->block_size.height)];

          src_desc->to_float (row, mc->src_data, &mc->src_layout, y);

          if (needs_unpremultiply)
            unpremultiply (row, mc->dest_layout.width);

          if (convert_func)
            convert_func (mc->src_cs, row, mc->dest_layout.width);

          if (convert_func2)
            convert_func2 (mc->dest_cs, row, mc->dest_layout.width);

          if (needs_premultiply)
            premultiply (row, mc->dest_layout.width);

          if (y % dest_desc->block_size.height == dest_desc->block_size.height - 1)
            dest_desc->from_float (mc->dest_data, &mc->dest_layout, tmp, y - (dest_desc->block_size.height - 1));
        }
    }

  g_free (tmp);

  ADD_MARK (before,
            "Memory convert (thread)", "size %lux%lu, %lu rows",
            mc->dest_layout.width, mc->dest_layout.height, rows);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

void
gdk_memory_convert (guchar                *dest_data,
                    const GdkMemoryLayout *dest_layout,
                    GdkColorState         *dest_cs,
                    const guchar          *src_data,
                    const GdkMemoryLayout *src_layout,
                    GdkColorState         *src_cs)
{
  MemoryConvert mc = {
    .dest_data = dest_data,
    .dest_layout = *dest_layout,
    .dest_cs = dest_cs,
    .src_data = src_data,
    .src_layout = *src_layout,
    .src_cs = src_cs,
    .chunk_size = round_up (MAX (1, 512 / dest_layout->width), gdk_memory_format_get_block_height (dest_layout->format)),
  };

  guint n_tasks;

  /* Use gdk_memory_layout_init_sublayout() if you encounter this */
  g_assert (dest_layout->width == src_layout->width);
  g_assert (dest_layout->height == src_layout->height);
  g_assert (dest_layout->format < GDK_MEMORY_N_FORMATS);
  g_assert (src_layout->format < GDK_MEMORY_N_FORMATS);
  /* We don't allow overlap here. If you want to do in-place color state conversions,
   * use gdk_memory_convert_color_state.
   */
  g_assert (!gdk_memory_layout_has_overlap (dest_data, dest_layout, src_data, src_layout));

  if (src_layout->format == dest_layout->format && gdk_color_state_equal (dest_cs, src_cs))
    {
      gdk_memory_copy (dest_data, dest_layout, src_data, src_layout);
      return;
    }

  n_tasks = (mc.dest_layout.height + mc.chunk_size - 1) / mc.chunk_size;

  gdk_parallel_task_run (gdk_memory_convert_generic, &mc, n_tasks);
}

typedef struct _MemoryConvertColorState MemoryConvertColorState;

struct _MemoryConvertColorState
{
  guchar *data;
  GdkMemoryLayout layout;
  GdkColorState *src_cs;
  GdkColorState *dest_cs;
  gsize chunk_size;

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
  gsize y0, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  for (y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size), rows = 0;
       y0 < mc->layout.height;
       y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size))
    {
      for (y = y0; y < MIN (y0 + mc->chunk_size, mc->layout.height); y++, rows++)
        convert_srgb_to_srgb_linear (mc->data + gdk_memory_layout_offset (&mc->layout, 0, 0, y), mc->layout.width);
    }

  ADD_MARK (before,
            "Color state convert srgb->srgb-linear (thread)", "size %lux%lu, %lu rows",
            mc->layout.width, mc->layout.height, rows);
}

static void
gdk_memory_convert_color_state_srgb_linear_to_srgb (gpointer data)
{
  MemoryConvertColorState *mc = data;
  gsize y0, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  for (y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size), rows = 0;
       y0 < mc->layout.height;
       y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size))
    {
      for (y = y0; y < MIN (y0 + mc->chunk_size, mc->layout.height); y++, rows++)
        convert_srgb_linear_to_srgb (mc->data + gdk_memory_layout_offset (&mc->layout, 0, 0, y), mc->layout.width);
    }

  ADD_MARK (before,
            "Color state convert srgb-linear->srgb (thread)", "size %lux%lu, %lu rows",
            mc->layout.width, mc->layout.height, rows);
}

static void
gdk_memory_convert_color_state_generic (gpointer user_data)
{
  MemoryConvertColorState *mc = user_data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mc->layout.format];
  GdkFloatColorConvert convert_func = NULL;
  GdkFloatColorConvert convert_func2 = NULL;
  float (*tmp)[4];
  gsize y0, y;
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

  tmp = g_malloc (sizeof (*tmp) * mc->layout.width * desc->block_size.height);

  for (y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size), rows = 0;
       y0 < mc->layout.height;
       y0 = g_atomic_int_add (&mc->rows_done, mc->chunk_size))
    {
      for (y = y0; y < MIN (y0 + mc->chunk_size, mc->layout.height); y++, rows++)
        {
          float (*row)[4] = &tmp[mc->layout.width * (y % desc->block_size.height)];

          desc->to_float (row, mc->data, &mc->layout, y);

          if (desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED)
            unpremultiply (row, mc->layout.width);

          if (convert_func)
            convert_func (mc->src_cs, row, mc->layout.width);

          if (convert_func2)
            convert_func2 (mc->dest_cs, row, mc->layout.width);

          if (desc->alpha == GDK_MEMORY_ALPHA_PREMULTIPLIED)
            premultiply (row, mc->layout.width);

          if (y % desc->block_size.height == desc->block_size.height - 1)
            desc->from_float (mc->data, &mc->layout, row, y - (desc->block_size.height - 1));
        }
    }

  g_free (tmp);

  ADD_MARK (before,
            "Color state convert (thread)", "size %lux%lu, %lu rows",
            mc->layout.width, mc->layout.height, rows);
}

void
gdk_memory_convert_color_state (guchar                *data,
                                const GdkMemoryLayout *layout,
                                GdkColorState         *src_color_state,
                                GdkColorState         *dest_color_state)
{
  MemoryConvertColorState mc = {
    .data = data,
    .layout = *layout,
    .src_cs = src_color_state,
    .dest_cs = dest_color_state,
    .chunk_size = round_up (MAX (1, 512 / layout->width), gdk_memory_format_get_block_height (layout->format)),
  };
  guint n_tasks;

  if (gdk_color_state_equal (src_color_state, dest_color_state))
    return;

  n_tasks = (mc.layout.height + mc.chunk_size - 1) / mc.chunk_size;

  if (mc.layout.format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED &&
      src_color_state == GDK_COLOR_STATE_SRGB &&
      dest_color_state == GDK_COLOR_STATE_SRGB_LINEAR)
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_srgb_to_srgb_linear, &mc, n_tasks);
    }
  else if (mc.layout.format == GDK_MEMORY_B8G8R8A8_PREMULTIPLIED &&
           src_color_state == GDK_COLOR_STATE_SRGB_LINEAR &&
           dest_color_state == GDK_COLOR_STATE_SRGB)
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_srgb_linear_to_srgb, &mc, n_tasks);
    }
  else
    {
      gdk_parallel_task_run (gdk_memory_convert_color_state_generic, &mc, n_tasks);
    }
}

typedef struct _MipmapData MipmapData;

struct _MipmapData
{
  guchar          *dest;
  GdkMemoryLayout  dest_layout;
  const guchar    *src;
  GdkMemoryLayout  src_layout;
  guint            lod_level;
  gboolean         linear;

  gint             rows_done;
};

static void
gdk_memory_mipmap_same_format_nearest (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_layout.format];
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_layout.height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      guchar *dest = mipmap->dest + gdk_memory_layout_offset (&mipmap->dest_layout, 0, 0, (y >> mipmap->lod_level));

      desc->mipmap_nearest (dest,
                            mipmap->src, &mipmap->src_layout,
                            y,
                            mipmap->lod_level);
    }

  ADD_MARK (before,
            "Mipmap nearest (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_layout.width, mipmap->src_layout.height, mipmap->lod_level, rows);
}

static void
gdk_memory_mipmap_same_format_linear (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_layout.format];
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_layout.height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      guchar *dest = mipmap->dest + gdk_memory_layout_offset (&mipmap->dest_layout, 0, 0, (y >> mipmap->lod_level));

      desc->mipmap_linear (dest,
                           mipmap->src, &mipmap->src_layout,
                           y,
                           mipmap->lod_level);
    }

  ADD_MARK (before,
            "Mipmap linear (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_layout.width, mipmap->src_layout.height, mipmap->lod_level, rows);
}

static void
gdk_memory_mipmap_generic (gpointer data)
{
  MipmapData *mipmap = data;
  const GdkMemoryFormatDescription *desc = &memory_formats[mipmap->src_layout.format];
  FastConversionFunc func;
  gsize dest_width;
  GdkMemoryLayout tmp_layout;
  guchar *tmp;
  gsize n, y;
  guint64 before = GDK_PROFILER_CURRENT_TIME;
  gsize rows;

  n = 1 << mipmap->lod_level;
  dest_width = (mipmap->src_layout.width + n - 1) >> mipmap->lod_level;
  gdk_memory_layout_init (&tmp_layout,
                          desc->mipmap_format,
                          dest_width,
                          gdk_memory_format_get_block_height (desc->mipmap_format),
                          1);
  tmp = g_malloc (tmp_layout.size);
  func = get_fast_conversion_func (mipmap->dest_layout.format, desc->mipmap_format);

  for (y = g_atomic_int_add (&mipmap->rows_done, n), rows = 0;
       y < mipmap->src_layout.height;
       y = g_atomic_int_add (&mipmap->rows_done, n), rows++)
    {
      if (mipmap->linear)
        desc->mipmap_linear (tmp,
                             mipmap->src, &mipmap->src_layout,
                             y,
                             mipmap->lod_level);
      else
        desc->mipmap_nearest (tmp,
                              mipmap->src, &mipmap->src_layout,
                              y,
                              mipmap->lod_level);
      if (func)
        {
          guchar *dest = mipmap->dest + gdk_memory_layout_offset (&mipmap->dest_layout, 0, 0, (y >> mipmap->lod_level));

          func (dest, tmp, dest_width);
        }
      else
        {
          GdkMemoryLayout sub;
          gdk_memory_layout_init_sublayout (&sub,
                                            &mipmap->dest_layout,
                                            &(cairo_rectangle_int_t) {
                                                0,
                                                y >> mipmap->lod_level, 
                                                mipmap->dest_layout.width,
                                                1
                                            });
          gdk_memory_convert (mipmap->dest,
                              &sub,
                              GDK_COLOR_STATE_SRGB,
                              tmp,
                              &tmp_layout,
                              GDK_COLOR_STATE_SRGB);
        }
    }

  g_free (tmp);

  ADD_MARK (before,
            "Mipmap generic (thread)", "size %lux%lu, lod %u, %lu rows",
            mipmap->src_layout.width, mipmap->src_layout.height, mipmap->lod_level, rows);
}

void
gdk_memory_mipmap (guchar                *dest,
                   const GdkMemoryLayout *dest_layout,
                   const guchar          *src,
                   const GdkMemoryLayout *src_layout,
                   guint                  lod_level,
                   gboolean               linear)
{
  MipmapData mipmap = {
    .dest = dest,
    .dest_layout = *dest_layout,
    .src = src,
    .src_layout = *src_layout,
    .lod_level = lod_level,
    .linear = linear,
    .rows_done = 0,
  };
  gsize chunk_size;
  guint n_tasks;

  g_assert (lod_level > 0);

  chunk_size = MAX (1, 512 / src_layout->width);
  n_tasks = (src_layout->height + chunk_size - 1) / chunk_size;

  if (memory_formats[dest_layout->format].mipmap_format == src_layout->format)
    {
      if (linear)
        gdk_parallel_task_run (gdk_memory_mipmap_same_format_linear, &mipmap, n_tasks);
      else
        gdk_parallel_task_run (gdk_memory_mipmap_same_format_nearest, &mipmap, n_tasks);
    }
  else
    {
      gdk_parallel_task_run (gdk_memory_mipmap_generic, &mipmap, n_tasks);
    }
}

