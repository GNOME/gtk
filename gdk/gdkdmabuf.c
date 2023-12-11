/* gdkdmabuf.c
 *
 * Copyright 2023  Red Hat, Inc.
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

#include "config.h"

#include "gdkdmabufprivate.h"

#include "gdkdebugprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkmemoryformatprivate.h"

#ifdef HAVE_DMABUF
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <epoxy/egl.h>

typedef struct _GdkDrmFormatInfo GdkDrmFormatInfo;

struct _GdkDrmFormatInfo
{
  guint32 fourcc;
  GdkMemoryFormat memory_format;
  void (* download) (guchar          *dst_data,
                     gsize            dst_stride,
                     GdkMemoryFormat  dst_format,
                     gsize            width,
                     gsize            height,
                     const GdkDmabuf *dmabuf,
                     const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                     gsize            sizes[GDK_DMABUF_MAX_PLANES]);
};

static void
download_memcpy (guchar          *dst_data,
                 gsize            dst_stride,
                 GdkMemoryFormat  dst_format,
                 gsize            width,
                 gsize            height,
                 const GdkDmabuf *dmabuf,
                 const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                 gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *src_data;
  gsize src_stride;
  guint bpp;

  bpp = gdk_memory_format_bytes_per_pixel (dst_format);
  src_stride = dmabuf->planes[0].stride;
  src_data = src_datas[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + (height - 1) * dst_stride + width * bpp);

  if (dst_stride == src_stride)
    memcpy (dst_data, src_data, (height - 1) * dst_stride + width * bpp);
  else
    {
      gsize i;

      for (i = 0; i < height; i++)
        memcpy (dst_data + i * dst_stride, src_data + i * src_stride, width * bpp);
    }
}

static void
download_memcpy_3_1 (guchar          *dst_data,
                     gsize            dst_stride,
                     GdkMemoryFormat  dst_format,
                     gsize            width,
                     gsize            height,
                     const GdkDmabuf *dmabuf,
                     const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
                     gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  guint a;
  guchar *dst_row;
  const guchar *src_data, *src_row;
  gsize src_stride;

  g_assert (dmabuf->n_planes == 2);

  download_memcpy (dst_data, dst_stride, dst_format, width, height, dmabuf, src_datas, sizes);

  switch ((int)dst_format)
    {
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      a = 0;
      break;
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      a = 3;
      break;
    default:
      g_assert_not_reached ();
    }

  src_stride = dmabuf->planes[1].stride;
  src_data = src_datas[1];

  for (gsize y = 0; y < height; y++)
    {
      dst_row = dst_data + y * dst_stride;
      src_row = src_data + y * src_stride;
      for (gsize x = 0; x < width; x++)
        dst_row[4 * x + a] = src_row[x];
    }
}

typedef struct _YUVCoefficients YUVCoefficients;

struct _YUVCoefficients
{
  int v_to_r;
  int u_to_g;
  int v_to_g;
  int u_to_b;
};

/* multiplied by 65536 */
//static const YUVCoefficients itu601_narrow = { 104597, -25675, -53279, 132201 };
static const YUVCoefficients itu601_wide = { 74711, -25864, -38050, 133176 };

static inline void
get_uv_values (const YUVCoefficients *coeffs,
               guint8                 u,
               guint8                 v,
               int                   *out_r,
               int                   *out_g,
               int                   *out_b)
{
  int u2 = (int) u - 127;
  int v2 = (int) v - 127;
  *out_r = coeffs->v_to_r * v2;
  *out_g = coeffs->u_to_g * u2 + coeffs->v_to_g * v2;
  *out_b = coeffs->u_to_b * u2;
}

static inline void
set_rgb_values (guint8 rgb[3],
                guint8 y,
                int    r,
                int    g,
                int    b)
{
  int y2 = y * 65536;

  rgb[0] = CLAMP ((y2 + r) >> 16, 0, 255);
  rgb[1] = CLAMP ((y2 + g) >> 16, 0, 255);
  rgb[2] = CLAMP ((y2 + b) >> 16, 0, 255);
}

