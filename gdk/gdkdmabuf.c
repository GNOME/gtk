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
};

static GdkDrmFormatInfo supported_formats[] = {
  { DRM_FORMAT_ARGB8888, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED, GDK_MEMORY_A8R8G8B8 },
  { DRM_FORMAT_RGBA8888, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED, GDK_MEMORY_R8G8B8A8 },
  { DRM_FORMAT_BGRA8888, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED, GDK_MEMORY_B8G8R8A8 },
  { DRM_FORMAT_ABGR16161616F, GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED, GDK_MEMORY_R16G16B16A16_FLOAT },
  { DRM_FORMAT_RGB888, GDK_MEMORY_R8G8B8, GDK_MEMORY_R8G8B8 },
  { DRM_FORMAT_BGR888, GDK_MEMORY_B8G8R8, GDK_MEMORY_B8G8R8 },
};

static GdkDrmFormatInfo *
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
  GdkDrmFormatInfo *info;

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

  if (dmabuf->n_planes > 1)
    {
      g_set_error_literal (error,
                           GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_CREATION_FAILED,
                           "Multiplanar dmabufs are not supported");
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
  const GdkDmabuf *dmabuf;
  gsize size;
  unsigned int height;
  gsize src_stride;
  guchar *src_data;
  int bpp;

  GDK_DEBUG (DMABUF, "Using mmap() and memcpy() for downloading a dmabuf");

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  height = gdk_texture_get_height (texture);
  bpp = gdk_memory_format_bytes_per_pixel (gdk_texture_get_format (texture));

  src_stride = dmabuf->planes[0].stride;
  size = dmabuf->planes[0].stride * height;

  if (ioctl (dmabuf->planes[0].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dma-buf: %s", g_strerror (errno));

  src_data = mmap (NULL, size, PROT_READ, MAP_SHARED, dmabuf->planes[0].fd, dmabuf->planes[0].offset);

  if (stride == src_stride)
    memcpy (data, src_data, size);
  else
    {
      for (unsigned int i = 0; i < height; i++)
        memcpy (data + i * stride, src_data + i * src_stride, height * bpp);
    }

  munmap (src_data, size);

  if (ioctl (dmabuf->planes[0].fd, DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dma-buf: %s", g_strerror (errno));
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
