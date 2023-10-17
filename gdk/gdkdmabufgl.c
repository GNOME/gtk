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
#include "gdkgltexturebuilder.h"
#include "gdktexturedownloader.h"

#include <graphene.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <drm/drm_fourcc.h>
#include <epoxy/egl.h>

static void
gdk_dmabuf_gl_downloader_add_formats (const GdkDmabufDownloader *downloader,
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
gdk_dmabuf_gl_downloader_supports (const GdkDmabufDownloader  *downloader,
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


static int
gl_format_for_memory_format (GdkMemoryFormat format)
{
  switch (gdk_memory_format_get_depth (format))
    {
    case GDK_MEMORY_U8:
      return GL_RGBA8;
    case GDK_MEMORY_U16:
      return GL_RGBA16;
    case GDK_MEMORY_FLOAT16:
      return GL_RGBA16F;
    case GDK_MEMORY_FLOAT32:
      return GL_RGBA32F;
    default:
      g_assert_not_reached ();
    }
}

static int
gl_type_for_gl_format (int format)
{
  switch (format)
    {
    case GL_RGBA8:
      return GL_UNSIGNED_BYTE;
    case GL_RGBA16:
      return GL_UNSIGNED_SHORT;
    case GL_RGBA16F:
      return GL_HALF_FLOAT;
    case GL_RGBA32F:
      return GL_FLOAT;
    default:
      g_assert_not_reached ();
    }
}

static void
create_render_target (int           width,
                      int           height,
                      int           format,
                      unsigned int *fbo_id,
                      unsigned int *texture_id)
{
  int type;
  unsigned int texture, fbo;

  type = gl_type_for_gl_format (format);

  glGenTextures (1, &texture);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, texture);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D (GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, type, NULL);

  glGenFramebuffers (1, &fbo);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  g_assert (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  *fbo_id = fbo;
  *texture_id = texture;
}

static const char vertex_shader_source[] = ""
"#version 150\n"
"in vec2 in_position;\n"
"void main() {\n"
"  gl_Position = vec4(in_position, 0.0, 1.0);\n"
"}";

static const char fragment_shader_source[] = ""
"#version 150\n"
"uniform sampler2D source;\n"
"uniform vec2 size;\n"
"out vec4 out_color;\n"
"void main() {\n"
"  vec4 in_color = texture(source, gl_FragCoord.xy / size);\n"
"  out_color = in_color;\n"
"}";

static unsigned int
create_shader (int         type,
               const char *src)
{
  unsigned int shader;
  int status;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &src, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetShaderInfoLog (shader, log_len, NULL, buffer);

      g_warning ("Compile failure in %s shader:\n%s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment",
                 buffer);

      g_free (buffer);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

static unsigned int
compile_program (const char *vs,
                 const char *fs,
                 unsigned int *source_location,
                 unsigned int *size_location)
{
  unsigned int vertex, fragment, program;
  int status;

  vertex = create_shader (GL_VERTEX_SHADER, vs);
  if (vertex == 0)
    return 0;

  fragment = create_shader (GL_FRAGMENT_SHADER, fs);
  if (fragment == 0)
    {
      glDeleteShader (vertex);
      return 0;
    }

  program = glCreateProgram ();

  glAttachShader (program, vertex);
  glAttachShader (program, fragment);

  glLinkProgram (program);

  glGetProgramiv (program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetProgramInfoLog (program, log_len, NULL, buffer);

      g_warning ("Linking failure:\n%s", buffer);

      g_free (buffer);

      glDeleteProgram (program);
      program = 0;

      goto out;
    }

  *source_location = glGetUniformLocation (program, "source");
  *size_location = glGetUniformLocation (program, "size");

  glDetachShader (program, vertex);
  glDetachShader (program, fragment);

out:
  glDeleteShader (vertex);
  glDeleteShader (fragment);

  return program;
}

typedef struct {
  unsigned int program;
  unsigned int source_location;
  unsigned int size_location;
} ProgramData;

static unsigned int
get_blit_program (GdkGLContext *context,
                  unsigned int *source_location,
                  unsigned int *size_location)
{
  ProgramData *data;

  data = g_object_get_data (G_OBJECT (context), "dmabuf-blit-program-data");
  if (!data)
    {
      data = g_new0 (ProgramData, 1);
      data->program = compile_program (vertex_shader_source,
                                       fragment_shader_source,
                                       &data->source_location,
                                       &data->size_location);
      g_object_set_data (G_OBJECT (context), "dmabuf-blit-program-data", data);
    }

  *source_location = data->source_location;
  *size_location = data->size_location;

  return data->program;
}

static void
blit_texture (GdkGLContext *context,
              int           width,
              int           height,
              unsigned int  texture,
              unsigned int  fbo)
{
  unsigned int program;
  unsigned int source_location;
  unsigned int size_location;
  unsigned int vao;
  unsigned int buffer;
  const float vertices[] = {
    -1.0f, 1.0f,
    -1.0f, -1.0f,
    1.0f, 1.0f,
    1.0f, -1.0f
  };

  program = get_blit_program (context, &source_location, &size_location);

  glGenVertexArrays (1, &vao);
  glBindVertexArray (vao);

  glGenBuffers (1, &buffer);
  glBindBuffer (GL_ARRAY_BUFFER, buffer);
  glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray (0);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, texture);
  glBindFramebuffer (GL_FRAMEBUFFER, fbo);

  glUseProgram (program);
  glUniform1i (source_location, texture);
  glUniform2f (size_location, width, height);

  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  glUseProgram (0);
  glDisableVertexAttribArray (0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);

  glDeleteBuffers (1, &buffer);
  glDeleteVertexArrays (1, &vao);
}

static void
gdk_dmabuf_gl_downloader_download (const GdkDmabufDownloader *downloader,
                                   GdkTexture                *texture,
                                   GdkMemoryFormat            format,
                                   guchar                    *data,
                                   gsize                      stride)
{
  GdkDmabufTexture *self = GDK_DMABUF_TEXTURE (texture);
  GdkGLContext *context;
  unsigned int texture_id;
  unsigned int texture_id2;
  unsigned int fbo;
  int width, height;
  int gl_format;
  GdkGLTextureBuilder *builder;
  GdkTexture *gl_texture;
  gpointer sync;
  GdkTextureDownloader *tex_downloader;

  GDK_DEBUG (DMABUF, "Using gl for downloading a dmabuf");

  context = gdk_display_get_gl_context (gdk_dmabuf_texture_get_display (self));

  gdk_gl_context_make_current (context);

  gl_format = gl_format_for_memory_format (format),

  width = gdk_texture_get_width (GDK_TEXTURE (self));
  height = gdk_texture_get_height (GDK_TEXTURE (self));

  /* 1. import the dmabuf as GL texture */
  texture_id = gdk_gl_context_import_dmabuf (context,
                                             width, height,
                                             gdk_dmabuf_texture_get_dmabuf (self));

  /* 2. create a texture to render into */
  create_render_target (width, height, gl_format, &fbo, &texture_id2);

  /* 3. copy from texture_id1 to texture_id2, using a blit shader */
  glClear (GL_COLOR_BUFFER_BIT);
  blit_texture (context, width, height, texture_id, fbo);
  sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  /* 4. create a GdkGLTexture from the rendered texture */
  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, context);
  gdk_gl_texture_builder_set_id (builder, texture_id2);
  gdk_gl_texture_builder_set_width (builder, width);
  gdk_gl_texture_builder_set_height (builder, height);
  gdk_gl_texture_builder_set_format (builder, format);
  gdk_gl_texture_builder_set_sync (builder, sync);
  gl_texture = gdk_gl_texture_builder_build (builder, NULL, NULL);

  /* 5. download it */
  tex_downloader = gdk_texture_downloader_new (gl_texture);
  gdk_texture_downloader_set_format (tex_downloader, format);
  gdk_texture_downloader_download_into (tex_downloader, data, stride);
  gdk_texture_downloader_free (tex_downloader);
  g_object_unref (gl_texture);

  /* 6. cleanup */
  glDeleteFramebuffers (1, &fbo);
  glDeleteTextures (1, &texture_id);
  glDeleteTextures (1, &texture_id2);
}

const GdkDmabufDownloader *
gdk_dmabuf_get_gl_downloader (void)
{
  static const GdkDmabufDownloader downloader = {
    gdk_dmabuf_gl_downloader_add_formats,
    gdk_dmabuf_gl_downloader_supports,
    gdk_dmabuf_gl_downloader_download,
  };

  return &downloader;
}

#endif  /* HAVE_LINUX_DMA_BUF_H && HAVE_EGL */