static void
download_nv12 (guchar          *dst_data,
               gsize            dst_stride,
               GdkMemoryFormat  dst_format,
               gsize            width,
               gsize            height,
               const GdkDmabuf *dmabuf,
               const guchar    *src_data[GDK_DMABUF_MAX_PLANES],
               gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *y_data, *uv_data;
  gsize x, y, y_stride, uv_stride;
  gsize U, V, X_SUB, Y_SUB;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_NV12:
      U = 0; V = 1; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_NV21:
      U = 1; V = 0; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_NV16:
      U = 0; V = 1; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV61:
      U = 1; V = 0; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV24:
      U = 0; V = 1; X_SUB = 1; Y_SUB = 1;
      break;
    case DRM_FORMAT_NV42:
      U = 1; V = 0; X_SUB = 1; Y_SUB = 1;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  y_stride = dmabuf->planes[0].stride;
  y_data = src_data[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * y_stride);
  uv_stride = dmabuf->planes[1].stride;
  uv_data = src_data[1] + dmabuf->planes[1].offset;
  g_return_if_fail (sizes[1] >= dmabuf->planes[1].offset + (height + Y_SUB - 1) / Y_SUB * uv_stride);

  for (y = 0; y < height; y += Y_SUB)
    {
      for (x = 0; x < width; x += X_SUB)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_wide, uv_data[x / X_SUB * 2 + U], uv_data[x / X_SUB * 2 + V], &r, &g, &b);

          for (ys = 0; ys < Y_SUB && y + ys < height; ys++)
            for (xs = 0; xs < X_SUB && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += Y_SUB * dst_stride;
      y_data += Y_SUB * y_stride;
      uv_data += uv_stride;
    }
}

static void
download_yuv_3 (guchar          *dst_data,
                gsize            dst_stride,
                GdkMemoryFormat  dst_format,
                gsize            width,
                gsize            height,
                const GdkDmabuf *dmabuf,
                const guchar    *src_data[GDK_DMABUF_MAX_PLANES],
                gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *y_data, *u_data, *v_data;
  gsize x, y, y_stride, u_stride, v_stride;
  gsize U, V, X_SUB, Y_SUB;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_YUV410:
      U = 1; V = 2; X_SUB = 4; Y_SUB = 4;
      break;
    case DRM_FORMAT_YVU410:
      U = 2; V = 1; X_SUB = 4; Y_SUB = 4;
      break;
    case DRM_FORMAT_YUV411:
      U = 1; V = 2; X_SUB = 4; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU411:
      U = 2; V = 1; X_SUB = 4; Y_SUB = 1;
      break;
    case DRM_FORMAT_YUV420:
      U = 1; V = 2; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_YVU420:
      U = 2; V = 1; X_SUB = 2; Y_SUB = 2;
      break;
    case DRM_FORMAT_YUV422:
      U = 1; V = 2; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU422:
      U = 2; V = 1; X_SUB = 2; Y_SUB = 1;
      break;
    case DRM_FORMAT_YUV444:
      U = 1; V = 2; X_SUB = 1; Y_SUB = 1;
      break;
    case DRM_FORMAT_YVU444:
      U = 2; V = 1; X_SUB = 1; Y_SUB = 1;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  y_stride = dmabuf->planes[0].stride;
  y_data = src_data[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * y_stride);
  u_stride = dmabuf->planes[U].stride;
  u_data = src_data[U] + dmabuf->planes[U].offset;
  g_return_if_fail (sizes[U] >= dmabuf->planes[U].offset + (height + Y_SUB - 1) / Y_SUB * u_stride);
  v_stride = dmabuf->planes[V].stride;
  v_data = src_data[V] + dmabuf->planes[V].offset;
  g_return_if_fail (sizes[V] >= dmabuf->planes[V].offset + (height + Y_SUB - 1) / Y_SUB * v_stride);

  for (y = 0; y < height; y += Y_SUB)
    {
      for (x = 0; x < width; x += X_SUB)
        {
          int r, g, b;
          gsize xs, ys;

          get_uv_values (&itu601_wide, u_data[x / X_SUB], v_data[x / X_SUB], &r, &g, &b);

          for (ys = 0; ys < Y_SUB && y + ys < height; ys++)
            for (xs = 0; xs < X_SUB && x + xs < width; xs++)
              set_rgb_values (&dst_data[3 * (x + xs) + dst_stride * ys], y_data[x + xs + y_stride * ys], r, g, b);
        }
      dst_data += Y_SUB * dst_stride;
      y_data += Y_SUB * y_stride;
      u_data += u_stride;
      v_data += v_stride;
    }
}

