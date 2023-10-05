/* gdkdmabuftexture.c
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

#include "gdkdmabuftextureprivate.h"

#ifndef __linux__

GType
gdk_dmabuf_texture_get_type (void)
{
  return G_TYPE_INVALID;
}

#else

#include "gdkdisplayprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include <gdk/gdkglcontext.h>
#include <gdk/gdkgltexturebuilder.h>
#include <gdk/gdktexturedownloader.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>

/**
 * GdkDmabufTexture:
 *
 * A `GdkTexture` representing a dma-buf object.
 *
 * To create a `GdkDmabufTexture`, use the auxiliary
 * [class@Gdk.DmabufTextureBuilder] object.
 *
 * Dma-buf textures can only be created on Linux.
 */

struct _GdkDmabufTexture
{
  GdkTexture parent_instance;

  GdkDisplay *display;

  unsigned int fourcc;
  guint64 modifier;

  /* Per-plane properties */
  int fd[4];
  unsigned int stride[4];
  unsigned int offset[4];

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkDmabufTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkDmabufTexture, gdk_dmabuf_texture, GDK_TYPE_TEXTURE)

static void
gdk_dmabuf_texture_dispose (GObject *object)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (object);

  if (self->destroy)
    self->destroy (self->data);

  G_OBJECT_CLASS (gdk_dmabuf_texture_parent_class)->dispose (object);
}

static struct {
  unsigned int fourcc;
  guint64 modifier;
  GdkMemoryFormat memory;
  int planes;
} supported_formats[] = {
  { DRM_FORMAT_RGB888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_R8G8B8, 1 },
  { DRM_FORMAT_BGR888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_B8G8R8, 1 },
  { DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_A8R8G8B8, 1 },
  { DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_A8B8G8R8, 1 },
  { DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_R8G8B8A8, 1 },
  { DRM_FORMAT_BGRA8888, DRM_FORMAT_MOD_LINEAR, GDK_MEMORY_B8G8R8A8, 1 },
};

static gboolean
get_memory_format (GdkDisplay      *display,
                   unsigned int     fourcc,
                   guint64          modifier,
                   GdkMemoryFormat *format,
                   int             *n_planes)
{
  /* We can always support simple linear formats */
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (supported_formats[i].fourcc == fourcc &&
          supported_formats[i].modifier == modifier)
        {
          *format = supported_formats[i].memory;
          *n_planes = supported_formats[i].planes;
          return TRUE;
        }
    }

  if (display)
    {
      const GdkDmabufFormat *formats;
      gsize n_formats;

      /* For others, we rely on a detour though GL */
      formats = gdk_display_get_dmabuf_formats (display, &n_formats);
      for (gsize i = 0; i < n_formats; i++)
        {
          if (formats[i].fourcc == fourcc &&
              formats[i].modifier == modifier)
            {
              *format = GDK_MEMORY_A8R8G8B8; // FIXME
              *n_planes = 1; // FIXME
              return TRUE;
            }
        }
    }

  return FALSE;
}

static int
import_dmabuf_into_gl (GdkDmabufTexture *self)
{
  GdkGLContext *context;
  EGLDisplay egl_display;
  EGLint attribs[64];
  EGLImage image;
  guint texture_id;
  int i;

  egl_display = gdk_display_get_egl_display (self->display);

  if (egl_display == EGL_NO_DISPLAY)
    {
      g_warning ("Can't import dmabufs when not using EGL");
      return 0;
    }

  i = 0;
  attribs[i++] = EGL_WIDTH;
  attribs[i++] = gdk_texture_get_width (GDK_TEXTURE (self));
  attribs[i++] = EGL_HEIGHT;
  attribs[i++] = gdk_texture_get_height (GDK_TEXTURE (self));
  attribs[i++] = EGL_LINUX_DRM_FOURCC_EXT;
  attribs[i++] = self->fourcc;

#define ADD_PLANE_ATTRIBUTES(plane) \
  if (self->fd[plane] != -1 || self->offset[plane] != 0) \
    { \
      attribs[i++] = EGL_DMA_BUF_PLANE ## plane ## _FD_EXT; \
      attribs[i++] = self->fd[plane]; \
      attribs[i++] = EGL_DMA_BUF_PLANE ## plane ## _MODIFIER_LO_EXT; \
      attribs[i++] = self->modifier & 0xFFFFFFFF; \
      attribs[i++] = EGL_DMA_BUF_PLANE ## plane ## _MODIFIER_HI_EXT; \
      attribs[i++] = self->modifier >> 32; \
      attribs[i++] = EGL_DMA_BUF_PLANE ## plane ## _PITCH_EXT; \
      attribs[i++] = self->stride[plane]; \
      attribs[i++] = EGL_DMA_BUF_PLANE ## plane ## _OFFSET_EXT; \
      attribs[i++] = self->offset[plane]; \
    }

  ADD_PLANE_ATTRIBUTES (0)
  ADD_PLANE_ATTRIBUTES (1)
  ADD_PLANE_ATTRIBUTES (2)
  ADD_PLANE_ATTRIBUTES (3)

  attribs[i++] = EGL_NONE;

  image = eglCreateImageKHR (egl_display,
                             EGL_NO_CONTEXT,
                             EGL_LINUX_DMA_BUF_EXT,
                             (EGLClientBuffer)NULL,
                             attribs);
  if (image == EGL_NO_IMAGE)
    {
      g_warning ("Failed to create EGL image: %d", eglGetError ());
      return 0;
    }

  context = gdk_display_get_gl_context (self->display);
  gdk_gl_context_make_current (context);

  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glEGLImageTargetTexture2DOES (GL_TEXTURE_2D, image);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  eglDestroyImageKHR (egl_display, image);

  return texture_id;
}

