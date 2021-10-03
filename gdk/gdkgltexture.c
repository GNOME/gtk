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

#include "gdkdisplayprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdktextureprivate.h"
#include "gdkcolorprofile.h"

#include <epoxy/gl.h>

/**
 * GdkGLTexture:
 *
 * A GdkTexture representing a GL texture object.
 */

struct _GdkGLTexture {
  GdkTexture parent_instance;

  GdkGLContext *context;
  GdkGLTextureFlags flags;
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

typedef struct _InvokeData
{
  GdkGLTexture *self;
  volatile int spinlock;
  GFunc func;
  gpointer data;
} InvokeData;

static gboolean
gdk_gl_texture_invoke_callback (gpointer data)
{
  InvokeData *invoke = data;
  GdkGLContext *context;

  context = gdk_display_get_gl_context (gdk_gl_context_get_display (invoke->self->context));

  gdk_gl_context_make_current (context);
  glBindTexture (GL_TEXTURE_2D, invoke->self->id);

  invoke->func (invoke->self, invoke->data);

  g_atomic_int_set (&invoke->spinlock, 1);

  return FALSE;
}

static void
gdk_gl_texture_run (GdkGLTexture *self,
                    GFunc         func,
                    gpointer      data)
{
  InvokeData invoke = { self, 0, func, data };

  g_main_context_invoke (NULL, gdk_gl_texture_invoke_callback, &invoke);

  while (g_atomic_int_get (&invoke.spinlock) == 0);
}

static inline void
gdk_gl_texture_get_tex_image (GdkGLTexture *self,
                              GLenum        gl_format,
                              GLenum        gl_type,
                              GLvoid       *data)
{
  if (gdk_gl_context_get_use_es (self->context))
    {
      GdkTexture *texture = GDK_TEXTURE (self);
      GLuint fbo;

      glGenFramebuffers (1, &fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->id, 0);
      glReadPixels (0, 0,
                    texture->width, texture->height, 
                    gl_format,
                    gl_type,
                    data);
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &fbo);
    }
  else
    {
      glGetTexImage (GL_TEXTURE_2D,
                     0,
                     gl_format,
                     gl_type,
                     data);
    }
}

typedef struct {
  GdkMemoryFormat format;
  GLint gl_format;
  GLint gl_type;
} GdkGLTextureFormat;

static void
gdk_gl_texture_get_format (gpointer texture_,
                           gpointer result_)
{
  GdkGLTextureFormat *result = result_;
  GLint internal_format, gl_format, gl_type;
  GdkMemoryFormat format;

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

    case GL_RGB16:
      format = GDK_MEMORY_R16G16B16;
      gl_format = GL_RGB;
      gl_type = GL_UNSIGNED_SHORT;
      break;

    case GL_RGBA16:
      format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
      gl_format = GL_RGBA;
      gl_type = GL_UNSIGNED_SHORT;
      break;

    case GL_RGB16F:
      format = GDK_MEMORY_R16G16B16_FLOAT;
      gl_format = GL_RGB;
      gl_type = GL_HALF_FLOAT;
      break;

    case GL_RGBA16F:
      format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
      gl_format = GL_RGBA;
      gl_type = GL_HALF_FLOAT;
      break;

    case GL_RGB32F:
      format = GDK_MEMORY_R32G32B32_FLOAT;
      gl_format = GL_RGB;
      gl_type = GL_FLOAT;
      break;

    case GL_RGBA32F:
      format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
      gl_format = GL_RGBA;
      gl_type = GL_FLOAT;
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

  result->format = format;
  result->gl_format = gl_format;
  result->gl_type = gl_type;
}

static void
gdk_gl_texture_do_download_texture (gpointer texture_,
                                    gpointer result_)
{
  GdkTexture *texture = texture_;
  GdkTexture **result = result_;
  GdkGLTextureFormat format;
  gsize stride;
  guchar *data;
  GBytes *bytes;

  gdk_gl_texture_get_format (texture, &format);

  stride = gdk_memory_format_bytes_per_pixel (format.format) * texture->width;
  data = g_malloc (stride * texture->height);

  gdk_gl_texture_get_tex_image (texture_,
                                format.gl_format,
                                format.gl_type,
                                data);

  bytes = g_bytes_new_take (data, stride * texture->height);
  *result = gdk_memory_texture_new (texture->width,
                                    texture->height,
                                    format.format,
                                    bytes,
                                    stride);

  g_bytes_unref (bytes);
}