static void
download_yuyv (guchar          *dst_data,
               gsize            dst_stride,
               GdkMemoryFormat  dst_format,
               gsize            width,
               gsize            height,
               const GdkDmabuf *dmabuf,
               const guchar    *src_datas[GDK_DMABUF_MAX_PLANES],
               gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  const guchar *src_data;
  gsize x, y, src_stride;
  gsize Y1, Y2, U, V;

  switch (dmabuf->fourcc)
    {
    case DRM_FORMAT_YUYV:
      Y1 = 0; U = 1; Y2 = 2; V = 3;
      break;
    case DRM_FORMAT_YVYU:
      Y1 = 0; V = 1; Y2 = 2; U = 3;
      break;
    case DRM_FORMAT_UYVY:
      U = 0; Y1 = 1; V = 2; Y2 = 3;
      break;
    case DRM_FORMAT_VYUY:
      V = 0; Y1 = 1; U = 2; Y2 = 3;
      break;
    default:
      g_assert_not_reached ();
      return;
    }

  src_stride = dmabuf->planes[0].stride;
  src_data = src_datas[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * src_stride);

  for (y = 0; y < height; y ++)
    {
      for (x = 0; x < width; x += 2)
        {
          int r, g, b;

          get_uv_values (&itu601_wide, src_data[2 * x + U], src_data[2 * x + V], &r, &g, &b);
          set_rgb_values (&dst_data[3 * x], src_data[2 * x + Y1], r, g, b);
          if (x + 1 < width)
            set_rgb_values (&dst_data[3 * (x + 1)], src_data[2 * x + Y2], r, g, b);
        }
      dst_data += dst_stride;
      src_data += src_stride;
    }
}