static void
do_indirect_download (GdkDmabufTexture *self,
                      GdkMemoryFormat  format,
                      guchar          *data,
                      gsize            stride)
{
  GdkGLTextureBuilder *builder;
  int id;
  GdkTexture *gl_texture;
  GdkTextureDownloader *downloader;

  GDK_DEBUG (MISC, "Using EGLImage and GL import for downloading a dmabuf");

  g_assert (GDK_IS_DISPLAY (self->display));
  g_assert (GDK_IS_GL_CONTEXT (gdk_display_get_gl_context (self->display)));

  id = import_dmabuf_into_gl (self);
  if (id == 0)
    return;

  builder = gdk_gl_texture_builder_new ();

  gdk_gl_texture_builder_set_context (builder, gdk_display_get_gl_context (self->display));
  gdk_gl_texture_builder_set_id (builder, id);
  gdk_gl_texture_builder_set_width (builder, gdk_texture_get_width (GDK_TEXTURE (self)));
  gdk_gl_texture_builder_set_height (builder, gdk_texture_get_height (GDK_TEXTURE (self)));
  gdk_gl_texture_builder_set_format (builder, gdk_texture_get_format (GDK_TEXTURE (self)));

  gl_texture = gdk_gl_texture_builder_build (builder, NULL, NULL);

  downloader = gdk_texture_downloader_new (gl_texture);

  gdk_texture_downloader_set_format (downloader, format);

  gdk_texture_downloader_download_into (downloader, data, stride);

  gdk_texture_downloader_free (downloader);
  g_object_unref (gl_texture);
  g_object_unref (builder);
}