static GdkTexture *
gdk_gl_texture_download_texture (GdkTexture *texture)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  GdkTexture *result;

  if (self->saved)
    return g_object_ref (self->saved);

  gdk_gl_texture_run (self, gdk_gl_texture_do_download_texture, &result);

  return result;
}

static void
gdk_gl_texture_do_download (gpointer texture,
                            gpointer data)
{
  glGetTexImage (GL_TEXTURE_2D,
                 0,
                 GL_BGRA,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                 GL_UNSIGNED_INT_8_8_8_8_REV,
#elif G_BYTE_ORDER == G_BIG_ENDIAN
                 GL_UNSIGNED_BYTE,
#else
#error "Unknown byte order for gdk_gl_texture_download()"
#endif
                 data);
}

static void
gdk_gl_texture_download (GdkTexture *texture,
                         guchar     *data,
                         gsize       stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);

  if (self->saved)
    {
      gdk_texture_download (self->saved, data, stride);
      return;
    }

  if (gdk_gl_context_get_use_es (self->context) ||
      stride != texture->width * 4)
    {
      GDK_TEXTURE_CLASS (gdk_gl_texture_parent_class)->download (texture, data, stride);
      return;
    }

  gdk_gl_texture_run (self, gdk_gl_texture_do_download, data);
}

static void
gdk_gl_texture_do_download_float (gpointer texture,
                                  gpointer data)
{
  glGetTexImage (GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 data);
}

static void
gdk_gl_texture_download_float (GdkTexture *texture,
                               float      *data,
                               gsize       stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);

  if (self->saved)
    {
      gdk_texture_download_float (self->saved, data, stride);
      return;
    }

  if (gdk_gl_context_get_use_es (self->context) ||
      stride != texture->width * 4)
    {
      GDK_TEXTURE_CLASS (gdk_gl_texture_parent_class)->download_float (texture, data, stride);
      return;
    }

  gdk_gl_texture_run (self, gdk_gl_texture_do_download_float, data);
}

static gboolean
gdk_gl_texture_is_hdr (GdkTexture *texture)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  GdkGLTextureFormat result;

  gdk_gl_texture_run (self, gdk_gl_texture_get_format, &result);

  return gdk_memory_format_is_hdr (result.format);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download_texture = gdk_gl_texture_download_texture;
  texture_class->download = gdk_gl_texture_download;
  texture_class->download_float = gdk_gl_texture_download_float;
  texture_class->is_hdr = gdk_gl_texture_is_hdr;
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

GdkGLTextureFlags
gdk_gl_texture_get_flags (GdkGLTexture *self)
{
  return self->flags;
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
  return gdk_gl_texture_new_with_color_profile (context, id,
                                                width, height,
                                                GDK_GL_TEXTURE_PREMULTIPLIED,
                                                gdk_color_profile_get_srgb (),
                                                destroy, data);
}

/**
 * gdk_gl_texture_new_with_color_profile:
 * @context: a `GdkGLContext`
 * @id: the ID of a texture that was created with @context
 * @width: the nominal width of the texture
 * @height: the nominal height of the texture
 * @flags: flags that describe the content of the texture
 * @color_profile: the `GdkColorProfile` for the content of the texture
 * @destroy: a destroy notify that will be called when the GL resources
 *   are released
 * @data: data that gets passed to @destroy
 *
 * Creates a new texture for an existing GL texture with a given color profile.
 *
 * Note that the GL texture must not be modified until @destroy is called,
 * which will happen when the GdkTexture object is finalized, or due to
 * an explicit call of [method@Gdk.GLTexture.release].
 *
 * Return value: (transfer full): A newly-created `GdkTexture`
 *
 * Since: 4.6
 */
GdkTexture *
gdk_gl_texture_new_with_color_profile (GdkGLContext      *context,
                                       guint              id,
                                       int                width,
                                       int                height,
                                       GdkGLTextureFlags  flags,
                                       GdkColorProfile   *color_profile,
                                       GDestroyNotify     destroy,
                                       gpointer           data)
{
  GdkGLTexture *self;

  g_return_val_if_fail (GDK_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (id != 0, NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  self = g_object_new (GDK_TYPE_GL_TEXTURE,
                       "width", width,
                       "height", height,
                       "color-profile", color_profile,
                       NULL);

  self->context = g_object_ref (context);
  self->id = id;
  self->flags = flags;
  self->destroy = destroy;
  self->data = data;

  return GDK_TEXTURE (self);
}

