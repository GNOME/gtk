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

typedef struct _Download Download;

struct _Download
{
  guint gl_internalformat;
  guint gl_format;
  guint gl_type;
  guchar *data;
  gsize stride;
};

static inline void
gdk_gl_texture_do_download (gpointer texture_,
                            gpointer download_)
{
  GdkGLTexture *self = texture_;
  GdkTexture *texture = texture_;
  Download *download = download_;

  if (gdk_gl_context_get_use_es (self->context))
    {
      GLuint fbo;

      glGenFramebuffers (1, &fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->id, 0);
      glReadPixels (0, 0,
                    texture->width, texture->height, 
                    download->gl_format,
                    download->gl_type,
                    download->data);
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &fbo);
    }
  else
    {
      glGetTexImage (GL_TEXTURE_2D,
                     0,
                     download->gl_format,
                     download->gl_type,
                     download->data);
    }
}

static void
gdk_gl_texture_download (GdkTexture      *texture,
                         GdkMemoryFormat  format,
                         guchar          *data,
                         gsize            stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  Download download;

  if (self->saved)
    {
      gdk_texture_do_download (self->saved, format, data, stride);
      return;
    }

  download.data = data;
  download.stride = stride;

  if (stride != texture->width * gdk_memory_format_bytes_per_pixel (format) ||
      !gdk_memory_format_gl_format (format,
                                    gdk_gl_context_get_use_es (self->context),
                                    &download.gl_internalformat,
                                    &download.gl_format,
                                    &download.gl_type))
    {
      GdkMemoryTexture *memtex;

      memtex = gdk_memory_texture_from_texture (texture,
                                                format == texture->format ? GDK_MEMORY_R8G8B8A8_PREMULTIPLIED
                                                                          : texture->format);
      gdk_texture_do_download (GDK_TEXTURE (memtex), format, data, stride);
      g_object_unref (memtex);
      return;
    }

  gdk_gl_texture_run (self, gdk_gl_texture_do_download, &download);
}

static void
gdk_gl_texture_class_init (GdkGLTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

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
  GdkTexture *texture;

  g_return_if_fail (GDK_IS_GL_TEXTURE (self));
  g_return_if_fail (self->saved == NULL);

  texture = GDK_TEXTURE (self);
  self->saved = GDK_TEXTURE (gdk_memory_texture_from_texture (texture,
                                                              gdk_texture_get_format (texture)));

  if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
      self->data = NULL;
    }

  g_clear_object (&self->context);
  self->id = 0;
}

static void
gdk_gl_texture_do_determine_format (gpointer texture_,
                                    gpointer unused)
{
  GdkTexture *texture = texture_;
  GLint internal_format;

  glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

  switch (internal_format)
  {
    case GL_RGB8:
      texture->format = GDK_MEMORY_R8G8B8;
      break;

    case GL_RGBA8:
      texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;

    case GL_RGB16:
      texture->format = GDK_MEMORY_R16G16B16;
      break;

    case GL_RGBA16:
      texture->format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
      break;

    case GL_RGB16F:
      texture->format = GDK_MEMORY_R16G16B16_FLOAT;
      break;

    case GL_RGBA16F:
      texture->format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
      break;

    case GL_RGB32F:
      texture->format = GDK_MEMORY_R32G32B32_FLOAT;
      break;

    case GL_RGBA32F:
      texture->format = GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
      break;

    default:
      g_warning ("Texture in unexpected format 0x%X (%d). File a bug about adding it to GTK", internal_format, internal_format);
      /* fallback to the dumbest possible format
       * so that even age old GLES can do it */
      texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;
  }
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

  gdk_gl_texture_run (self, gdk_gl_texture_do_determine_format, NULL);

  return GDK_TEXTURE (self);
}

