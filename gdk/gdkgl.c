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
get_vertex_type_name (int type)
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

      g_warning ("Compile failure in %s shader:\n%s", get_vertex_type_name (type), buffer);
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
 * This is the main way to draw GL content in GTK. It takes a render buffer ID 
 * (@source_type == #GL_RENDERBUFFER) or a texture id (@source_type == #GL_TEXTURE)
 * and draws it onto @cr with an OVER operation, respecting the current clip.
 * The top left corner of the rectangle specified by @x, @y, @width and @height
 * will be drawn at the current (0,0) position of the cairo_t.
 *
 * This will work for *all* cairo_t, as long as @surface is realized, but the
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
  cairo_matrix_t matrix;
  guint framebuffer;
  int alpha_size = 0;
  cairo_region_t *clip_region;
  GdkGLContextPaintData *paint_data;
  int major, minor, version;

  paint_context = gdk_surface_get_paint_gl_context (surface, NULL);
  if (paint_context == NULL)
    {
      g_warning ("gdk_cairo_draw_gl_render_buffer failed - no paint context");
      return;
    }

  clip_region = gdk_cairo_region_from_clip (cr);

  gdk_gl_context_make_current (paint_context);
  paint_data = gdk_gl_context_get_paint_data (paint_context);

  if (paint_data->tmp_framebuffer == 0)
    glGenFramebuffersEXT (1, &paint_data->tmp_framebuffer);

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

  cairo_get_matrix (cr, &matrix);

  gdk_gl_context_get_version (paint_context, &major, &minor);
  version = major * 100 + minor;

  /* TODO: Use glTexSubImage2D() and do a row-by-row copy to replace
   * the GL_UNPACK_ROW_LENGTH support
   */
  if (gdk_gl_context_get_use_es (paint_context) &&
      !(version >= 300 || gdk_gl_context_has_unpack_subimage (paint_context)))
    goto out;

  /* TODO: avoid reading back non-required data due to dest clip */
  image = cairo_surface_create_similar_image (cairo_get_target (cr),
                                              (alpha_size == 0) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32,
                                              width, height);

  cairo_surface_set_device_scale (image, buffer_scale, buffer_scale);

  framebuffer = paint_data->tmp_framebuffer;
  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      /* Create a framebuffer with the source renderbuffer and
         make it the current target for reads */
      glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_RENDERBUFFER_EXT, source);
    }
  else
    {
      glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_TEXTURE_2D, source, 0);
    }

  glPixelStorei (GL_PACK_ALIGNMENT, 4);
  glPixelStorei (GL_PACK_ROW_LENGTH, cairo_image_surface_get_stride (image) / 4);

  /* The implicit format conversion is going to make this path slower */
  if (!gdk_gl_context_get_use_es (paint_context))
    glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                  cairo_image_surface_get_data (image));
  else
    glReadPixels (x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                  cairo_image_surface_get_data (image));

  glPixelStorei (GL_PACK_ROW_LENGTH, 0);

  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);

  cairo_surface_mark_dirty (image);

  /* Invert due to opengl having different origin */
  cairo_scale (cr, 1, -1);
  cairo_translate (cr, 0, -height / buffer_scale);

  cairo_set_source_surface (cr, image, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_paint (cr);

  cairo_surface_destroy (image);

out:
  if (clip_region)
    cairo_region_destroy (clip_region);
}

