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

#include "gdkdataformatprivate.h"
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
  g_return_if_fail (sizes[0] >= dmabuf->planes[0].offset + gdk_memory_format_min_buffer_size (dst_format, src_stride, width, height));

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

static const GdkDrmFormatInfo *
get_drm_format_info (guint32 fourcc);

static void
download_data_format (guchar          *dst_data,
                      gsize            dst_stride,
                      GdkMemoryFormat  dst_format,
                      gsize            width,
                      gsize            height,
                      const GdkDmabuf *dmabuf,
                      const guchar    *src_data[GDK_DMABUF_MAX_PLANES],
                      gsize            sizes[GDK_DMABUF_MAX_PLANES])
{
  GdkDataBuffer buffer = {
    .width = width,
    .height = height,
    .planes = {
      { src_data[0] + dmabuf->planes[0].offset, dmabuf->planes[0].stride },
      { src_data[1] + dmabuf->planes[1].offset, dmabuf->planes[1].stride },
      { src_data[2] + dmabuf->planes[2].offset, dmabuf->planes[2].stride },
      { src_data[3] + dmabuf->planes[3].offset, dmabuf->planes[3].stride }
    },
  };

  if (!gdk_data_format_find_by_dmabuf_fourcc (dmabuf->fourcc, &buffer.format))
    {
      g_assert_not_reached ();
    }

  gdk_data_convert (&buffer,
                    dst_data,
                    dst_stride);
}

