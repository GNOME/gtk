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

#include "gdkdisplayprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include <gdk/gdkglcontext.h>
#include <gdk/gdkgltexturebuilder.h>
#include <gdk/gdktexturedownloader.h>

#ifdef HAVE_LINUX_DMA_BUF_H
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>
#endif

#include <graphene.h>

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

  guint32 fourcc;
  guint64 modifier;

  unsigned int n_planes;

  /* Per-plane properties */
  int fds[MAX_DMABUF_PLANES];
  unsigned int strides[MAX_DMABUF_PLANES];
  unsigned int offsets[MAX_DMABUF_PLANES];

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkDmabufTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_QUARK (gdk-dmabuf-error-quark, gdk_dmabuf_error)

G_DEFINE_TYPE (GdkDmabufTexture, gdk_dmabuf_texture, GDK_TYPE_TEXTURE)

static void
gdk_dmabuf_texture_init (GdkDmabufTexture *self)
{
  self->fds[0] = self->fds[1] = self->fds[2] = self->fds[3] = -1;
}

static void
gdk_dmabuf_texture_dispose (GObject *object)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (object);

  if (self->destroy)
    self->destroy (self->data);

  G_OBJECT_CLASS (gdk_dmabuf_texture_parent_class)->dispose (object);
}

static void gdk_dmabuf_texture_download (GdkTexture      *texture,
                                         GdkMemoryFormat  format,
                                         guchar          *data,
                                         gsize            stride);

static void
gdk_dmabuf_texture_class_init (GdkDmabufTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_dmabuf_texture_download;

  gobject_class->dispose = gdk_dmabuf_texture_dispose;
}

guint32
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
gdk_dmabuf_texture_get_n_planes (GdkDmabufTexture *texture)
{
  return texture->n_planes;
}

int *
gdk_dmabuf_texture_get_fds (GdkDmabufTexture *texture)
{
  return texture->fds;
}

unsigned int *
gdk_dmabuf_texture_get_strides (GdkDmabufTexture *texture)
{
  return texture->strides;
}

unsigned int *
gdk_dmabuf_texture_get_offsets (GdkDmabufTexture *texture)
{
  return texture->offsets;
}

#ifdef HAVE_LINUX_DMA_BUF_H

typedef struct _GdkDrmFormatInfo GdkDrmFormatInfo;
struct _GdkDrmFormatInfo
{
  guint32 fourcc;
  GdkMemoryFormat premultiplied_memory_format;
  GdkMemoryFormat unpremultiplied_memory_format;
};

/* These are the dma-buf formats that we can support just with memcpy,
 * as long as they are combined with DRM_FORMAT_MOD_LINEAR
 */
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

/* Hack. We don't include gsk/gsk.h here to avoid a build order problem
 * with the generated header gskenumtypes.h, so we need to hack around
 * a bit to access the gsk api we need.
 */

typedef gpointer GskRenderer;
typedef gpointer GskRenderNode;

extern GskRenderer *   gsk_gl_renderer_new         (void);
extern gboolean        gsk_renderer_realize        (GskRenderer            *renderer,
                                                    GdkSurface             *surface,
                                                    GError                **error);
extern void            gsk_renderer_unrealize      (GskRenderer            *renderer);
extern GdkTexture *    gsk_renderer_render_texture (GskRenderer            *renderer,
                                                    GskRenderNode          *node,
                                                    const graphene_rect_t  *bounds);
extern GskRenderNode * gsk_texture_node_new        (GdkTexture             *texture,
                                                    const graphene_rect_t  *viewport);
extern void            gsk_render_node_unref       (GskRenderNode          *node);

