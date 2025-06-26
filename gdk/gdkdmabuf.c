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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <linux/udmabuf.h>
#include <epoxy/egl.h>
#include <errno.h>

GdkDmabufFormats *
gdk_dmabuf_get_mmap_formats (void)
{
  static GdkDmabufFormats *formats = NULL;

  if (formats == NULL)
    {
      GdkDmabufFormatsBuilder *builder;
      guint32 fourcc;
      gsize i;

      builder = gdk_dmabuf_formats_builder_new ();

      for (i = 0; i < GDK_MEMORY_N_FORMATS; i++)
        {
          fourcc = gdk_memory_format_get_dmabuf_rgb_fourcc (i);
          if (fourcc != 0)
            {
              GDK_DEBUG (DMABUF,
                         "mmap advertises dmabuf format %.4s::%016" G_GINT64_MODIFIER "x as RGB",
                         (char *) &fourcc, (guint64) DRM_FORMAT_MOD_LINEAR);
              gdk_dmabuf_formats_builder_add_format (builder, fourcc, DRM_FORMAT_MOD_LINEAR);
            }
          fourcc = gdk_memory_format_get_dmabuf_yuv_fourcc (i);
          if (fourcc != 0)
            {
              GDK_DEBUG (DMABUF,
                         "mmap advertises dmabuf format %.4s::%016" G_GINT64_MODIFIER "x as YUV",
                         (char *) &fourcc, (guint64) DRM_FORMAT_MOD_LINEAR);
              gdk_dmabuf_formats_builder_add_format (builder, fourcc, DRM_FORMAT_MOD_LINEAR);
            }
        }

      formats = gdk_dmabuf_formats_builder_free_to_formats (builder);
    }

  return formats;
}

const guchar *
gdk_dmabuf_mmap (int    dmabuf_fd,
                 gsize *out_size)
{
  gsize size;
  const guchar *result;

  size = lseek (dmabuf_fd, 0, SEEK_END);
  if (size == (off_t) -1)
    {
      g_warning ("Failed to seek dmabuf: %s", g_strerror (errno));
      return NULL;
    }

  /* be a good citizen and seek back to the start, as the docs recommend */
  lseek (dmabuf_fd, 0, SEEK_SET);

  if (gdk_dmabuf_ioctl (dmabuf_fd,
                        DMA_BUF_IOCTL_SYNC,
                        &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
    {
      g_warning ("Failed to sync dmabuf before mmap(): %s", g_strerror (errno));
      /* not a fatal error, but might cause glitches */
    }

  result = mmap (NULL, size, PROT_READ, MAP_SHARED, dmabuf_fd, 0);
  if (result == NULL || result == MAP_FAILED)
    {
      g_warning ("Failed to mmap dmabuf: %s", g_strerror (errno));
      return NULL;
    }

  *out_size = size;

  return result;
}

void
gdk_dmabuf_munmap (int           dmabuf_fd,
                   const guchar *addr,
                   gsize         size)
{
  munmap ((void *) addr, size);

  if (gdk_dmabuf_ioctl (dmabuf_fd,
                        DMA_BUF_IOCTL_SYNC,
                        &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
    {
      g_warning ("Failed to sync dmabuf after munmap(): %s", g_strerror (errno));
      /* not a fatal error, but might cause glitches */
    }
}

static gboolean
gdk_dmabuf_do_download_mmap (GdkTexture            *texture,
                             guchar                *data,
                             const GdkMemoryLayout *layout,
                             GdkColorState         *color_state)
{
  const GdkDmabuf *dmabuf;
  GdkMemoryLayout dmabuf_layout;
  const guchar *src_data[GDK_DMABUF_MAX_PLANES];
  gsize sizes[GDK_DMABUF_MAX_PLANES];
  gsize needs_unmap[GDK_DMABUF_MAX_PLANES] = { FALSE, };
  gsize i, j;
  gboolean retval = FALSE;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));

  if (dmabuf->modifier != DRM_FORMAT_MOD_LINEAR)
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

      src_data[i] = gdk_dmabuf_mmap (dmabuf->planes[i].fd, &sizes[i]);
      if (src_data[i] == NULL)
        goto out;
      needs_unmap[i] = TRUE;
    }

  if (gdk_memory_layout_init_from_dmabuf (&dmabuf_layout,
                                          dmabuf,
                                          gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_PREMULTIPLIED,
                                          gdk_texture_get_width (texture),
                                          gdk_texture_get_height (texture)))
    {
      gdk_memory_convert (data,
                          layout,
                          gdk_texture_get_color_state (texture),
                          src_data[0],
                          &dmabuf_layout,
                          gdk_texture_get_color_state (texture));
      retval = TRUE;
    }

  GDK_DISPLAY_DEBUG (gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (texture)), DMABUF,
                     "Used mmap for downloading %dx%d dmabuf (format %.4s:%#" G_GINT64_MODIFIER "x)",
                     gdk_texture_get_width (texture), gdk_texture_get_height (texture),
                     (char *)&dmabuf->fourcc, dmabuf->modifier);

out:
  for (i = 0; i < dmabuf->n_planes; i++)
    {
      if (!needs_unmap[i])
        continue;

      gdk_dmabuf_munmap (dmabuf->planes[i].fd, src_data[i], sizes[i]);
    }

  return retval;
}

