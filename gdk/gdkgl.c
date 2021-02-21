/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2014 Red Hat, Inc.
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

#include "gdkcairo.h"
#include "gdkglcontextprivate.h"

#include "gdkinternals.h"

#include <epoxy/gl.h>
#include <math.h>
#include <string.h>

static const char *
get_shader_type_name (int type)
{
  switch (type)
    {
    case GL_VERTEX_SHADER:
      return "vertex";
    case GL_GEOMETRY_SHADER:
      return "geometry";
    case GL_FRAGMENT_SHADER:
      return "fragment";
    default:
      return "unknown";
    }
}

static guint
create_shader (int         type,
               const char *code)
{
  guint shader;
  int status;

  shader = glCreateShader (type);
  glShaderSource (shader, 1, &code, NULL);
  glCompileShader (shader);

  glGetShaderiv (shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetShaderInfoLog (shader, log_len, NULL, buffer);

      g_warning ("Compile failure in %s shader:\n%s", get_shader_type_name (type), buffer);
      g_free (buffer);

      glDeleteShader (shader);

      return 0;
    }

  return shader;
}

static void
make_program (GdkGLContextProgram *program,
              const char          *vertex_shader_path,
              const char          *fragment_shader_path)
{
  guint vertex_shader, fragment_shader;
  GBytes *source;
  int status;

  source = g_resources_lookup_data (vertex_shader_path, 0, NULL);
  g_assert (source != NULL);
  vertex_shader = create_shader (GL_VERTEX_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);
  if (vertex_shader == 0)
    return;

  source = g_resources_lookup_data (fragment_shader_path, 0, NULL);
  g_assert (source != NULL);
  fragment_shader = create_shader (GL_FRAGMENT_SHADER, g_bytes_get_data (source, NULL));
  g_bytes_unref (source);
  if (fragment_shader == 0)
    {
      glDeleteShader (vertex_shader);
      return;
    }

  program->program = glCreateProgram ();
  glAttachShader (program->program, vertex_shader);
  glAttachShader (program->program, fragment_shader);

  glLinkProgram (program->program);

  glDetachShader (program->program, vertex_shader);
  glDetachShader (program->program, fragment_shader);

  glDeleteShader (vertex_shader);
  glDeleteShader (fragment_shader);

  glGetProgramiv (program->program, GL_LINK_STATUS, &status);
  if (status == GL_FALSE)
    {
      int log_len;
      char *buffer;

      glGetProgramiv (program->program, GL_INFO_LOG_LENGTH, &log_len);

      buffer = g_malloc (log_len + 1);
      glGetProgramInfoLog (program->program, log_len, NULL, buffer);
      g_warning ("Linker failure: %s\n", buffer);
      g_free (buffer);

      glDeleteProgram (program->program);
    }

  program->position_location = glGetAttribLocation (program->program, "position");
  program->uv_location = glGetAttribLocation (program->program, "uv");
  program->map_location = glGetUniformLocation (program->program, "map");
  program->flip_location = glGetUniformLocation (program->program, "flipColors");
}

static void
bind_vao (GdkGLContextPaintData *paint_data)
{
  if (paint_data->vertex_array_object == 0)
    {
      glGenVertexArrays (1, &paint_data->vertex_array_object);
      /* ATM we only use one VAO, so always bind it */
      glBindVertexArray (paint_data->vertex_array_object);
    }
}

static void
use_texture_gles_program (GdkGLContextPaintData *paint_data)
{
  if (paint_data->texture_2d_quad_program.program == 0)
    make_program (&paint_data->texture_2d_quad_program,
                  "/org/gtk/libgdk/glsl/gles2-texture.vs.glsl",
                  "/org/gtk/libgdk/glsl/gles2-texture.fs.glsl");

  if (paint_data->current_program != &paint_data->texture_2d_quad_program)
    {
      paint_data->current_program = &paint_data->texture_2d_quad_program;
      glUseProgram (paint_data->current_program->program);
    }
}

static void
use_texture_2d_program (GdkGLContextPaintData *paint_data)
{
  const char *vertex_shader_path = paint_data->is_legacy
    ? "/org/gtk/libgdk/glsl/gl2-texture-2d.vs.glsl"
    : "/org/gtk/libgdk/glsl/gl3-texture-2d.vs.glsl";

  const char *fragment_shader_path = paint_data->is_legacy
    ? "/org/gtk/libgdk/glsl/gl2-texture-2d.fs.glsl"
    : "/org/gtk/libgdk/glsl/gl3-texture-2d.fs.glsl";

  if (paint_data->texture_2d_quad_program.program == 0)
    make_program (&paint_data->texture_2d_quad_program, vertex_shader_path, fragment_shader_path);

  if (paint_data->current_program != &paint_data->texture_2d_quad_program)
    {
      paint_data->current_program = &paint_data->texture_2d_quad_program;
      glUseProgram (paint_data->current_program->program);
    }
}