static void
do_indirect_download_gsk (GdkDmabufTexture *self,
                          GdkMemoryFormat  format,
                          guchar          *data,
                          gsize            stride)
{
  GskRenderer *renderer;
  int width, height;
  GskRenderNode *node;
  GdkTexture *gl_texture;
  GdkTextureDownloader *downloader;
  GError *error = NULL;

  GDK_DEBUG (DMABUF, "Using gsk for downloading a dmabuf");

  g_assert (GDK_IS_DISPLAY (self->display));
  g_assert (GDK_IS_GL_CONTEXT (gdk_display_get_gl_context (self->display)));

  renderer = g_object_get_data (G_OBJECT (self->display), "dmabuf-gl-renderer");
  if (!renderer)
    {
      renderer = gsk_gl_renderer_new ();
      if (!gsk_renderer_realize (renderer, NULL, &error))
        {
          g_warning ("Failed to realize GL renderer: %s", error->message);
          g_error_free (error);
          g_object_unref (renderer);
          return;
        }

      g_object_set_data_full (G_OBJECT (self->display), "dmabuf-gl-renderer",
                              renderer, g_object_unref);
    }

  width = gdk_texture_get_width (GDK_TEXTURE (self));
  height = gdk_texture_get_height (GDK_TEXTURE (self));

  node = gsk_texture_node_new (GDK_TEXTURE (self), &GRAPHENE_RECT_INIT (0, 0, width, height));
  gl_texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_render_node_unref (node);

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);

  downloader = gdk_texture_downloader_new (gl_texture);

  gdk_texture_downloader_set_format (downloader, format);

  gdk_texture_downloader_download_into (downloader, data, stride);

  gdk_texture_downloader_free (downloader);
  g_object_unref (gl_texture);
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

  GDK_DEBUG (DMABUF, "Using mmap for downloading a dmabuf");

  height = gdk_texture_get_height (GDK_TEXTURE (self));
  bpp = gdk_memory_format_bytes_per_pixel (gdk_texture_get_format (GDK_TEXTURE (self)));

  src_stride = self->strides[0];
  size = self->strides[0] * height;

  if (ioctl (self->fds[0], DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));

  src_data = mmap (NULL, size, PROT_READ, MAP_SHARED, self->fds[0], self->offsets[0]);

  if (stride == src_stride)
    memcpy (data, src_data, size);
  else
    {
      for (unsigned int i = 0; i < height; i++)
        memcpy (data + i * stride, src_data + i * src_stride, height * bpp);
    }

  munmap (src_data, size);

  if (ioctl (self->fds[0], DMA_BUF_IOCTL_SYNC, &(struct dma_buf_sync) { DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ }) < 0)
    g_warning ("Failed to sync dmabuf: %s", g_strerror (errno));
}

/* A helper to determine suitable download memory formats for drm formats */
static GdkMemoryFormat
compute_memory_format (guint32  fourcc,
                       gboolean premultiplied)
{
  switch (fourcc)
    {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XRGB8888_A8:
    case DRM_FORMAT_XBGR8888_A8:
      return premultiplied ? GDK_MEMORY_A8R8G8B8_PREMULTIPLIED : GDK_MEMORY_A8R8G8B8;

    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_RGBX8888_A8:
      return premultiplied ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED : GDK_MEMORY_R8G8B8A8;

    case DRM_FORMAT_BGRA8888:
      return premultiplied ? GDK_MEMORY_B8G8R8A8_PREMULTIPLIED : GDK_MEMORY_B8G8R8A8;

    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_BGRX8888:
      return GDK_MEMORY_R8G8B8;

    case DRM_FORMAT_BGR888:
      return GDK_MEMORY_B8G8R8;

    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_XRGB16161616:
    case DRM_FORMAT_XBGR16161616:
      return GDK_MEMORY_R16G16B16;

    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
    case DRM_FORMAT_ARGB16161616:
    case DRM_FORMAT_ABGR16161616:
      return premultiplied ? GDK_MEMORY_R16G16B16A16_PREMULTIPLIED : GDK_MEMORY_R16G16B16A16;

    case DRM_FORMAT_ARGB16161616F:
    case DRM_FORMAT_ABGR16161616F:
      return premultiplied ? GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED : GDK_MEMORY_R16G16B16A16_FLOAT;

    case DRM_FORMAT_XRGB16161616F:
    case DRM_FORMAT_XBGR16161616F:
      return GDK_MEMORY_R16G16B16_FLOAT;

    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_XYUV8888:
    case DRM_FORMAT_XVUY8888:
    case DRM_FORMAT_VUY888:
      return GDK_MEMORY_R8G8B8;

    /* Add more formats here */
    default:
      return premultiplied ? GDK_MEMORY_A8R8G8B8_PREMULTIPLIED : GDK_MEMORY_A8R8G8B8;
    }
}

#endif  /* HAVE_LINUX_DMA_BUF_H */

void
gdk_dmabuf_texture_add_supported_formats (GdkDmabufFormatsBuilder *builder)
{
#ifdef HAVE_LINUX_DMA_BUF_H
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      gdk_dmabuf_formats_builder_add_format (builder,
                                             supported_formats[i].fourcc,
                                             DRM_FORMAT_MOD_LINEAR);
    }