static const GdkDrmFormatInfo supported_formats[] = {
#if 0
  /* palette formats?! */
  {
    .fourcc = DRM_FORMAT_C1,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_C2,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_C4,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_C8,
    .download = NULL,
  },
#endif
  /* darkness */
  {
    .fourcc = DRM_FORMAT_D1,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_D2,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_D4,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_D8,
    .download = NULL,
  },
  /* red only - we treat this as gray */
  {
    .fourcc = DRM_FORMAT_R1,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R2,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R4,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R8,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R10,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R12,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_R16,
    .download = NULL,
  },
  /* 2 channels - FIXME: Should this be gray + alpha? */
  {
    .fourcc = DRM_FORMAT_RG88,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_GR88,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RG1616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_GR1616,
    .download = NULL,
  },
  /* <8bit per channel RGB(A) */
  {
    .fourcc = DRM_FORMAT_RGB332,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGR233,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XRGB4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XBGR4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBX4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRX4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ARGB4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ABGR4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBA4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRA4444,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XRGB1555,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XBGR1555,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBX5551,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRX5551,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ARGB1555,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ABGR1555,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBA5551,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRA5551,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGB565,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGR565,
    .download = NULL,
  },
  /* 8bit RGB */
  {
    .fourcc = DRM_FORMAT_RGB888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_BGR888,
    .download = download_memcpy,
  },
  /* 8bit RGBA */
  {
    .fourcc = DRM_FORMAT_BGRA8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_ABGR8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_ARGB8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_RGBA8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_BGRX8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_XBGR8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_XRGB8888,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_RGBX8888,
    .download = download_memcpy,
  },
  /* 10bit RGB(A) */
  {
    .fourcc = DRM_FORMAT_XRGB2101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XBGR2101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBX1010102,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRX1010102,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ARGB2101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ABGR2101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGBA1010102,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGRA1010102,
    .download = NULL,
  },
  /* 16bit RGB(A) */
  {
    .fourcc = DRM_FORMAT_XRGB16161616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XBGR16161616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ARGB16161616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ABGR16161616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XRGB16161616F,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XBGR16161616F,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ARGB16161616F,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_ABGR16161616F,
    .download = download_memcpy,
  },
  {
    .fourcc = DRM_FORMAT_AXBXGXRX106106106106,
    .download = NULL,
  },
  /* 1-plane YUV formats */
  {
    .fourcc = DRM_FORMAT_YUYV,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVYU,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_VYUY,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_UYVY,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_AYUV,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_AVUY8888,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XYUV8888,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XVUY8888,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_VUY888,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_VUY101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y210,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y212,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y216,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y410,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y412,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y416,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XVYU2101010,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XVYU12_16161616,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_XVYU16161616,
    .download = NULL,
  },
  /* tiled YUV */
  {
    .fourcc = DRM_FORMAT_Y0L0,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_X0L0,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Y0L2,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_X0L2,
    .download = NULL,
  },
  /* non-linear YUV */
  {
    .fourcc = DRM_FORMAT_YUV420_8BIT,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_YUV420_10BIT,
    .download = NULL,
  },
  /* 2 plane RGB + A */
  {
    .fourcc = DRM_FORMAT_BGRX8888_A8,
    .download = download_memcpy_3_1,
  },
  {
    .fourcc = DRM_FORMAT_RGBX8888_A8,
    .download = download_memcpy_3_1,
  },
  {
    .fourcc = DRM_FORMAT_XBGR8888_A8,
    .download = download_memcpy_3_1,
  },
  {
    .fourcc = DRM_FORMAT_XRGB8888_A8,
    .download = download_memcpy_3_1,
  },
  {
    .fourcc = DRM_FORMAT_RGB888_A8,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGR888_A8,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_RGB565_A8,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_BGR565_A8,
    .download = NULL,
  },
  /* 2-plane YUV formats */
  {
    .fourcc = DRM_FORMAT_NV12,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV21,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV16,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV61,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV24,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV42,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_NV15,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_P210,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_P010,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_P012,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_P016,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_P030,
    .download = NULL,
  },
  /* 3-plane YUV */
  {
    .fourcc = DRM_FORMAT_Q410,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_Q401,
    .download = NULL,
  },
  {
    .fourcc = DRM_FORMAT_YUV410,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVU410,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YUV411,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVU411,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YUV420,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVU420,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YUV422,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVU422,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YUV444,
    .download = download_data_format,
  },
  {
    .fourcc = DRM_FORMAT_YVU444,
    .download = download_data_format,
  },
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

GdkDmabufFormats *
gdk_dmabuf_get_mmap_formats (void)
{
  static GdkDmabufFormats *formats = NULL;

  if (formats == NULL)
    {
      GdkDmabufFormatsBuilder *builder;
      gsize i;

      builder = gdk_dmabuf_formats_builder_new ();

      for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
        {
          if (!supported_formats[i].download)
            continue;

          GDK_DEBUG (DMABUF,
                     "mmap advertises dmabuf format %.4s::%016" G_GINT64_MODIFIER "x",
                     (char *) &supported_formats[i].fourcc, (guint64) DRM_FORMAT_MOD_LINEAR);

          gdk_dmabuf_formats_builder_add_format (builder,
                                                 supported_formats[i].fourcc,
                                                 DRM_FORMAT_MOD_LINEAR);
        }

      formats = gdk_dmabuf_formats_builder_free_to_formats (builder);
    }

  return formats;
}

static gboolean
gdk_dmabuf_do_download_mmap (GdkTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  const GdkDrmFormatInfo *info;
  const GdkDmabuf *dmabuf;
  const guchar *src_data[GDK_DMABUF_MAX_PLANES];
  gsize sizes[GDK_DMABUF_MAX_PLANES];
  gsize needs_unmap[GDK_DMABUF_MAX_PLANES] = { FALSE, };
  gsize i, j;
  gboolean retval = FALSE;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));

  if (dmabuf->modifier != DRM_FORMAT_MOD_LINEAR)
    return FALSE;

  info = get_drm_format_info (dmabuf->fourcc);

  if (!info || !info->download)
    return FALSE;

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

      if (gdk_dmabuf_ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));

      src_data[i] = mmap (NULL, sizes[i], PROT_READ, MAP_SHARED, dmabuf->planes[i].fd, 0);
      if (src_data[i] == NULL || src_data[i] == MAP_FAILED)
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

  retval = TRUE;

  GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (texture)), DMABUF,
                     "Used mmap for downloading %dx%d dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                     gdk_texture_get_width (texture), gdk_texture_get_height (texture),
                     (char *)&dmabuf->fourcc, dmabuf->modifier);

out:
  for (i = 0; i < dmabuf->n_planes; i++)
    {
      if (!needs_unmap[i])
        continue;

      munmap ((void *)src_data[i], sizes[i]);

      if (gdk_dmabuf_ioctl (dmabuf->planes[i].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
        g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));
    }

  return retval;
}

gboolean
gdk_dmabuf_download_mmap (GdkTexture      *texture,
                          GdkMemoryFormat  format,
                          GdkColorState   *color_state,
                          guchar          *data,
                          gsize            stride)
{
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);
  GdkColorState *src_color_state = gdk_texture_get_color_state (texture);
  gboolean retval;

  if (format == src_format)
    {
      retval = gdk_dmabuf_do_download_mmap (texture, data, stride);
      gdk_memory_convert_color_state (data, stride, format,
                                      src_color_state, color_state,
                                      gdk_texture_get_width (texture),
                                      gdk_texture_get_height (texture));
    }
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = width * gdk_memory_format_bytes_per_pixel (src_format);
      src_data = g_new (guchar, src_stride * height);

      retval = gdk_dmabuf_do_download_mmap (texture, src_data, src_stride);

      gdk_memory_convert (data, stride, format, color_state,
                          src_data, src_stride, src_format, src_color_state,
                          width, height);

      g_free (src_data);
    }

  return retval;
}

