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
#include "gdkmemorytextureprivate.h"
#include "gdktextureprivate.h"

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

  GdkTexture *saved;

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

  g_clear_object (&self->saved);

  G_OBJECT_CLASS (gdk_gl_texture_parent_class)->dispose (object);
}

static GdkTexture *
gdk_gl_texture_download_texture (GdkTexture *texture)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  GdkTexture *result;
  int active_texture;
  GdkMemoryFormat format;
  GLint internal_format, gl_format, gl_type;
  guchar *data;
  gsize stride;
  GBytes *bytes;

  if (self->saved)
    return g_object_ref (self->saved);

  gdk_gl_context_make_current (self->context);

  glGetIntegerv (GL_TEXTURE_BINDING_2D, &active_texture);
  glBindTexture (GL_TEXTURE_2D, self->id);

  glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

  switch (internal_format)
  {
    case GL_RGB8:
      format = GDK_MEMORY_R8G8B8;
      gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_BYTE;
      break;

    case GL_RGBA8:
      format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
      break;

    default:
      g_warning ("Texture in unexpected format 0x%X (%d). File a bug about adding it to GTK", internal_format, internal_format);
      /* fallback to the dumbest possible format
       * so that even age old GLES can do it */
      format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_BYTE;
      break;
  }

  stride = gdk_memory_format_bytes_per_pixel (format) * texture->width;
  data = g_malloc (stride * texture->height);

  glGetTexImage (GL_TEXTURE_2D,
                 0,
                 gl_format,
                 gl_type,
                 data);

  bytes = g_bytes_new_take (data, stride * texture->height);
  result = gdk_memory_texture_new (texture->width,
                                   texture->height,
                                   format,
                                   bytes,
                                   stride);

  g_bytes_unref (bytes);

  glBindTexture (GL_TEXTURE_2D, active_texture);

  return result;
}

static void
gdk_gl_texture_download (GdkTexture *texture,
                         guchar     *data,
                         gsize       stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  GdkSurface *gl_surface;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width, height;

  if (self->saved)
    {
      gdk_texture_download (self->saved, data, stride);
      return;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_width (texture);
  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 stride);

  cr = cairo_create (surface);

  gl_surface = gdk_gl_context_get_surface (self->context);
  gdk_cairo_draw_from_gl (cr, gl_surface, self->id, GL_TEXTURE, 1, 
                          0, 0,
                          width, height);

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download_texture = gdk_gl_texture_download_texture;
  texture_class->download = gdk_gl_texture_download;
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
  g_return_if_fail (GDK_IS_GL_TEXTURE (self));
  g_return_if_fail (self->saved == NULL);

  self->saved = gdk_texture_download_texture (GDK_TEXTURE (self));

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

