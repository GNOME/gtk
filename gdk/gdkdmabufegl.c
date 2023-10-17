/* gdkdmabufegl.c
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

#if defined(HAVE_LINUX_DMA_BUF_H) && defined (HAVE_EGL)
#include "gdkdmabufprivate.h"

#include "gdkdebugprivate.h"
#include "gdkdmabuftextureprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include "gdktexturedownloader.h"

#include <graphene.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>

static void
gdk_dmabuf_egl_downloader_add_formats (const GdkDmabufDownloader *downloader,
                                       GdkDisplay                *display,
                                       GdkDmabufFormatsBuilder   *builder)
{
  GdkGLContext *context = gdk_display_get_gl_context (display);
  EGLDisplay egl_display = gdk_display_get_egl_display (display);

  gdk_gl_context_make_current (context);

  if (egl_display != EGL_NO_DISPLAY &&
      display->have_egl_dma_buf_import &&
      gdk_gl_context_has_image_storage (context))
    {
      int num_fourccs;
      int *fourccs;
      guint64 *modifiers;
      unsigned int *external_only;
      int n_mods;

      eglQueryDmaBufFormatsEXT (egl_display, 0, NULL, &num_fourccs);
      fourccs = g_new (int, num_fourccs);
      eglQueryDmaBufFormatsEXT (egl_display, num_fourccs, fourccs, &num_fourccs);

      n_mods = 80;
      modifiers = g_new (guint64, n_mods);
      external_only = g_new (unsigned int, n_mods);

      for (int i = 0; i < num_fourccs; i++)
        {
          int num_modifiers;

          eglQueryDmaBufModifiersEXT (egl_display,
                                      fourccs[i],
                                      0,
                                      NULL,
                                      NULL,
                                      &num_modifiers);

          if (num_modifiers > n_mods)
            {
              n_mods = num_modifiers;
              modifiers = g_renew (guint64, modifiers, n_mods);
              external_only = g_renew (unsigned int, external_only, n_mods);
            }

          eglQueryDmaBufModifiersEXT (egl_display,
                                      fourccs[i],
                                      num_modifiers,
                                      modifiers,
                                      external_only,
                                      &num_modifiers);

          for (int j = 0; j < num_modifiers; j++)
            {
              GDK_DEBUG (DMABUF, "%ssupported EGL dmabuf format %.4s:%#lx %s",
                         external_only[j] ? "un" : "",
                         (char *) &fourccs[i],
                         modifiers[j],
                         external_only[j] ? "EXT" : "");
              if (!external_only[j])
                gdk_dmabuf_formats_builder_add_format (builder, fourccs[i], modifiers[j]);
            }
        }

      g_free (modifiers);
      g_free (external_only);
      g_free (fourccs);
    }
}

static GdkMemoryFormat
get_memory_format (guint32  fourcc,
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

static gboolean
gdk_dmabuf_egl_downloader_supports (const GdkDmabufDownloader  *downloader,
                                    GdkDisplay                 *display,
                                    const GdkDmabuf            *dmabuf,
                                    gboolean                    premultiplied,
                                    GdkMemoryFormat            *out_format,
                                    GError                    **error)
{
  EGLDisplay egl_display;
  GdkGLContext *context;
  int num_modifiers;
  guint64 *modifiers;
  unsigned int *external_only;

  egl_display = gdk_display_get_egl_display (display);
  if (egl_display == EGL_NO_DISPLAY)
    {
      g_set_error_literal (error,
                           GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                           "EGL not available");
      return FALSE;
    }

  context = gdk_display_get_gl_context (display);

  gdk_gl_context_make_current (context);

  eglQueryDmaBufModifiersEXT (egl_display,
                              dmabuf->fourcc,
                              0,
                              NULL,
                              NULL,
                              &num_modifiers);

  modifiers = g_newa (uint64_t, num_modifiers);
  external_only = g_newa (unsigned int, num_modifiers);

  eglQueryDmaBufModifiersEXT (egl_display,
                              dmabuf->fourcc,
                              num_modifiers,
                              modifiers,
                              external_only,
                              &num_modifiers);

  for (int i = 0; i < num_modifiers; i++)
    {
      if (external_only[i])
        continue;

       if (modifiers[i] == dmabuf->modifier)
         {
           *out_format = get_memory_format (dmabuf->fourcc, premultiplied);
           return TRUE;
         }
    }

  g_set_error (error,
               GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
               "Unsupported dmabuf format: %.4s:%#lx",
               (char *) &dmabuf->fourcc, dmabuf->modifier);

  return FALSE;
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
gdk_dmabuf_egl_downloader_download (const GdkDmabufDownloader *downloader,
                                    GdkTexture                *texture,
                                    GdkMemoryFormat            format,
                                    guchar                    *data,
                                    gsize                      stride)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  GdkDisplay *display;
  GskRenderer *renderer;
  int width, height;
  GskRenderNode *node;
  GdkTexture *gl_texture;
  GdkTextureDownloader *tex_downloader;
  GError *error = NULL;

  GDK_DEBUG (DMABUF, "Using gsk for downloading a dmabuf");

  display = gdk_dmabuf_texture_get_display (self);

  renderer = gdk_display_get_egl_downloader_data (display);
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

      gdk_display_set_egl_downloader_data (display, renderer, g_object_unref);
    }

  width = gdk_texture_get_width (GDK_TEXTURE (self));
  height = gdk_texture_get_height (GDK_TEXTURE (self));

  node = gsk_texture_node_new (GDK_TEXTURE (self), &GRAPHENE_RECT_INIT (0, 0, width, height));
  gl_texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, width, height));
  gsk_render_node_unref (node);

  tex_downloader = gdk_texture_downloader_new (gl_texture);

  gdk_texture_downloader_set_format (tex_downloader, format);

  gdk_texture_downloader_download_into (tex_downloader, data, stride);

  gdk_texture_downloader_free (tex_downloader);
  g_object_unref (gl_texture);
}

const GdkDmabufDownloader *
gdk_dmabuf_get_egl_downloader (void)
{
  static const GdkDmabufDownloader downloader = {
    gdk_dmabuf_egl_downloader_add_formats,
    gdk_dmabuf_egl_downloader_supports,
    gdk_dmabuf_egl_downloader_download,
  };

  return &downloader;
}

#endif  /* HAVE_LINUX_DMA_BUF_H && HAVE_EGL */