static void
use_texture_rect_program (GdkGLContextPaintData *paint_data)
{
  const char *vertex_shader_path = paint_data->is_legacy
    ? "/org/gtk/libgdk/glsl/gl2-texture-rect.vs.glsl"
    : "/org/gtk/libgdk/glsl/gl3-texture-rect.vs.glsl";

  const char *fragment_shader_path = paint_data->is_legacy
    ? "/org/gtk/libgdk/glsl/gl2-texture-rect.fs.glsl"
    : "/org/gtk/libgdk/glsl/gl3-texture-rect.vs.glsl";

  if (paint_data->texture_rect_quad_program.program == 0)
    make_program (&paint_data->texture_rect_quad_program, vertex_shader_path, fragment_shader_path);

  if (paint_data->current_program != &paint_data->texture_rect_quad_program)
    {
      paint_data->current_program = &paint_data->texture_rect_quad_program;
      glUseProgram (paint_data->current_program->program);
    }
}

void
gdk_gl_texture_quads (GdkGLContext *paint_context,
                      guint texture_target,
                      int n_quads,
                      GdkTexturedQuad *quads,
                      gboolean flip_colors)
{
  GdkGLContextPaintData *paint_data  = gdk_gl_context_get_paint_data (paint_context);
  GdkGLContextProgram *program;
  GdkSurface *surface = gdk_gl_context_get_surface (paint_context);
  int surface_scale = gdk_surface_get_scale_factor (surface);
  float w = gdk_surface_get_width (surface) * surface_scale;
  float h = gdk_surface_get_height (surface) * surface_scale;
  int i;
  float *vertex_buffer_data;

  bind_vao (paint_data);

  if (paint_data->tmp_vertex_buffer == 0)
    glGenBuffers(1, &paint_data->tmp_vertex_buffer);

  if (paint_data->use_es)
    use_texture_gles_program (paint_data);
  else
    {
      if (texture_target == GL_TEXTURE_RECTANGLE_ARB)
        use_texture_rect_program (paint_data);
      else
        use_texture_2d_program (paint_data);
    }

  program = paint_data->current_program;

  /* Use texture unit 0 */
  glActiveTexture (GL_TEXTURE0);
  glUniform1i(program->map_location, 0);

  /* Flip 'R' and 'B' colors on GLES, if necessary */
  if (gdk_gl_context_get_use_es (paint_context))
    glUniform1i (program->flip_location, flip_colors ? 1 : 0);

  glEnableVertexAttribArray (program->position_location);
  glEnableVertexAttribArray (program->uv_location);
  glBindBuffer (GL_ARRAY_BUFFER, paint_data->tmp_vertex_buffer);

  glVertexAttribPointer (program->position_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, NULL);
  glVertexAttribPointer (program->uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void *) (sizeof(float) * 2));

#define VERTEX_SIZE 4

#define QUAD_N_VERTICES 6

#define QUAD_SIZE (VERTEX_SIZE * QUAD_N_VERTICES)

  vertex_buffer_data = g_new (float, n_quads * QUAD_SIZE);

  for (i = 0; i < n_quads; i++)
    {
      GdkTexturedQuad *quad = &quads[i];
      float vertex_data[] = {
        (quad->x1 * 2) / w - 1, (quad->y1 * 2) / h - 1, quad->u1, quad->v1,
        (quad->x1 * 2) / w - 1, (quad->y2 * 2) / h - 1, quad->u1, quad->v2,
        (quad->x2 * 2) / w - 1, (quad->y1 * 2) / h - 1, quad->u2, quad->v1,

        (quad->x2 * 2) / w - 1, (quad->y2 * 2) / h - 1, quad->u2, quad->v2,
        (quad->x1 * 2) / w - 1, (quad->y2 * 2) / h - 1, quad->u1, quad->v2,
        (quad->x2 * 2) / w - 1, (quad->y1 * 2) / h - 1, quad->u2, quad->v1,
      };

      float *vertex = &vertex_buffer_data[i * QUAD_SIZE];
      memcpy (vertex, vertex_data, sizeof(vertex_data));
    }

  glBufferData (GL_ARRAY_BUFFER, sizeof(float) * n_quads * QUAD_SIZE, vertex_buffer_data, GL_STREAM_DRAW);
  glDrawArrays (GL_TRIANGLES, 0, n_quads * QUAD_N_VERTICES);

  g_free (vertex_buffer_data);

  glDisableVertexAttribArray (program->position_location);
  glDisableVertexAttribArray (program->uv_location);
}