static const GdkDrmFormatInfo supported_formats[] = {
#if 0
  /* palette formats?! */
  { DRM_FORMAT_C1, GDK_MEMORY_, NULL },
  { DRM_FORMAT_C2, GDK_MEMORY_, NULL },
  { DRM_FORMAT_C4, GDK_MEMORY_, NULL },
  { DRM_FORMAT_C8, GDK_MEMORY_, NULL },
#endif
  /* darkness */
  { DRM_FORMAT_D1, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_D2, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_D4, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_D8, GDK_MEMORY_G8, NULL },
  /* red only - we treat this as gray */
  { DRM_FORMAT_R1, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_R2, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_R4, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_R8, GDK_MEMORY_G8, NULL },
  { DRM_FORMAT_R10, GDK_MEMORY_G16, NULL },
  { DRM_FORMAT_R12, GDK_MEMORY_G16, NULL },
  { DRM_FORMAT_R16, GDK_MEMORY_G16, NULL },
  /* 2 channels - FIXME: Should this be gray + alpha? */
  { DRM_FORMAT_RG88, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_GR88, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_RG1616, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_GR1616, GDK_MEMORY_R16G16B16, NULL },
  /* <8bit per channel RGB(A) */
  { DRM_FORMAT_RGB332, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_BGR233, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_XRGB4444, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_XBGR4444, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_RGBX4444, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_BGRX4444, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_ARGB4444, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_ABGR4444, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_RGBA4444, GDK_MEMORY_A8B8G8R8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_BGRA4444, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_XRGB1555, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_XBGR1555, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_RGBX5551, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_BGRX5551, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_ARGB1555, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_ABGR1555, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_RGBA5551, GDK_MEMORY_A8B8G8R8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_BGRA5551, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_RGB565, GDK_MEMORY_B8G8R8, NULL },
  { DRM_FORMAT_BGR565, GDK_MEMORY_R8G8B8, NULL },
  /* 8bit RGB */
  { DRM_FORMAT_RGB888, GDK_MEMORY_R8G8B8, download_memcpy },
  { DRM_FORMAT_BGR888, GDK_MEMORY_B8G8R8, download_memcpy },
  /* 8bit RGBA */
  { DRM_FORMAT_BGRA8888, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, download_memcpy },
  { DRM_FORMAT_ABGR8888, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, download_memcpy },
  { DRM_FORMAT_ARGB8888, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, download_memcpy },
  { DRM_FORMAT_RGBA8888, GDK_MEMORY_A8B8G8R8_PREMULTIPLIED, download_memcpy },
  { DRM_FORMAT_BGRX8888, GDK_MEMORY_X8R8G8B8, download_memcpy },
  { DRM_FORMAT_XBGR8888, GDK_MEMORY_R8G8B8X8, download_memcpy },
  { DRM_FORMAT_XRGB8888, GDK_MEMORY_B8G8R8X8, download_memcpy },
  { DRM_FORMAT_RGBX8888, GDK_MEMORY_X8B8G8R8, download_memcpy },
  { DRM_FORMAT_ABGR16161616F, GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, download_memcpy },
  /* 10bit RGB(A) */
  { DRM_FORMAT_XRGB2101010, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_XBGR2101010, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_RGBX1010102, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_BGRX1010102, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_ARGB2101010, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  { DRM_FORMAT_ABGR2101010, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  { DRM_FORMAT_RGBA1010102, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  { DRM_FORMAT_BGRA1010102, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  /* 16bit RGB(A) */
  { DRM_FORMAT_XRGB16161616, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_XBGR16161616, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_ARGB16161616, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  { DRM_FORMAT_ABGR16161616, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  { DRM_FORMAT_XRGB16161616F, GDK_MEMORY_R16G16B16_FLOAT, NULL },
  { DRM_FORMAT_XBGR16161616F, GDK_MEMORY_R16G16B16_FLOAT, NULL },
  { DRM_FORMAT_ARGB16161616F, GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, NULL },
  { DRM_FORMAT_ABGR16161616F, GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, NULL },
  { DRM_FORMAT_AXBXGXRX106106106106, GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, NULL },
  /* 1-plane YUV formats */
  { DRM_FORMAT_YUYV, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_YVYU, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_VYUY, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_UYVY, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_AYUV, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_AVUY8888, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_XYUV8888, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_XVUY8888, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_VUY888, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_VUY101010, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y210, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y212, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y216, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y410, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y412, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Y416, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_XVYU2101010, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_XVYU12_16161616, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_XVYU16161616, GDK_MEMORY_R16G16B16, NULL },
  /* tiled YUV */
  { DRM_FORMAT_Y0L0, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_X0L0, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_Y0L2, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_X0L2, GDK_MEMORY_R16G16B16, NULL },
  /* non-linear YUV */
  { DRM_FORMAT_YUV420_8BIT, GDK_MEMORY_R8G8B8, NULL },
  { DRM_FORMAT_YUV420_10BIT, GDK_MEMORY_R16G16B16, NULL },
  /* 2 plane RGB + A */
  { DRM_FORMAT_BGRX8888_A8, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, download_memcpy_3_1 },
  { DRM_FORMAT_RGBX8888_A8, GDK_MEMORY_A8B8G8R8_PREMULTIPLIED, download_memcpy_3_1 },
  { DRM_FORMAT_XBGR8888_A8, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, download_memcpy_3_1 },
  { DRM_FORMAT_XRGB8888_A8, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, download_memcpy_3_1 },
  { DRM_FORMAT_RGB888_A8, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_BGR888_A8, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_RGB565_A8, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, NULL },
  { DRM_FORMAT_BGR565_A8, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, NULL },
  /* 2-plane YUV formats */
  { DRM_FORMAT_NV12, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV21, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV16, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV61, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV24, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV42, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV15, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_P210, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_P010, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_P012, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_P016, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_P030, GDK_MEMORY_R16G16B16, NULL },
  /* 3-plane YUV */
  { DRM_FORMAT_Q410, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_Q401, GDK_MEMORY_R16G16B16, NULL },
  { DRM_FORMAT_YUV410, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YVU410, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YUV411, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YVU411, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YUV420, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YVU420, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YUV422, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YVU422, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YUV444, GDK_MEMORY_R8G8B8, download_yuv_3 },
  { DRM_FORMAT_YVU444, GDK_MEMORY_R8G8B8, download_yuv_3 },
};

static const GdkDrmFormatInfo *
get_drm_format_info (guint32 fourcc)
{
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (supported_formats[i].fourcc == fourcc)
        return &supported_formats[i];
    }

  return NULL;
}

gboolean
gdk_dmabuf_get_memory_format (guint32          fourcc,
                              gboolean         premultiplied,
                              GdkMemoryFormat *out_format)
{
  const GdkDrmFormatInfo *info = get_drm_format_info (fourcc);

  if (info == NULL)
    return FALSE;

  if (premultiplied)
    *out_format = gdk_memory_format_get_premultiplied (info->memory_format);
  else
    *out_format = gdk_memory_format_get_straight (info->memory_format);

  return TRUE;
}

gboolean
gdk_dmabuf_get_fourcc (GdkMemoryFormat  format,
                       guint32         *out_fourcc)
{
  /* we're not walking the list on purpose, so that we can keep track
   * of exact matches here. Certain DRM formats map to the same memory
   * format and we don't want the list to accidentally get mixed up.
   */
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
      *out_fourcc = DRM_FORMAT_ARGB8888;
      return TRUE;

    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8:
      *out_fourcc = DRM_FORMAT_BGRA8888;
      return TRUE;

    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8:
      *out_fourcc = DRM_FORMAT_RGBA8888;
      return TRUE;

    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8:
      *out_fourcc = DRM_FORMAT_ABGR8888;
      return TRUE;

    case GDK_MEMORY_R8G8B8:
      *out_fourcc = DRM_FORMAT_BGR888;
      return TRUE;

    case GDK_MEMORY_B8G8R8:
      *out_fourcc = DRM_FORMAT_RGB888;
      return TRUE;

    case GDK_MEMORY_B8G8R8X8:
      *out_fourcc = DRM_FORMAT_XRGB8888;
      return TRUE;

    case GDK_MEMORY_X8R8G8B8:
      *out_fourcc = DRM_FORMAT_BGRX8888;
      return TRUE;

    case GDK_MEMORY_R8G8B8X8:
      *out_fourcc = DRM_FORMAT_XBGR8888;
      return TRUE;

    case GDK_MEMORY_X8B8G8R8:
      *out_fourcc = DRM_FORMAT_RGBX8888;
      return TRUE;

    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      *out_fourcc = DRM_FORMAT_ABGR16161616F;
      return TRUE;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
      *out_fourcc = DRM_FORMAT_ABGR16161616;
      return TRUE;

    case GDK_MEMORY_G8:
      *out_fourcc = DRM_FORMAT_R8;
      return TRUE;

    case GDK_MEMORY_G16:
      *out_fourcc = DRM_FORMAT_R16;
      return TRUE;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return FALSE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gboolean
gdk_dmabuf_direct_downloader_add_formats (const GdkDmabufDownloader *downloader,
                                          GdkDisplay                *display,
                                          GdkDmabufFormatsBuilder   *builder)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      GDK_DISPLAY_DEBUG (display, DMABUF,
                         "%s dmabuf format %.4s:%#" G_GINT64_MODIFIER "x",
                         downloader->name,
                         (char *) &supported_formats[i].fourcc, (guint64) DRM_FORMAT_MOD_LINEAR);

      gdk_dmabuf_formats_builder_add_format (builder,
                                             supported_formats[i].fourcc,
                                             DRM_FORMAT_MOD_LINEAR);
    }

  return TRUE;
}

static gboolean
gdk_dmabuf_direct_downloader_supports (const GdkDmabufDownloader  *downloader,
                                       GdkDisplay                 *display,
                                       const GdkDmabuf            *dmabuf,
                                       gboolean                    premultiplied,
                                       GdkMemoryFormat            *out_format,
                                       GError                    **error)
{
  const GdkDrmFormatInfo *info;

  info = get_drm_format_info (dmabuf->fourcc);

  if (!info || !info->download)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format %.4s",
                   (char *) &dmabuf->fourcc);
      return FALSE;
    }

  if (dmabuf->modifier != DRM_FORMAT_MOD_LINEAR)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf modifier %#lx (only linear buffers are supported)",
                   dmabuf->modifier);
      return FALSE;
    }

  if (premultiplied)
    *out_format = gdk_memory_format_get_premultiplied (info->memory_format);
  else
    *out_format = gdk_memory_format_get_straight (info->memory_format);

  return TRUE;
}