static void
do_direct_download (GdkDmabufTexture *self,
                    guchar           *data,
                    gsize             stride)
{
  gsize size;
  unsigned int height;
  gsize src_stride;
  guchar *src_data;
  int bpp;

  GDK_DEBUG (MISC, "Using mmap() and memcpy() for downloading a dmabuf");

  height = gdk_texture_get_height (GDK_TEXTURE (self));
  bpp = gdk_memory_format_bytes_per_pixel (gdk_texture_get_format (GDK_TEXTURE (self)));

  src_stride = self->stride[0];
  size = self->stride[0] * height;

  if (ioctl (self->fd[0], DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dma-buf: %s", g_strerror (errno));

  src_data = mmap (NULL, size, PROT_READ, MAP_SHARED, self->fd[0], self->offset[0]);

  if (stride == src_stride)
    memcpy (data, src_data, size);
  else
    {
      for (unsigned int i = 0; i < height; i++)
        memcpy (data + i * stride, src_data + i * src_stride, height * bpp);
    }

  if (ioctl (self->fd[0], DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dma-buf: %s", g_strerror (errno));

  munmap (src_data, size);
}

static void
gdk_dmabuf_texture_download (GdkTexture      *texture,
                             GdkMemoryFormat  format,
                             guchar          *data,
                             gsize            stride)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  GdkMemoryFormat fmt;
  int planes;
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);

  if (!get_memory_format (NULL, self->fourcc, self->modifier, &fmt, &planes))
    do_indirect_download (self, format, data, stride);
  else if (format == src_format)
    do_direct_download (self, data, stride);
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = self->stride[0];
      src_data = g_new (guchar, src_stride * height);

      do_direct_download (self, src_data, src_stride);

      gdk_memory_convert (data, stride, format,
                          src_data, src_stride, src_format,
                          width, height);

      g_free (src_data);
    }
}

static void
gdk_dmabuf_texture_class_init (GdkDmabufTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_dmabuf_texture_download;

  gobject_class->dispose = gdk_dmabuf_texture_dispose;
}

static void
gdk_dmabuf_texture_init (GdkDmabufTexture *self)
{
  self->fd[0] = self->fd[1] = self->fd[2] = self->fd[3] = -1;
}

GdkTexture *
gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                     GDestroyNotify           destroy,
                                     gpointer                 data)
{
  GdkDmabufTexture *self;
  GdkTexture *update_texture;
  GdkMemoryFormat format;
  int n_planes;
  uint32_t f = gdk_dmabuf_texture_builder_get_fourcc (builder);
  uint64_t m = gdk_dmabuf_texture_builder_get_modifier(builder);

  if (!get_memory_format (gdk_dmabuf_texture_builder_get_display (builder),
                          gdk_dmabuf_texture_builder_get_fourcc (builder),
                          gdk_dmabuf_texture_builder_get_modifier (builder),
                          &format, &n_planes))
    {
      g_warning ("Unsupported dmabuf format %c%c%c%c:%#lx", f & 0xff, (f >> 8) & 0xff, (f >> 16) & 0xff, (f >> 24) & 0xff, m);
      return NULL;
    }

  GDK_DEBUG (MISC, "Dmabuf texture in format %c%c%c%c:%#lx", f & 0xff, (f >> 8) & 0xff, (f >> 16) & 0xff, (f >> 24) & 0xff, m);

  self = g_object_new (GDK_TYPE_DMABUF_TEXTURE,
                       "width", gdk_dmabuf_texture_builder_get_width (builder),
                       "height", gdk_dmabuf_texture_builder_get_height (builder),
                       NULL);

  GDK_TEXTURE (self)->format = format;

  g_set_object (&self->display, gdk_dmabuf_texture_builder_get_display (builder));

  self->fourcc = gdk_dmabuf_texture_builder_get_fourcc (builder);
  self->modifier = gdk_dmabuf_texture_builder_get_modifier (builder);

  for (int i = 0; i < 4; i++)
    {
      self->fd[i] = gdk_dmabuf_texture_builder_get_fd (builder, i);
      self->stride[i] = gdk_dmabuf_texture_builder_get_stride (builder, i);
      if (self->stride[i] == 0)
        self->stride[i] = gdk_dmabuf_texture_builder_get_width (builder) *
                          gdk_memory_format_bytes_per_pixel (format);
      self->offset[i] = gdk_dmabuf_texture_builder_get_offset (builder, i);
    }

  update_texture = gdk_dmabuf_texture_builder_get_update_texture (builder);
  if (update_texture)
    {
      cairo_region_t *update_region = gdk_dmabuf_texture_builder_get_update_region (builder);
      if (update_region)
        {
          update_region = cairo_region_copy (update_region);
          cairo_region_intersect_rectangle (update_region,
                                            &(cairo_rectangle_int_t) {
                                              0, 0,
                                              update_texture->width, update_texture->height
                                            });
          gdk_texture_set_diff (GDK_TEXTURE (self), update_texture, update_region);
        }
    }

  return GDK_TEXTURE (self);
}

unsigned int
gdk_dmabuf_texture_get_fourcc (GdkDmabufTexture *texture)
{
  return texture->fourcc;
}

guint64
gdk_dmabuf_texture_get_modifier (GdkDmabufTexture *texture)
{
  return texture->modifier;
}

unsigned int
gdk_dmabuf_texture_get_offset (GdkDmabufTexture *texture,
                               int               plane)
{
  return texture->offset[plane];
}

unsigned int
gdk_dmabuf_texture_get_stride (GdkDmabufTexture *texture,
                               int               plane)
{
  return texture->stride[plane];
}

int
gdk_dmabuf_texture_get_fd (GdkDmabufTexture *texture,
                           int               plane)
{
  return texture->fd[plane];
}

#endif /* __linux__ */
