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
#include "gdkglcontextprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkcolorspace.h"
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

typedef struct _Download Download;

struct _Download
{
  GdkMemoryFormat format;
  GdkColorSpace *color_space;
  guchar *data;
  gsize stride;
};

static gboolean
gdk_gl_texture_find_format (gboolean         use_es,
                            GLint            gl_format,
                            GLint            gl_type,
                            GdkMemoryFormat *out_format)
{
  GdkMemoryFormat format;

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      GLenum q_internal_format, q_format, q_type;

      if (!gdk_memory_format_gl_format (format, use_es, &q_internal_format, &q_format, &q_type))
        continue;

      if (q_format != gl_format || q_type != gl_type)
        continue;

      *out_format = format;
      return TRUE;
    }

  return FALSE;
}

static inline void
gdk_gl_texture_do_download (gpointer texture_,
                            gpointer download_)
{
  gsize expected_stride;
  GdkGLTexture *self = texture_;
  GdkTexture *texture = texture_;
  Download *download = download_;
  GLenum gl_internal_format, gl_format, gl_type;

  expected_stride = texture->width * gdk_memory_format_bytes_per_pixel (download->format);

  if (download->stride == expected_stride &&
      !gdk_gl_context_get_use_es (self->context) &&
      gdk_memory_format_gl_format (download->format, TRUE, &gl_internal_format, &gl_format, &gl_type))
    {
      glGetTexImage (GL_TEXTURE_2D,
                     0,
                     gl_format,
                     gl_type,
                     download->data);
    }
  else
    {
      GdkMemoryFormat actual_format;
      GLint gl_read_format, gl_read_type;
      GLuint fbo;

      glGenFramebuffers (1, &fbo);
      glBindFramebuffer (GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->id, 0);
      if (gdk_gl_context_check_version (self->context, 4, 3, 3, 1))
        {
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_FORMAT, &gl_read_format);
          glGetFramebufferParameteriv (GL_FRAMEBUFFER, GL_IMPLEMENTATION_COLOR_READ_TYPE, &gl_read_type);
          if (!gdk_gl_texture_find_format (gdk_gl_context_get_use_es (self->context), gl_read_format, gl_read_type, &actual_format))
            actual_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED; /* pray */
        }
      else
        {
          gl_read_format = GL_RGBA;
          gl_read_type = GL_UNSIGNED_BYTE;
          actual_format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
        }

      if (download->format == actual_format &&
          (download->stride == expected_stride))
        {
          glReadPixels (0, 0,
                        texture->width, texture->height,
                        gl_read_format,
                        gl_read_type,
                        download->data);
        }
      else
        {
          gsize actual_bpp = gdk_memory_format_bytes_per_pixel (actual_format);
          guchar *pixels = g_malloc_n (texture->width * actual_bpp, texture->height);

          glReadPixels (0, 0,
                        texture->width, texture->height,
                        gl_read_format,
                        gl_read_type,
                        pixels);

          gdk_memory_convert (download->data,
                              download->stride,
                              download->format,
                              download->color_space,
                              pixels,
                              texture->width * actual_bpp,
                              actual_format,
                              gdk_color_space_get_srgb (),
                              texture->width,
                              texture->height);

          g_free (pixels);
        }
      glBindFramebuffer (GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers (1, &fbo);
    }
}

