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
#include "gdkdmabuffourccprivate.h"
#include "gdkdmabufprivate.h"
#include "gdktextureprivate.h"
#include <gdk/gdkglcontext.h>
#include <gdk/gdkgltexturebuilder.h>
#include <gdk/gdktexturedownloader.h>

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
  const GdkDmabufDownloader *downloader;

  GdkDmabuf dmabuf;

  GDestroyNotify destroy;
  gpointer data;

  gboolean y_invert;
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
}

static void
gdk_dmabuf_texture_dispose (GObject *object)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (object);

  if (self->destroy)
    self->destroy (self->data);

  G_OBJECT_CLASS (gdk_dmabuf_texture_parent_class)->dispose (object);
}

static void
gdk_dmabuf_texture_download (GdkTexture      *texture,
                             GdkMemoryFormat  format,
                             guchar          *data,
                             gsize            stride)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);

  self->downloader->download (self->downloader,
                              texture,
                              format,
                              data,
                              stride);
}

static void
gdk_dmabuf_texture_class_init (GdkDmabufTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_dmabuf_texture_download;

  gobject_class->dispose = gdk_dmabuf_texture_dispose;
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

gboolean
gdk_dmabuf_texture_get_y_invert (GdkDmabufTexture *self)
{
  return self->y_invert;
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
  GdkMemoryFormat format;
  GError *local_error = NULL;
  int width, height;
  gsize i;

  display = gdk_dmabuf_texture_builder_get_display (builder);
  width = gdk_dmabuf_texture_builder_get_width (builder);
  height = gdk_dmabuf_texture_builder_get_height (builder);

  if (!gdk_dmabuf_sanitize (&dmabuf,
                            width,
                            height,
                            gdk_dmabuf_texture_builder_get_dmabuf (builder),
                            error))
    return NULL;

  gdk_display_init_dmabuf (display);

  for (i = 0; display->dmabuf_downloaders[i] != NULL; i++)
    {
      if (local_error && g_error_matches (local_error, GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT))
        g_clear_error (&local_error);

      if (display->dmabuf_downloaders[i]->supports (display->dmabuf_downloaders[i],
                                                    display,
                                                    &dmabuf,
                                                    gdk_dmabuf_texture_builder_get_premultiplied (builder),
                                                    &format,
                                                    local_error ? NULL : &local_error))
        break;
    }

  if (display->dmabuf_downloaders[i] == NULL)
    {
      g_propagate_error (error, local_error);
      return NULL;
    }

  GDK_DISPLAY_DEBUG (display, DMABUF,
             "Creating dmabuf texture, format %.4s:%#" G_GINT64_MODIFIER "x, %s%u planes, memory format %u, downloader %s",
             (char *) &dmabuf.fourcc, dmabuf.modifier,
             gdk_dmabuf_texture_builder_get_premultiplied (builder) ? " premultiplied, " : "",
             dmabuf.n_planes,
             format,
             display->dmabuf_downloaders[i]->name);

  self = g_object_new (GDK_TYPE_DMABUF_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  GDK_TEXTURE (self)->format = format;
  g_set_object (&self->display, display);
  self->downloader = display->dmabuf_downloaders[i];
  self->dmabuf = dmabuf;
  self->destroy = destroy;
  self->data = data;
  self->y_invert = gdk_dmabuf_texture_builder_get_y_invert (builder);

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

