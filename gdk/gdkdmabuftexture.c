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

#include "gdkcolorstateprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkdmabufdownloaderprivate.h"
#include "gdkdmabufformatsbuilderprivate.h"
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabufprivate.h"
#include "gdkdmabuftexturebuilderprivate.h"
#include "gdktextureprivate.h"
#include <gdk/gdkglcontext.h>
#include <gdk/gdkgltexturebuilder.h>
#include <gdk/gdktexturedownloader.h>

/**
 * GdkDmabufTexture:
 *
 * A `GdkTexture` representing a DMA buffer.
 *
 * To create a `GdkDmabufTexture`, use the auxiliary
 * [class@Gdk.DmabufTextureBuilder] object.
 *
 * Dma-buf textures can only be created on Linux.
 *
 * Since: 4.14
 */

struct _GdkDmabufTexture
{
  GdkTexture parent_instance;

  GdkDisplay *display;

  GdkDmabuf dmabuf;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkDmabufTextureClass
{
  GdkTextureClass parent_class;
};

/**
 * gdk_dmabuf_error_quark:
 *
 * Registers an error quark for [class@Gdk.DmabufTexture] errors.
 *
 * Returns: the error quark
 **/
G_DEFINE_QUARK (gdk-dmabuf-error-quark, gdk_dmabuf_error)

G_DEFINE_TYPE (GdkDmabufTexture, gdk_dmabuf_texture, GDK_TYPE_TEXTURE)

static void
gdk_dmabuf_texture_dispose (GObject *object)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (object);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
    }

  g_clear_object (&self->display);

  G_OBJECT_CLASS (gdk_dmabuf_texture_parent_class)->dispose (object);
}

typedef struct _Download Download;

struct _Download
{
  GdkDmabufTexture *texture;
  guchar *data;
  GdkMemoryLayout layout;
  GdkColorState *color_state;
  volatile int spinlock;
};

static gboolean
gdk_dmabuf_texture_invoke_callback (gpointer data)
{
  Download *download = data;
  GdkDisplay *display = download->texture->display;

  if (display->egl_downloader &&
      gdk_dmabuf_downloader_download (display->egl_downloader,
                                      download->texture,
                                      download->data,
                                      &download->layout,
                                      download->color_state))
    {
      /* Successfully downloaded using EGL */
    }
  else if (display->vk_downloader &&
           gdk_dmabuf_downloader_download (display->vk_downloader,
                                           download->texture,
                                           download->data,
                                           &download->layout,
                                           download->color_state))
    {
      /* Successfully downloaded using Vulkan */
    }
#ifdef HAVE_DMABUF
  else if (gdk_dmabuf_download_mmap (GDK_TEXTURE (download->texture),
                                     download->data,
                                     &download->layout,
                                     download->color_state))
    {
      /* Successfully downloaded using mmap */
    }
#endif
  else
    {
      const GdkDmabuf *dmabuf = gdk_dmabuf_texture_get_dmabuf (download->texture);

      g_critical ("Failed to download %dx%d dmabuf texture (format %.4s:%#" G_GINT64_MODIFIER "x)",
                  gdk_texture_get_width (GDK_TEXTURE (download->texture)),
                  gdk_texture_get_height (GDK_TEXTURE (download->texture)),
                  (char *)&(dmabuf->fourcc), dmabuf->modifier);
    }

  g_atomic_int_set (&download->spinlock, 1);
  return FALSE;
}

static void
gdk_dmabuf_texture_download (GdkTexture            *texture,
                             guchar                *data,
                             const GdkMemoryLayout *layout,
                             GdkColorState         *color_state)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  Download download = { self, data, *layout, color_state, 0 };

  g_main_context_invoke (NULL, gdk_dmabuf_texture_invoke_callback, &download);

  while (g_atomic_int_get (&download.spinlock) == 0);
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
}

GdkDisplay *
gdk_dmabuf_texture_get_display (GdkDmabufTexture *self)
{
  return self->display;
}

const GdkDmabuf *
gdk_dmabuf_texture_get_dmabuf (GdkDmabufTexture *self)
{
  return &self->dmabuf;
}

GdkTexture *
gdk_dmabuf_texture_new_from_builder (GdkDmabufTextureBuilder *builder,
                                     GDestroyNotify           destroy,
                                     gpointer                 data,
                                     GError                 **error)
{
#ifdef HAVE_DMABUF
  GdkDmabufTexture *self;
  GdkTexture *update_texture;
  GdkDisplay *display;
  GdkDmabuf dmabuf;
  GdkColorState *color_state;
  int width, height;
  gboolean premultiplied;
  GdkMemoryFormat format;
  gboolean is_yuv;

  display = gdk_dmabuf_texture_builder_get_display (builder);
  width = gdk_dmabuf_texture_builder_get_width (builder);
  height = gdk_dmabuf_texture_builder_get_height (builder);
  premultiplied = gdk_dmabuf_texture_builder_get_premultiplied (builder);

  if (!gdk_dmabuf_sanitize (&dmabuf,
                            width,
                            height,
                            gdk_dmabuf_texture_builder_get_dmabuf (builder),
                            error))
    return NULL;

  if (gdk_memory_format_find_by_dmabuf_fourcc (dmabuf.fourcc, premultiplied, &format, &is_yuv))
    {
      if (!gdk_memory_format_is_block_boundary (format, width, height))
        {
          g_set_error (error,
                       GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                       "Size of %dx%d is not valid for dmabuf format %.4s, must be multiple of %zux%zu",
                       width, height,
                       (char *) &dmabuf.fourcc,
                       gdk_memory_format_get_block_width (format),
                       gdk_memory_format_get_block_height (format));
          return NULL;
        }
    }
  else
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format %.4s",
                   (char *) &dmabuf.fourcc);
      return NULL;
    }

  gdk_display_init_dmabuf (display);

  if (!gdk_dmabuf_formats_contains (display->dmabuf_formats, dmabuf.fourcc, dmabuf.modifier))
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format: %.4s:%#" G_GINT64_MODIFIER "x",
                   (char *) &dmabuf.fourcc, dmabuf.modifier);
      return NULL;
    }

  color_state = gdk_dmabuf_texture_builder_get_color_state (builder);
  if (color_state == NULL)
    {
      if (is_yuv)
        color_state = GDK_COLOR_STATE_YUV;
      else
        color_state = GDK_COLOR_STATE_SRGB;
    }

  self = g_object_new (GDK_TYPE_DMABUF_TEXTURE,
                       "width", width,
                       "height", height,
                       "color-state", color_state,
                       NULL);

  g_set_object (&self->display, display);
  GDK_TEXTURE (self)->format = format;
  self->dmabuf = dmabuf;

  GDK_DISPLAY_DEBUG (display, DMABUF,
                     "Creating dmabuf texture, format %.4s:%#" G_GINT64_MODIFIER "x, %s%u planes, memory format %s",
                     (char *) &dmabuf.fourcc, dmabuf.modifier,
                     gdk_dmabuf_texture_builder_get_premultiplied (builder) ? " premultiplied, " : "",
                     dmabuf.n_planes,
                     gdk_memory_format_get_name (GDK_TEXTURE (self)->format));

  /* Set this only once we know that the texture will be created.
   * Otherwise dispose() will run the callback */
  self->destroy = destroy;
  self->data = data;

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

#else /* !HAVE_DMABUF */
  g_set_error_literal (error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_NOT_AVAILABLE,
                       "dmabuf support disabled at compile-time.");
  return NULL;
#endif
}