static void
gdk_gl_texture_download (GdkTexture      *texture,
                         GdkMemoryFormat  format,
                         GdkColorSpace   *color_space,
                         guchar          *data,
                         gsize            stride)
{
  GdkGLTexture *self = GDK_GL_TEXTURE (texture);
  Download download;

  if (self->saved)
    {
      gdk_texture_do_download (self->saved, format, color_space, data, stride);
      return;
    }

  download.format = format;
  download.color_space = color_space;
  download.data = data;
  download.stride = stride;

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
  GdkTexture *texture;

  g_return_if_fail (GDK_IS_GL_TEXTURE (self));
  g_return_if_fail (self->saved == NULL);

  texture = GDK_TEXTURE (self);
  self->saved = GDK_TEXTURE (gdk_memory_texture_from_texture (texture,
                                                              gdk_texture_get_format (texture),
                                                              gdk_texture_get_color_space (texture)));

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
gdk_gl_texture_determine_format (GdkGLTexture *self)
{
  GdkTexture *texture = GDK_TEXTURE (self);
  GLint active_texture;
  GLint internal_format;

  /* Abort if somebody else is GL-ing here... */
  if (self->context != gdk_gl_context_get_current () ||
      /* ... or glGetTexLevelParameter() isn't supported */
      !gdk_gl_context_check_version (self->context, 0, 0, 3, 1))
    {
      texture->format = GDK_MEMORY_DEFAULT;
      return;
    }

  /* We need to be careful about modifying the GL context, as this is not
   * expected during construction */
  glGetIntegerv (GL_TEXTURE_BINDING_2D, &active_texture);
  glBindTexture (GL_TEXTURE_2D, self->id);

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

    case GL_RGBA:
      {
        GLint red_size = 0;
        GLint green_size = 0;
        GLint blue_size = 0;
        GLint alpha_size = 0;
        GLint red_type = 0;
        GLint green_type = 0;
        GLint blue_type = 0;
        GLint alpha_type = 0;

        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_RED_TYPE, &red_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_TYPE, &green_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_TYPE, &blue_type);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_TYPE, &alpha_type);

        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &red_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &green_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE, &blue_size);
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &alpha_size);

#define CHECK_RGBA(rt,gt,bt,at,rs,gs,bs,as) \
        (red_type == rt && green_type == gt && blue_type == bt && alpha_type == at && \
         red_size == rs && green_size == gs && blue_size == bs && alpha_size == as)

        if (CHECK_RGBA (GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, GL_UNSIGNED_NORMALIZED, 8, 8, 8, 8))
          {
            texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
            break;
          }

#undef CHECK_RGBA
      }

      G_GNUC_FALLTHROUGH;

    default:
      g_warning ("Texture in unexpected format 0x%X (%d). File a bug about adding it to GTK", internal_format, internal_format);
      /* fallback to the dumbest possible format
       * so that even age old GLES can do it */
      texture->format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
      break;
  }

  glBindTexture (GL_TEXTURE_2D, active_texture);
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
 * The texture data is assumed to be premultiplied, not flipped, and in the
 * sRGB colorspace, see [ctor@Gdk.GLTexture.new_with_color_profile] to override
 * this.
 *
 * Return value: (transfer full) (type GdkGLTexture): A newly-created
 *   `GdkTexture`
 */
GdkTexture *
gdk_gl_texture_new (GdkGLContext   *context,
                    guint           id,
                    int             width,
                    int             height,
                    GDestroyNotify  destroy,
                    gpointer        data)
{
  return gdk_gl_texture_new_with_color_space (context, id,
                                              width, height,
                                              GDK_GL_TEXTURE_PREMULTIPLIED,
                                              gdk_color_space_get_srgb (),
                                              destroy, data);
}

/**
 * gdk_gl_texture_new_with_color_space:
 * @context: a `GdkGLContext`
 * @id: the ID of a texture that was created with @context
 * @width: the nominal width of the texture
 * @height: the nominal height of the texture
 * @flags: flags that describe the content of the texture
 * @color_space: the `GdkColorSpace` for the content of the texture
 * @destroy: a destroy notify that will be called when the GL resources
 *   are released
 * @data: data that gets passed to @destroy
 *
 * Creates a new texture for an existing GL texture with a given color space
 * and flags.
 *
 * Note that the GL texture must not be modified until @destroy is called,
 * which will happen when the `GdkTexture` object is finalized, or due to
 * an explicit call of [method@Gdk.GLTexture.release].
 *
 * Return value: (transfer full): A newly-created `GdkTexture`
 *
 * Since: 4.10
 */
GdkTexture *
gdk_gl_texture_new_with_color_space (GdkGLContext      *context,
                                     guint              id,
                                     int                width,
                                     int                height,
                                     GdkGLTextureFlags  flags,
                                     GdkColorSpace     *color_space,
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
                       "color-space", color_space,
                       NULL);

  self->context = g_object_ref (context);
  self->id = id;
  self->flags = flags;
  self->destroy = destroy;
  self->data = data;

  gdk_gl_texture_determine_format (self);

  return GDK_TEXTURE (self);
}