int
gdk_dmabuf_ioctl (int            fd,
                  unsigned long  request,
                  void          *arg)
{
  int ret;

  do {
    ret = ioctl (fd, request, arg);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

  return ret;
}

#if !defined(DMA_BUF_IOCTL_IMPORT_SYNC_FILE)
struct dma_buf_import_sync_file
{
  __u32 flags;
  __s32 fd;
};
#define DMA_BUF_IOCTL_IMPORT_SYNC_FILE _IOW(DMA_BUF_BASE, 3, struct dma_buf_import_sync_file)
#endif

gboolean
gdk_dmabuf_import_sync_file (int     dmabuf_fd,
                             guint32 flags,
                             int     sync_file_fd)
{
  struct dma_buf_import_sync_file data = {
    .flags = flags,
    .fd = sync_file_fd,
  };

  if (gdk_dmabuf_ioctl (dmabuf_fd, DMA_BUF_IOCTL_IMPORT_SYNC_FILE, &data) != 0)
    {
      GDK_DEBUG (DMABUF, "Importing dmabuf sync failed: %s", g_strerror (errno));
      return FALSE;
    }
  
  return TRUE;
}

#if !defined(DMA_BUF_IOCTL_EXPORT_SYNC_FILE)
struct dma_buf_export_sync_file
{
  __u32 flags;
  __s32 fd;
};
#define DMA_BUF_IOCTL_EXPORT_SYNC_FILE _IOWR(DMA_BUF_BASE, 2, struct dma_buf_export_sync_file)
#endif

int
gdk_dmabuf_export_sync_file (int     dmabuf_fd,
                             guint32 flags)
{
  struct dma_buf_export_sync_file data = {
    .flags = flags,
    .fd = -1,
  };

  if (gdk_dmabuf_ioctl (dmabuf_fd, DMA_BUF_IOCTL_EXPORT_SYNC_FILE, &data) != 0)
    {
      GDK_DEBUG (DMABUF, "Exporting dmabuf sync failed: %s", g_strerror (errno));
      return -1;
    }

  return data.fd;
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
 * 1. Ignore non-linear modifiers.
 *
 * 2. Try and fix various inconsistencies between V4L and Mesa
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
  if (src->n_planes > GDK_DMABUF_MAX_PLANES)
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "GTK only support dmabufs with %u planes, not %u",
                   GDK_DMABUF_MAX_PLANES, src->n_planes);
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
 * 1 inode.
 * Multiple file descriptors may exist when the creator
 * of the dmabuf just dup()ed once for every plane...
 *
 * Returns: %TRUE if the dmabuf is disjoint
 **/
gboolean
gdk_dmabuf_is_disjoint (const GdkDmabuf *dmabuf)
{
  struct stat first_stat;
  unsigned i;

  /* First, do a fast check */

  for (i = 1; i < dmabuf->n_planes; i++)
    {
      if (dmabuf->planes[0].fd != dmabuf->planes[i].fd)
        break;
    }

  if (i == dmabuf->n_planes)
    return FALSE;

  /* We have different fds, do the fancy check instead */

  if (fstat (dmabuf->planes[0].fd, &first_stat) != 0)
    return TRUE;

  for (i = 1; i < dmabuf->n_planes; i++)
    {
      struct stat plane_stat;

      if (fstat (dmabuf->planes[i].fd, &plane_stat) != 0)
        return TRUE;

      if (first_stat.st_ino != plane_stat.st_ino)
        return TRUE;
    }

  return FALSE;
}

#endif  /* HAVE_DMABUF */

void
gdk_dmabuf_close_fds (GdkDmabuf *dmabuf)
{
  guint i, j;

  for (i = 0; i < dmabuf->n_planes; i++)
    {
      for (j = 0; j < i; j++)
        {
          if (dmabuf->planes[i].fd == dmabuf->planes[j].fd)
            break;
        }
      if (i == j)
        g_close (dmabuf->planes[i].fd, NULL);
    }
}