#endif
}

static void
gdk_dmabuf_texture_download (GdkTexture      *texture,
                             GdkMemoryFormat  format,
                             guchar          *data,
                             gsize            stride)
{
#ifdef HAVE_LINUX_DMA_BUF_H
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  GdkMemoryFormat src_format = gdk_texture_get_format (texture);
  GdkDrmFormatInfo *info;

  info = get_drm_format_info (self->fourcc);

  if (!info || self->n_planes > 1 || self->modifier != DRM_FORMAT_MOD_LINEAR)
    do_indirect_download_gsk (self, format, data, stride);
  else if (format == src_format)
    do_direct_download (self, data, stride);
  else
    {
      unsigned int width, height;
      guchar *src_data;
      gsize src_stride;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);

      src_stride = self->strides[0];
      src_data = g_new (guchar, src_stride * height);

      do_direct_download (self, src_data, src_stride);

      gdk_memory_convert (data, stride, format,
                          src_data, src_stride, src_format,
                          width, height);

      g_free (src_data);
     }
#endif  /* HAVE_LINUX_DMA_BUF_H */
}

static gboolean
display_supports_format (GdkDisplay *display,
                         guint32     fourcc,
                         guint64     modifier)
{
  GdkDmabufFormats *formats;

  if (!display)
    return FALSE;

  formats = gdk_display_get_dmabuf_formats (display);

  return gdk_dmabuf_formats_contains (formats, fourcc, modifier);
}

GdkTexture *
gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                     GDestroyNotify           destroy,
                                     gpointer                 data,
                                     GError                 **error)
{
#ifdef HAVE_LINUX_DMA_BUF_H
  GdkDmabufTexture *self;
  GdkTexture *update_texture;
  GdkDisplay *display = gdk_dmabuf_texture_builder_get_display (builder);
  guint32 fourcc = gdk_dmabuf_texture_builder_get_fourcc (builder);
  guint64 modifier = gdk_dmabuf_texture_builder_get_modifier (builder);
  gboolean premultiplied = gdk_dmabuf_texture_builder_get_premultiplied (builder);
  unsigned int n_planes = gdk_dmabuf_texture_builder_get_n_planes (builder);
  GdkDrmFormatInfo *info;
  gboolean can_download_directly;
  gboolean can_download_indirectly;

  info = get_drm_format_info (fourcc);

  can_download_directly = info != NULL && n_planes == 1 && modifier == DRM_FORMAT_MOD_LINEAR;
  can_download_indirectly = display_supports_format (display, fourcc, modifier);

  if (!can_download_directly && !can_download_indirectly)
    {
      g_set_error (error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format: %.4s:%#lx, planes: %u",
                   (char *) &fourcc, modifier, n_planes);
      return NULL;
    }

  GDK_DEBUG (DMABUF,
             "Create dmabuf texture (format: %.4s:%#lx, planes: %u)",
             (char *) &fourcc, modifier, n_planes);

  self = g_object_new (GDK_TYPE_DMABUF_TEXTURE,
                       "width", gdk_dmabuf_texture_builder_get_width (builder),
                       "height", gdk_dmabuf_texture_builder_get_height (builder),
                       NULL);

  self->destroy = destroy;
  self->data = data;

  if (can_download_directly)
    GDK_TEXTURE (self)->format = premultiplied ? info->premultiplied_memory_format
                                               : info->unpremultiplied_memory_format;
  else
    GDK_TEXTURE (self)->format = compute_memory_format (fourcc, premultiplied);

  g_set_object (&self->display, display);

  self->fourcc = fourcc;
  self->modifier = modifier;
  self->n_planes = n_planes;

  memcpy (self->fds, gdk_dmabuf_texture_builder_get_fds (builder), sizeof (int) * MAX_DMABUF_PLANES);
  memcpy (self->strides, gdk_dmabuf_texture_builder_get_strides (builder), sizeof (unsigned int) * MAX_DMABUF_PLANES);
  memcpy (self->offsets, gdk_dmabuf_texture_builder_get_offsets (builder), sizeof (unsigned int) * MAX_DMABUF_PLANES);

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

#else /* !HAVE_LINUX_DMA_BUF_H */
  g_set_error_literal (error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_NOT_AVAILABLE,
                       "dmabuf support disabled at compile-time.");
  return NULL;
#endif
}
