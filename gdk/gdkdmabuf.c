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
#include "gdkdmabuftextureprivate.h"
#include "gdkmemoryformatprivate.h"

#ifdef HAVE_LINUX_DMA_BUF_H
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>

typedef struct _GdkDrmFormatInfo GdkDrmFormatInfo;

struct _GdkDrmFormatInfo
{
  guint32 fourcc;
  GdkMemoryFormat premultiplied_memory_format;
  GdkMemoryFormat unpremultiplied_memory_format;
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
  gsize U, V;

  if (dmabuf->fourcc == DRM_FORMAT_NV21)
    {
      U = 1; V = 0;
    }
  else
    {
      U = 0; V = 1;
    }

  y_stride = dmabuf->planes[0].stride;
  y_data = src_data[0] + dmabuf->planes[0].offset;
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + height * y_stride);
  uv_stride = dmabuf->planes[1].stride;
  uv_data = src_data[1] + dmabuf->planes[1].offset;
  g_return_if_fail (sizes[1] >= dmabuf->planes[1].offset + (height + 1) / 2 * uv_stride);

  for (y = 0; y < height; y += 2)
    {
      guchar *dst2_data = dst_data + dst_stride;
      const guchar *y2_data = y_data + y_stride;

      for (x = 0; x < width; x += 2)
        {
          int r, g, b;

          get_uv_values (&itu601_wide, uv_data[x + U], uv_data[x + V], &r, &g, &b);

          set_rgb_values (&dst_data[3 * x], y_data[x], r, g, b);
          if (x + 1 < width)
            set_rgb_values (&dst_data[3 * (x + 1)], y_data[x], r, g, b);
          if (y + 1 < height)
            {
              set_rgb_values (&dst2_data[3 * x], y2_data[x], r, g, b);
              if (x + 1 < width)
                set_rgb_values (&dst2_data[3 * (x + 1)], y2_data[x], r, g, b);
            }
        }
      dst_data += 2 * dst_stride;
      y_data += 2 * y_stride;
      uv_data += uv_stride;
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
  { DRM_FORMAT_BGRA8888, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, GDK_MEMORY_A8R8G8B8, download_memcpy },
  { DRM_FORMAT_ABGR8888, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, GDK_MEMORY_R8G8B8A8, download_memcpy },
  { DRM_FORMAT_ARGB8888, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, GDK_MEMORY_B8G8R8A8, download_memcpy },
  { DRM_FORMAT_RGBA8888, GDK_MEMORY_A8B8G8R8_PREMULTIPLIED, GDK_MEMORY_A8B8G8R8, download_memcpy },
  { DRM_FORMAT_BGRX8888, GDK_MEMORY_X8R8G8B8, GDK_MEMORY_X8R8G8B8, download_memcpy },
  { DRM_FORMAT_XBGR8888, GDK_MEMORY_R8G8B8X8, GDK_MEMORY_R8G8B8X8, download_memcpy },
  { DRM_FORMAT_XRGB8888, GDK_MEMORY_B8G8R8X8, GDK_MEMORY_B8G8R8X8, download_memcpy },
  { DRM_FORMAT_RGBX8888, GDK_MEMORY_X8B8G8R8, GDK_MEMORY_X8B8G8R8, download_memcpy },
  { DRM_FORMAT_ABGR16161616F, GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, GDK_MEMORY_R16G16B16A16_FLOAT, download_memcpy },
  { DRM_FORMAT_RGB888, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_memcpy },
  { DRM_FORMAT_BGR888, GDK_MEMORY_B8G8R8, GDK_MEMORY_B8G8R8, download_memcpy },
  /* YUV formats */
  { DRM_FORMAT_NV12, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_NV21, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_nv12 },
  { DRM_FORMAT_YUYV, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_YVYU, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_VYUY, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_yuyv },
  { DRM_FORMAT_UYVY, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8, download_yuyv },
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

static void
gdk_dmabuf_direct_downloader_add_formats (const GdkDmabufDownloader *downloader,
                                          GdkDisplay                *display,
                                          GdkDmabufFormatsBuilder   *builder)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      gdk_dmabuf_formats_builder_add_format (builder,
                                             supported_formats[i].fourcc,
                                             DRM_FORMAT_MOD_LINEAR);
    }
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

  if (!info)
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

  *out_format = premultiplied ? info->premultiplied_memory_format
                              : info->unpremultiplied_memory_format;

  return TRUE;
}

static void
gdk_dmabuf_direct_downloader_do_download (GdkTexture *texture,
                                          guchar     *data,
                                          gsize       stride)
{
  const GdkDrmFormatInfo *info;
  const GdkDmabuf *dmabuf;
  const guchar *src_data[GDK_DMABUF_MAX_PLANES];
  gsize sizes[GDK_DMABUF_MAX_PLANES];
  gsize needs_unmap[GDK_DMABUF_MAX_PLANES] = { FALSE, };
  gsize i, j;

  GDK_DEBUG (DMABUF, "Using mmap() and memcpy() for downloading a dmabuf");

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  info = get_drm_format_info (dmabuf->fourcc);

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

      sizes[i] = lseek (dmabuf->planes[0].fd, 0, SEEK_END);
      if (sizes[i] == (off_t) -1)
        {
          g_warning ("Failed to seek dmabuf: %s", g_strerror (errno));
          goto out;
        }

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

      munmap (src_data, sizes[i]);

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
    gdk_dmabuf_direct_downloader_do_download (texture, data, stride);
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = width * gdk_memory_format_bytes_per_pixel (src_format);
      src_data = g_new (guchar, src_stride * height);

      gdk_dmabuf_direct_downloader_do_download (texture, src_data, src_stride);

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
 * 1. Treat the INVALID modifier the same as LINEAR.
 *
 * 2. Ignore all other modifiers.
 *
 * 3. Try and fix various inconsistencies between V4L and Mesa,
 *    like NV12.
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

  if (src->modifier && src->modifier != DRM_FORMAT_MOD_INVALID)
    return TRUE;

  switch (dest->fourcc)
    {
      case DRM_FORMAT_NV12:
        if (dest->n_planes == 1)
          {
            dest->n_planes = 2;
            dest->planes[1].fd = dest->planes[0].fd;
            dest->planes[1].stride = dest->planes[0].stride;
            dest->planes[1].offset = dest->planes[0].offset + dest->planes[0].stride * height;
          }
        break;

      default:
        break;
    }

  return TRUE;
}

#endif  /* HAVE_LINUX_DMA_BUF_H */
