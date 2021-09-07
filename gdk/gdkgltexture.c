/* gdkgltexture.c
 *
 * Copyright 2016  Benjamin Otte
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

#include "gdkgltextureprivate.h"

#include "gdkcairo.h"
#include "gdktextureprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdkinternals.h"

#include <epoxy/gl.h>

/**
 * GdkGLTexture:
 *
 * A GdkTexture representing a GL texture object.
 */

struct _GdkGLTexture {
  GdkTexture parent_instance;

  GdkGLContext *context;
  guint id;

  cairo_surface_t *saved;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkGLTextureClass {
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkGLTexture, gdk_gl_texture, GDK_TYPE_TEXTURE)

static void
gdk_gl_texture_dispose (GObject *object)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (object);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;

  if (self->saved)
    {
      cairo_surface_destroy (self->saved);
      self->saved = NULL;
    }

  G_OBJECT_CLASS (gdk_gl_texture_parent_class)->dispose (object);
}

static void
gdk_gl_texture_download (GdkTexture         *texture,
                         const GdkRectangle *area,
                         guchar             *data,
                         gsize               stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 area->width, area->height,
                                                 stride);

  cr = cairo_create (surface);

  if (self->saved)
    {
      cairo_set_source_surface (cr, self->saved, 0, 0);
      cairo_paint (cr);
    }
  else
    {
      GdkSurface *gl_surface;

      gl_surface = gdk_gl_context_get_surface (self->context);
      gdk_cairo_draw_from_gl (cr, gl_surface, self->id, GL_TEXTURE, 1, 
                              area->x, area->y,
                              area->width, area->height);
    }

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static int
type_from_internal_format (int internal_format)
{
  switch (internal_format)
    {
    case GL_RGB16:
    case GL_RGBA16:
      return GL_UNSIGNED_SHORT;
    default:
      g_assert_not_reached ();
    }
}

static int
internal_format_for_format (GdkMemoryFormat format)
{
  switch ((int)format)
    {
    case GDK_MEMORY_R16G16B16:
      return GL_RGB16;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      return GL_RGBA16;
    default:
      g_assert_not_reached ();
    }
}

static GBytes *
gdk_gl_texture_download_format (GdkTexture      *texture,
                                GdkMemoryFormat  format)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  GdkSurface *surface;
  GdkGLContext *context;
  int internal_format = 0;
  gpointer data;
  gsize size;

  surface = gdk_gl_context_get_surface (self->context);
  context = gdk_surface_get_paint_gl_context (surface, NULL);

  gdk_gl_context_make_current (context);
  glBindTexture (GL_TEXTURE_2D, self->id);
  glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

  if (internal_format != internal_format_for_format (format))
    return NULL;

  size = texture->width * texture->height * gdk_memory_format_bytes_per_pixel (format);
  data = malloc (size);

  glReadPixels (0, 0, texture->width, texture->height,
                internal_format, type_from_internal_format (internal_format),
                 data);

  return g_bytes_new_take (data, size);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_gl_texture_download;
  texture_class->download_format = gdk_gl_texture_download_format;
  gobject_class->dispose = gdk_gl_texture_dispose;
}

static void
gdk_gl_texture_init (GdkGLTexture *self)
{
}

GdkGLContext *
gdk_gl_texture_get_context (GdkGLTexture *self)
{
  return self->context;
}

guint
gdk_gl_texture_get_id (GdkGLTexture *self)
{
  return self->id;
}

/**
 * gdk_gl_texture_release:
 * @self: a `GdkTexture` wrapping a GL texture
 *
 * Releases the GL resources held by a `GdkGLTexture`.
 *
 * The texture contents are still available via the
 * [method@Gdk.Texture.download] function, after this
 * function has been called.
 */
void
gdk_gl_texture_release (GdkGLTexture *self)
{
  GdkSurface *surface;
  GdkTexture *texture;
  cairo_t *cr;

  g_return_if_fail (GDK_IS_GL_TEXTURE (self));
  g_return_if_fail (self->saved == NULL);

  texture = GDK_TEXTURE (self);
  self->saved = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            texture->width, texture->height);

  cr = cairo_create (self->saved);

  surface = gdk_gl_context_get_surface (self->context);
  gdk_cairo_draw_from_gl (cr, surface, self->id, GL_TEXTURE, 1, 0, 0,
                          texture->width, texture->height);

  cairo_destroy (cr);

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;
}

/**
 * gdk_gl_texture_new:
 * @context: a `GdkGLContext`
 * @id: the ID of a texture that was created with @context
 * @width: the nominal width of the texture
 * @height: the nominal height of the texture
 * @destroy: a destroy notify that will be called when the GL resources
 *   are released
 * @data: data that gets passed to @destroy
 *
 * Creates a new texture for an existing GL texture.
 *
 * Note that the GL texture must not be modified until @destroy is called,
 * which will happen when the GdkTexture object is finalized, or due to
 * an explicit call of [method@Gdk.GLTexture.release].
 *
 * Return value: (transfer full): A newly-created `GdkTexture`
 */
GdkTexture *
gdk_gl_texture_new (GdkGLContext   *context,
                    guint           id,
                    int             width,
                    int             height,
                    GDestroyNotify  destroy,
                    gpointer        data)
{
  GdkGLTexture *self;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (id != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  self = g_object_new (GDK_TYPE_GL_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  self->context = g_object_ref (context);
  self->id = id;
  self->destroy = destroy;
  self->data = data;

  return GDK_TEXTURE (self);
}