static void
gdk_dmabuf_direct_downloader_do_download (const GdkDmabufDownloader *downloader,
                                          GdkTexture                *texture,
                                          guchar                    *data,
                                          gsize                      stride)
{
  const GdkDrmFormatInfo *info;
  const GdkDmabuf *dmabuf;
  const guchar *src_data[GDK_DMABUF_MAX_PLANES];
  gsize sizes[GDK_DMABUF_MAX_PLANES];
  gsize needs_unmap[GDK_DMABUF_MAX_PLANES] = { FALSE, };
  gsize i, j;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  info = get_drm_format_info (dmabuf->fourcc);

  g_return_if_fail (info && info->download);

  GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (texture)), DMABUF,
                     "Using %s for downloading a dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                     downloader->name, (char *)&dmabuf->fourcc, dmabuf->modifier);

  for (i = 0; i < dmabuf->n_planes; i++)
    {
      for (j = 0; j < i; j++)
        {
          if (dmabuf->planes[i].fd == dmabuf->planes[j].fd)
            break;
        }
      if (j < i)
        {
          src_data[i] = src_data[j];
          sizes[i] = sizes[j];
          continue;
        }

      sizes[i] = lseek (dmabuf->planes[i].fd, 0, SEEK_END);
      if (sizes[i] == (off_t) -1)
        {
          g_warning ("Failed to seek dmabuf: %s", g_strerror (errno));
          goto out;
        }
      /* be a good citizen and seek back to the start, as the docs recommend */
      lseek (dmabuf->planes[i].fd, 0, SEEK_SET);

      if (ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));

      src_data[i] = mmap (NULL, sizes[i], PROT_READ, MAP_SHARED, dmabuf->planes[i].fd, dmabuf->planes[i].offset);
      if (src_data[i] == NULL)
        {
          g_warning ("Failed to mmap dmabuf: %s", g_strerror (errno));
          goto out;
        }
      needs_unmap[i] = TRUE;
    }

    info->download (data,
                    stride,
                    gdk_texture_get_format (texture),
                    gdk_texture_get_width (texture),
                    gdk_texture_get_height (texture),
                    dmabuf,
                    src_data,
                    sizes);