/* This is always called with the paint context current */
void
gdk_gl_texture_from_surface (cairo_surface_t *cairo_surface,
			     cairo_region_t  *region)
{
  GdkGLContext *paint_context;
  GdkDisplay *display;
  cairo_surface_t *image;
  double device_x_offset, device_y_offset;
  cairo_rectangle_int_t rect, e;
  int n_rects, i;
  GdkSurface *surface;
  int unscaled_surface_height;
  unsigned int texture_id;
  int surface_scale;
  double sx, sy;
  float umax, vmax;
  gboolean use_texture_rectangle;
  guint target;

  paint_context = gdk_gl_context_get_current ();
  if (paint_context)
    display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (paint_context));
  else
    display = NULL;

  if (paint_context &&
      GDK_DISPLAY_DEBUG_CHECK (display, GL_SOFTWARE) == 0 &&
      GDK_GL_CONTEXT_GET_CLASS (paint_context)->texture_from_surface &&
      GDK_GL_CONTEXT_GET_CLASS (paint_context)->texture_from_surface (paint_context, cairo_surface, region))
    return;

  /* Software fallback */
  use_texture_rectangle = gdk_gl_context_use_texture_rectangle (paint_context);

  surface = gdk_gl_context_get_surface (paint_context);
  surface_scale = gdk_surface_get_scale_factor (surface);
  gdk_surface_get_unscaled_size (surface, NULL, &unscaled_surface_height);

  sx = sy = 1;
  cairo_surface_get_device_scale (cairo_surface, &sx, &sy);
  cairo_surface_get_device_offset (cairo_surface, &device_x_offset, &device_y_offset);

  glGenTextures (1, &texture_id);
  if (use_texture_rectangle)
    target = GL_TEXTURE_RECTANGLE_ARB;
  else
    target = GL_TEXTURE_2D;

  glBindTexture (target, texture_id);
  glEnable (GL_SCISSOR_TEST);

  glTexParameteri (target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  n_rects = cairo_region_num_rectangles (region);

#define FLIP_Y(_y) (unscaled_surface_height - (_y))

  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);

      glScissor (rect.x * surface_scale, FLIP_Y ((rect.y + rect.height) * surface_scale),
                 rect.width * surface_scale, rect.height * surface_scale);

      e = rect;
      e.x *= sx;
      e.y *= sy;
      e.x += (int)device_x_offset;
      e.y += (int)device_y_offset;
      e.width *= sx;
      e.height *= sy;
      image = cairo_surface_map_to_image (cairo_surface, &e);

      gdk_gl_context_upload_texture (paint_context, 
                                     cairo_image_surface_get_data (image),
                                     e.width,
                                     e.height,
                                     cairo_image_surface_get_stride (image),
                                     target);

      cairo_surface_unmap_image (cairo_surface, image);

      if (use_texture_rectangle)
        {
          umax = rect.width * sx;
          vmax = rect.height * sy;
        }
      else
        {
          umax = 1.0;
          vmax = 1.0;
        }

      {
        GdkTexturedQuad quad = {
          rect.x * surface_scale, FLIP_Y(rect.y * surface_scale),
          (rect.x + rect.width) * surface_scale, FLIP_Y((rect.y + rect.height) * surface_scale),
          0, 0,
          umax, vmax,
        };

        /* We don't want to combine the quads here, because they have different textures.
         * And we don't want to upload the unused source areas to make it one texture. */
        gdk_gl_texture_quads (paint_context, target, 1, &quad, TRUE);
      }
    }

#undef FLIP_Y

  glDisable (GL_SCISSOR_TEST);
  glDeleteTextures (1, &texture_id);
}

/**
 * gdk_cairo_surface_upload_to_gl:
 * @surface: a Cairo surface
 * @target: a GL texture target
 * @width: the width of the texture @target
 * @height: the height of the texture @target
 * @context: (nullable): a #GdkGLContext, or %NULL to use the currently
 *   bound context
 *
 * Uploads the contents of a Cairo @surface to a GL texture @target.
 */
void
gdk_cairo_surface_upload_to_gl (cairo_surface_t *surface,
                                int              target,
                                int              width,
                                int              height,
                                GdkGLContext    *context)
{
  cairo_rectangle_int_t rect;
  cairo_surface_t *tmp;
  double device_x_offset, device_y_offset;

  g_return_if_fail (surface != NULL);
  g_return_if_fail (context == NULL || GDK_IS_GL_CONTEXT (context));

  if (context == NULL)
    context = gdk_gl_context_get_current ();

  cairo_surface_flush (surface);

  cairo_surface_get_device_offset (surface, &device_x_offset, &device_y_offset);

  rect.x = (int) device_x_offset;
  rect.y = (int) device_y_offset;
  rect.width = width;
  rect.height = height;
  tmp = cairo_surface_map_to_image (surface, &rect);

  gdk_gl_context_upload_texture (context,
                                 cairo_image_surface_get_data (tmp),
                                 rect.width,
                                 rect.height,
                                 cairo_image_surface_get_stride (tmp),
                                 target);

  cairo_surface_unmap_image (surface, tmp);
}