/* x,y,width,height describes a rectangle in the gl render buffer
   coordinate space, and its top left corner is drawn at the current
   position according to the cairo translation. */

/**
 * gdk_cairo_draw_from_gl:
 * @cr: a cairo context
 * @surface: The surface we're rendering for (not necessarily into)
 * @source: The GL ID of the source buffer
 * @source_type: The type of the @source
 * @buffer_scale: The scale-factor that the @source buffer is allocated for
 * @x: The source x position in @source to start copying from in GL coordinates
 * @y: The source y position in @source to start copying from in GL coordinates
 * @width: The width of the region to draw
 * @height: The height of the region to draw
 *
 * The main way to draw GL content in GTK.
 *
 * It takes a render buffer ID (@source_type == #GL_RENDERBUFFER) or a texture
 * id (@source_type == #GL_TEXTURE) and draws it onto @cr with an OVER operation,
 * respecting the current clip. The top left corner of the rectangle specified
 * by @x, @y, @width and @height will be drawn at the current (0,0) position of
 * the `cairo_t`.
 *
 * This will work for *all* `cairo_t`, as long as @surface is realized, but the
 * fallback implementation that reads back the pixels from the buffer may be
 * used in the general case. In the case of direct drawing to a surface with
 * no special effects applied to @cr it will however use a more efficient
 * approach.
 *
 * For #GL_RENDERBUFFER the code will always fall back to software for buffers
 * with alpha components, so make sure you use #GL_TEXTURE if using alpha.
 *
 * Calling this may change the current GL context.
 */
void
gdk_cairo_draw_from_gl (cairo_t              *cr,
                        GdkSurface            *surface,
                        int                   source,
                        int                   source_type,
                        int                   buffer_scale,
                        int                   x,
                        int                   y,
                        int                   width,
                        int                   height)
{
  GdkGLContext *paint_context;
  cairo_surface_t *image;
  guint framebuffer;
  int alpha_size = 0;
  GdkGLContextPaintData *paint_data;
  int major, minor, version;
  gboolean es_use_bgra = FALSE;

  paint_context = gdk_surface_get_paint_gl_context (surface, NULL);
  if (paint_context == NULL)
    {
      g_warning ("gdk_cairo_draw_gl_render_buffer failed - no paint context");
      return;
    }

  es_use_bgra = gdk_gl_context_use_es_bgra (paint_context);

  gdk_gl_context_make_current (paint_context);
  paint_data = gdk_gl_context_get_paint_data (paint_context);

  if (paint_data->tmp_framebuffer == 0)
    glGenFramebuffers (1, &paint_data->tmp_framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      glBindRenderbuffer (GL_RENDERBUFFER, source);
      glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE,  &alpha_size);
    }
  else if (source_type == GL_TEXTURE)
    {
      glBindTexture (GL_TEXTURE_2D, source);

      if (gdk_gl_context_get_use_es (paint_context))
        alpha_size = 1;
      else
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE,  &alpha_size);
    }
  else
    {
      g_warning ("Unsupported gl source type %d\n", source_type);
      return;
    }

  gdk_gl_context_get_version (paint_context, &major, &minor);
  version = major * 100 + minor;

  /* TODO: Use glTexSubImage2D() and do a row-by-row copy to replace
   * the GL_UNPACK_ROW_LENGTH support
   */
  if (gdk_gl_context_get_use_es (paint_context) &&
      !(version >= 300 || gdk_gl_context_has_unpack_subimage (paint_context)))
    return;

  /* TODO: avoid reading back non-required data due to dest clip */
  image = cairo_surface_create_similar_image (cairo_get_target (cr),
                                              (alpha_size == 0) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32,
                                              width, height);

  cairo_surface_set_device_scale (image, buffer_scale, buffer_scale);

  framebuffer = paint_data->tmp_framebuffer;
  glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      /* Create a framebuffer with the source renderbuffer and
         make it the current target for reads */
      glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, source);
    }
  else
    {
      glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, source, 0);
    }

  glPixelStorei (GL_PACK_ALIGNMENT, 4);
  glPixelStorei (GL_PACK_ROW_LENGTH, cairo_image_surface_get_stride (image) / 4);

  /* The implicit format conversion is going to make this path slower */
  if (!gdk_gl_context_get_use_es (paint_context))
    glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                  cairo_image_surface_get_data (image));
  else
    glReadPixels (x, y, width, height, es_use_bgra ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE,
                  cairo_image_surface_get_data (image));

  glPixelStorei (GL_PACK_ROW_LENGTH, 0);

  glBindFramebuffer (GL_FRAMEBUFFER, 0);

  cairo_surface_mark_dirty (image);

  cairo_set_source_surface (cr, image, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint (cr);

  cairo_surface_destroy (image);
}