out:
  for (i = 0; i < dmabuf->n_planes; i++)
    {
      if (!needs_unmap[i])
        continue;

      munmap ((void *)src_data[i], sizes[i]);

      if (ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));
    }
}

static void
gdk_dmabuf_direct_downloader_download (const GdkDmabufDownloader *downloader,
                                       GdkTexture                *texture,
                                       GdkMemoryFormat            format,
                                       guchar                    *data,
                                       gsize                      stride)
{
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);

  if (format == src_format)
    gdk_dmabuf_direct_downloader_do_download (downloader, texture, data, stride);
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = width * gdk_memory_format_bytes_per_pixel (src_format);
      src_data = g_new (guchar, src_stride * height);

      gdk_dmabuf_direct_downloader_do_download (downloader, texture, src_data, src_stride);

      gdk_memory_convert (data, stride, format,
                          src_data, src_stride, src_format,
                          width, height);

      g_free (src_data);
    }
}

const GdkDmabufDownloader *
gdk_dmabuf_get_direct_downloader (void)
{
  static const GdkDmabufDownloader downloader = {
    "mmap",
    gdk_dmabuf_direct_downloader_add_formats,
    gdk_dmabuf_direct_downloader_supports,
    gdk_dmabuf_direct_downloader_download,
  };

  return &downloader;
}