gboolean
gdk_dmabuf_download_mmap (GdkTexture            *texture,
                          guchar                *data,
                          const GdkMemoryLayout *layout,
                          GdkColorState         *color_state)
{
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);
  GdkColorState *src_color_state = gdk_texture_get_color_state (texture);
  gboolean retval;

  if (layout->format == src_format)
    {
      retval = gdk_dmabuf_do_download_mmap (texture, data, layout, color_state);
      gdk_memory_convert_color_state (data,
                                      layout,
                                      src_color_state,
                                      color_state);
    }
  else
    {
      GdkMemoryLayout src_layout;
      guchar *src_data;

      gdk_memory_layout_init (&src_layout,
                              src_format,
                              gdk_texture_get_width (texture),
                              gdk_texture_get_height (texture),
                              1);

      src_data = g_new (guchar, src_layout.size);

      retval = gdk_dmabuf_do_download_mmap (texture, src_data, &src_layout, src_color_state);

      gdk_memory_convert (data,
                          layout,
                          color_state,
                          src_data,
                          &src_layout,
                          src_color_state);

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

static int
udmabuf_initialize (GError **error)
{
  static int udmabuf_fd;

  if (udmabuf_fd == 0)
    {
      udmabuf_fd = open ("/dev/udmabuf", O_RDWR);
      if (udmabuf_fd == -1)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Failed to open /dev/udmabuf: %s",
                       g_strerror (errno));
        }
    }

  return udmabuf_fd;
}

#define align(x,y) (((x) + (y) - 1) & ~((y) - 1))

/*<private>
 * gdk_dmabuf_new_for_bytes:
 * @bytes: the bytes to create a dmabuf for
 * @error: Return location for an error
 *
 * Creates a dmabuf representing the given bytes.
 *
 * Returns: The new dmabuf fd or -1 on error.
 **/
int
gdk_dmabuf_new_for_bytes (GBytes  *bytes,
                          GError **error)
{
  int udmabuf_fd;
  int mem_fd = -1;
  int dmabuf_fd = -1;
  gsize alignment, size;
  gpointer data;
  int res;

  g_return_val_if_fail (bytes != NULL, -1);

  if (!gdk_has_feature (GDK_FEATURE_DMABUF))
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Dmabuf support is disabled");
      return -1;
    }

  udmabuf_fd = udmabuf_initialize (error);
  if (udmabuf_fd < 0)
    return -1;

  alignment = sysconf (_SC_PAGE_SIZE);
  size = align (g_bytes_get_size (bytes), alignment);

#ifdef HAVE_MEMFD_CREATE
  mem_fd = memfd_create ("gtk", MFD_ALLOW_SEALING);
#endif
  if (mem_fd == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to create memfd: %s",
                   g_strerror (errno));
      goto out;
    }

  res = ftruncate (mem_fd, size);
  if (res == -1)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "ftruncate on memfd failed: %s",
                    g_strerror (errno));
      goto out;
    }

  if (fcntl (mem_fd, F_ADD_SEALS, F_SEAL_SHRINK) < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "fcntl on memfd failed: %s",
                    g_strerror (errno));
      goto out;
    }

  data = mmap (NULL, size, PROT_WRITE, MAP_SHARED, mem_fd, 0);
  if (data == NULL || data == MAP_FAILED)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "mmap failed: %s",
                    g_strerror (errno));
      goto out;
    }

  memcpy (data, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));

  munmap (data, size);

  dmabuf_fd = ioctl (udmabuf_fd,
                     UDMABUF_CREATE,
                     &(struct udmabuf_create) {
                       .memfd = mem_fd,
                       .flags = UDMABUF_FLAGS_CLOEXEC,
                       .offset = 0,
                       .size = size
                     });

  if (dmabuf_fd < 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "UDMABUF_CREATE ioctl failed: %s",
                    g_strerror (errno));
    }

out:
  if (mem_fd != -1)
    close (mem_fd);

  return dmabuf_fd;
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

gboolean
gdk_memory_layout_init_from_dmabuf (GdkMemoryLayout *self,
                                    const GdkDmabuf *dmabuf,
                                    gboolean         premultiplied,
                                    gsize            width,
                                    gsize            height)
{
  gboolean is_yuv;
  gsize i;

  if (dmabuf->modifier != DRM_FORMAT_MOD_LINEAR)
    return FALSE;

  if (!gdk_memory_format_find_by_dmabuf_fourcc (dmabuf->fourcc, premultiplied, &self->format, &is_yuv))
    return FALSE;

  if (!gdk_memory_format_is_block_boundary (self->format, width, height))
    return FALSE;

  g_return_val_if_fail (dmabuf->n_planes == gdk_memory_format_get_n_planes (self->format), FALSE);

  self->width = width;
  self->height = height;

  for (i = 0; i < dmabuf->n_planes; i++)
    {
      self->planes[i].offset = dmabuf->planes[i].offset;
      self->planes[i].stride = dmabuf->planes[i].stride;
    }

  self->size = self->planes[dmabuf->n_planes - 1].offset + 
               (self->height - 1) / gdk_memory_format_get_plane_block_height (self->format, dmabuf->n_planes - 1)
                                  * self->planes[dmabuf->n_planes - 1].stride +
               self->width / gdk_memory_format_get_plane_block_width (self->format, dmabuf->n_planes - 1)
                           * gdk_memory_format_get_plane_block_bytes (self->format, dmabuf->n_planes - 1);

  return TRUE;
}