/* 
 * Tries to sanitize the dmabuf to conform to the values expected
 * by Vulkan/EGL which should also be the values expected by
 * Wayland compositors
 *
 * We put these sanitized values into the GdkDmabufTexture, by
 * sanitizing the input from GdkDmabufTextureBuilder, which are
 * controlled by the callers.
 *
 * Things we do here:
 *
 * 1. Disallow any dmabuf format that we do not know.
 *
 * 2. Ignore non-linear modifiers.
 *
 * 3. Try and fix various inconsistencies between V4L and Mesa
 *    for linear modifiers, like the e.g. single-plane NV12.
 *
 * *** WARNING ***
 *
 * This function is not absolutely perfect, you do not have a
 * perfect dmabuf afterwards.
 *
 * In particular, it doesn't check sizes.
 *
 * *** WARNING ***
 */
gboolean
gdk_dmabuf_sanitize (GdkDmabuf        *dest,
                     gsize             width,
                     gsize             height,
                     const GdkDmabuf  *src,
                     GError          **error)
{
  const GdkDrmFormatInfo *info;

  if (src->n_planes > GDK_DMABUF_MAX_PLANES)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "GTK only support dmabufs with %u planes, not %u",
                   GDK_DMABUF_MAX_PLANES, src->n_planes);
      return FALSE;
    }

  info = get_drm_format_info (src->fourcc);

  if (info == NULL)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format %.4s",
                   (char *) &src->fourcc);
      return FALSE;
    }

  *dest = *src;

  if (src->modifier)
    return TRUE;

  switch (dest->fourcc)
    {
      case DRM_FORMAT_NV12:
      case DRM_FORMAT_NV21:
      case DRM_FORMAT_NV16:
      case DRM_FORMAT_NV61:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 2;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
          }
        break;

      case DRM_FORMAT_NV24:
      case DRM_FORMAT_NV42:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 2;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride * 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
          }
        break;

      case DRM_FORMAT_YUV410:
      case DRM_FORMAT_YVU410:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 3) / 4;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * ((height + 3) / 4);
          }
        break;

      case DRM_FORMAT_YUV411:
      case DRM_FORMAT_YVU411:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 3) / 4;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      case DRM_FORMAT_YUV420:
      case DRM_FORMAT_YVU420:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 1) / 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * ((height + 1) / 2);
          }
        break;

      case DRM_FORMAT_YUV422:
      case DRM_FORMAT_YVU422:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = (dest->planes[0].stride + 1) / 2;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      case DRM_FORMAT_YUV444:
      case DRM_FORMAT_YVU444:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 3;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
            dest->planes[2].fd = dest->planes[1].fd;
            dest->planes[2].stride = dest->planes[1].stride;
            dest->planes[2].offset = dest->planes[1].offset + dest->planes[1].stride * height;
          }
        break;

      default:
        break;
    }

  return TRUE;
}

/*
 * gdk_dmabuf_is_disjoint:
 * @dmabuf: a sanitized GdkDmabuf
 *
 * A dmabuf is considered disjoint if it uses more than
 * 1 file descriptor.
 *
 * Returns: %TRUE if the dmabuf is disjoint
 **/
gboolean
gdk_dmabuf_is_disjoint (const GdkDmabuf *dmabuf)
{
  unsigned i;

  for (i = 1; i < dmabuf->n_planes; i++)
    {
      if (dmabuf->planes[0].fd != dmabuf->planes[i].fd)
        return TRUE;
    }

  return FALSE;
}

#endif  /* HAVE_DMABUF */
