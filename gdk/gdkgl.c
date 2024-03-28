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

static cairo_user_data_key_t direct_key;

void
gdk_cairo_surface_mark_as_direct (cairo_surface_t *surface,
                                  GdkWindow *window)
{
  cairo_surface_set_user_data (surface, &direct_key,
                               g_object_ref (window),  g_object_unref);
}

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
    }
  return "unknown";
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
  GdkWindow *window = gdk_gl_context_get_window (paint_context);
  int window_scale = gdk_window_get_scale_factor (window);
  float w = gdk_window_get_width (window) * window_scale;
  float h = gdk_window_get_height (window) * window_scale;
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
 * @window: The window we're rendering for (not necessarily into)
 * @source: The GL ID of the source buffer
 * @source_type: The type of the @source
 * @buffer_scale: The scale-factor that the @source buffer is allocated for
 * @x: The source x position in @source to start copying from in GL coordinates
 * @y: The source y position in @source to start copying from in GL coordinates
 * @width: The width of the region to draw
 * @height: The height of the region to draw
 *
 * This is the main way to draw GL content in GTK+. It takes a render buffer ID 
 * (@source_type == #GL_RENDERBUFFER) or a texture id (@source_type == #GL_TEXTURE)
 * and draws it onto @cr with an OVER operation, respecting the current clip.
 * The top left corner of the rectangle specified by @x, @y, @width and @height
 * will be drawn at the current (0,0) position of the cairo_t.
 *
 * This will work for *all* cairo_t, as long as @window is realized, but the
 * fallback implementation that reads back the pixels from the buffer may be
 * used in the general case. In the case of direct drawing to a window with
 * no special effects applied to @cr it will however use a more efficient
 * approach.
 *
 * For #GL_RENDERBUFFER the code will always fall back to software for buffers
 * with alpha components, so make sure you use #GL_TEXTURE if using alpha.
 *
 * Calling this may change the current GL context.
 *
 * Since: 3.16
 */
void
gdk_cairo_draw_from_gl (cairo_t              *cr,
                        GdkWindow            *window,
                        int                   source,
                        int                   source_type,
                        int                   buffer_scale,
                        int                   x,
                        int                   y,
                        int                   width,
                        int                   height)
{
  GdkGLContext *paint_context, *current_context;
  cairo_surface_t *image;
  cairo_matrix_t matrix;
  int dx, dy, window_scale;
  gboolean trivial_transform;
  cairo_surface_t *group_target;
  GdkWindow *direct_window, *impl_window;
  guint framebuffer;
  int alpha_size = 0;
  cairo_region_t *clip_region;
  GdkGLContextPaintData *paint_data;
  GLsync sync;

  impl_window = window->impl_window;

  window_scale = gdk_window_get_scale_factor (impl_window);

  paint_context = gdk_window_get_paint_gl_context (window, NULL);
  if (paint_context == NULL)
    {
      g_warning ("gdk_cairo_draw_gl_render_buffer failed - no paint context");
      return;
    }

  clip_region = gdk_cairo_region_from_clip (cr);
  current_context = gdk_gl_context_get_current ();

  if ((current_context != NULL) && (current_context != paint_context) &&
      gdk_gl_context_has_sync (current_context))
    sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  else
    sync = NULL;

  gdk_gl_context_make_current (paint_context);

  if (sync)
    {
      glWaitSync (sync, 0, GL_TIMEOUT_IGNORED);
      glDeleteSync (sync);
      sync = NULL;
    }

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

      if (gdk_gl_context_has_tex_param (paint_context))
        glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE,  &alpha_size);
      else
        alpha_size = 1;
    }
  else
    {
      g_warning ("Unsupported gl source type %d\n", source_type);
      return;
    }

  group_target = cairo_get_group_target (cr);
  direct_window = cairo_surface_get_user_data (group_target, &direct_key);

  cairo_get_matrix (cr, &matrix);

  dx = matrix.x0;
  dy = matrix.y0;

  /* Trivial == integer-only translation */
  trivial_transform =
    (double)dx == matrix.x0 && (double)dy == matrix.y0 &&
    matrix.xx == 1.0 && matrix.xy == 0.0 &&
    matrix.yx == 0.0 && matrix.yy == 1.0;

  /* For direct paint of non-alpha renderbuffer, we can
     just do a bitblit */
  if ((_gdk_gl_flags & GDK_GL_SOFTWARE_DRAW_GL) == 0 &&
      source_type == GL_RENDERBUFFER &&
      alpha_size == 0 &&
      direct_window != NULL &&
      direct_window->current_paint.use_gl &&
      gdk_gl_context_has_framebuffer_blit (paint_context) &&
      trivial_transform &&
      clip_region != NULL)
    {
      int unscaled_window_height;
      int i;

      /* Create a framebuffer with the source renderbuffer and
         make it the current target for reads */
      framebuffer = paint_data->tmp_framebuffer;
      glBindFramebuffer (GL_FRAMEBUFFER, framebuffer);
      glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_RENDERBUFFER, source);
      glBindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);

      /* Translate to impl coords */
      cairo_region_translate (clip_region, dx, dy);

      glEnable (GL_SCISSOR_TEST);

      gdk_window_get_unscaled_size (impl_window, NULL, &unscaled_window_height);

      /* We can use glDrawBuffer on OpenGL only; on GLES 2.0 we are already
       * double buffered so we don't need it...
       */
      if (!gdk_gl_context_get_use_es (paint_context))
        glDrawBuffer (GL_BACK);
      else
        {
          int maj, min;

          gdk_gl_context_get_version (paint_context, &maj, &min);

          /* ... but on GLES 3.0 we can use the vectorized glDrawBuffers
           * call.
           */
          if ((maj * 100 + min) >= 300)
            {
              static const GLenum buffers[] = { GL_BACK };

              glDrawBuffers (G_N_ELEMENTS (buffers), buffers);
            }
        }

#define FLIP_Y(_y) (unscaled_window_height - (_y))

      for (i = 0; i < cairo_region_num_rectangles (clip_region); i++)
        {
          cairo_rectangle_int_t clip_rect, dest;

          cairo_region_get_rectangle (clip_region, i, &clip_rect);
          clip_rect.x *= window_scale;
          clip_rect.y *= window_scale;
          clip_rect.width *= window_scale;
          clip_rect.height *= window_scale;

          glScissor (clip_rect.x, FLIP_Y (clip_rect.y + clip_rect.height),
                     clip_rect.width, clip_rect.height);

          dest.x = dx * window_scale;
          dest.y = dy * window_scale;
          dest.width = width * window_scale / buffer_scale;
          dest.height = height * window_scale / buffer_scale;

          if (gdk_rectangle_intersect (&clip_rect, &dest, &dest))
            {
              int clipped_src_x = x + (dest.x - dx * window_scale);
              int clipped_src_y = y + (height - dest.height - (dest.y - dy * window_scale));
              glBlitFramebuffer (clipped_src_x, clipped_src_y,
                                 (clipped_src_x + dest.width), (clipped_src_y + dest.height),
                                 dest.x, FLIP_Y(dest.y + dest.height),
                                 dest.x + dest.width, FLIP_Y(dest.y),
                                 GL_COLOR_BUFFER_BIT, GL_NEAREST);
              if (impl_window->current_paint.flushed_region)
                {
                  cairo_rectangle_int_t flushed_rect;

                  flushed_rect.x = dest.x / window_scale;
                  flushed_rect.y = dest.y / window_scale;
                  flushed_rect.width = (dest.x + dest.width + window_scale - 1) / window_scale - flushed_rect.x;
                  flushed_rect.height = (dest.y + dest.height + window_scale - 1) / window_scale - flushed_rect.y;

                  cairo_region_union_rectangle (impl_window->current_paint.flushed_region,
                                                &flushed_rect);
                  cairo_region_subtract_rectangle (impl_window->current_paint.need_blend_region,
                                                   &flushed_rect);
                }
            }
        }

      glDisable (GL_SCISSOR_TEST);

      glBindFramebuffer (GL_FRAMEBUFFER, 0);

#undef FLIP_Y

    }
  /* For direct paint of alpha or non-alpha textures we can use texturing */
  else if ((_gdk_gl_flags & GDK_GL_SOFTWARE_DRAW_GL) == 0 &&
           source_type == GL_TEXTURE &&
           direct_window != NULL &&
           direct_window->current_paint.use_gl &&
           trivial_transform &&
           clip_region != NULL)
    {
      int unscaled_window_height;
      GLint texture_width;
      GLint texture_height;
      int i, n_rects, n_quads;
      GdkTexturedQuad *quads;
      cairo_rectangle_int_t clip_rect;

      /* Translate to impl coords */
      cairo_region_translate (clip_region, dx, dy);

      if (alpha_size != 0)
        {
          cairo_region_t *opaque_region, *blend_region;

          opaque_region = cairo_region_copy (clip_region);
          cairo_region_subtract (opaque_region, impl_window->current_paint.flushed_region);
          cairo_region_subtract (opaque_region, impl_window->current_paint.need_blend_region);

          if (!cairo_region_is_empty (opaque_region))
            gdk_gl_texture_from_surface (impl_window->current_paint.surface,
                                         opaque_region);

          blend_region = cairo_region_copy (clip_region);
          cairo_region_intersect (blend_region, impl_window->current_paint.need_blend_region);

          glEnable (GL_BLEND);
          if (!cairo_region_is_empty (blend_region))
            gdk_gl_texture_from_surface (impl_window->current_paint.surface,
                                         blend_region);

          cairo_region_destroy (opaque_region);
          cairo_region_destroy (blend_region);
        }

      glBindTexture (GL_TEXTURE_2D, source);

      if (gdk_gl_context_get_use_es (paint_context) ||
          !gdk_gl_context_has_tex_param (paint_context))
        {
          texture_width = width;
          texture_height = height;
        }
      else
        {
          glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texture_width);
          glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texture_height);
        }

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glEnable (GL_SCISSOR_TEST);

      gdk_window_get_unscaled_size (impl_window, NULL, &unscaled_window_height);

#define FLIP_Y(_y) (unscaled_window_height - (_y))

      cairo_region_get_extents (clip_region, &clip_rect);

      glScissor (clip_rect.x * window_scale, FLIP_Y ((clip_rect.y + clip_rect.height) * window_scale),
                 clip_rect.width * window_scale, clip_rect.height * window_scale);

      n_quads = 0;
      n_rects = cairo_region_num_rectangles (clip_region);
      quads = g_new (GdkTexturedQuad, n_rects);
      for (i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t dest;

          cairo_region_get_rectangle (clip_region, i, &clip_rect);

          clip_rect.x *= window_scale;
          clip_rect.y *= window_scale;
          clip_rect.width *= window_scale;
          clip_rect.height *= window_scale;

          dest.x = dx * window_scale;
          dest.y = dy * window_scale;
          dest.width = width * window_scale / buffer_scale;
          dest.height = height * window_scale / buffer_scale;

          if (gdk_rectangle_intersect (&clip_rect, &dest, &dest))
            {
              int clipped_src_x = x + (dest.x - dx * window_scale);
              int clipped_src_y = y + (height - dest.height - (dest.y - dy * window_scale));
              GdkTexturedQuad quad = {
                dest.x, FLIP_Y(dest.y),
                dest.x + dest.width, FLIP_Y(dest.y + dest.height),
                clipped_src_x / (float)texture_width, (clipped_src_y + dest.height) / (float)texture_height,
                (clipped_src_x + dest.width) / (float)texture_width, clipped_src_y / (float)texture_height,
              };

              quads[n_quads++] = quad;

              if (impl_window->current_paint.flushed_region)
                {
                  cairo_rectangle_int_t flushed_rect;

                  flushed_rect.x = dest.x / window_scale;
                  flushed_rect.y = dest.y / window_scale;
                  flushed_rect.width = (dest.x + dest.width + window_scale - 1) / window_scale - flushed_rect.x;
                  flushed_rect.height = (dest.y + dest.height + window_scale - 1) / window_scale - flushed_rect.y;

                  cairo_region_union_rectangle (impl_window->current_paint.flushed_region,
                                                &flushed_rect);
                  cairo_region_subtract_rectangle (impl_window->current_paint.need_blend_region,
                                                   &flushed_rect);
                }
            }
        }

      if (n_quads > 0)
        gdk_gl_texture_quads (paint_context, GL_TEXTURE_2D, n_quads, quads, FALSE);

      g_free (quads);

      if (alpha_size != 0)
        glDisable (GL_BLEND);

#undef FLIP_Y

    }
  else
    {
      /* Software fallback */
      int major, minor, version;

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
      else if (gdk_gl_context_has_texture_format_bgra (paint_context) && G_BYTE_ORDER == G_LITTLE_ENDIAN)
        glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE,
                      cairo_image_surface_get_data (image));
      else
        glReadPixels (x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                      cairo_image_surface_get_data (image));

      glPixelStorei (GL_PACK_ROW_LENGTH, 0);

      glBindFramebuffer (GL_FRAMEBUFFER, 0);

      cairo_surface_mark_dirty (image);

      /* Invert due to opengl having different origin */
      cairo_scale (cr, 1, -1);
      cairo_translate (cr, 0, -height / buffer_scale);

      cairo_set_source_surface (cr, image, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_paint (cr);

      cairo_surface_destroy (image);
    }

out:
  if (clip_region)
    cairo_region_destroy (clip_region);

}

/* This is always called with the paint context current */
void
gdk_gl_texture_from_surface (cairo_surface_t *surface,
			     cairo_region_t  *region)
{
  GdkGLContext *paint_context;
  cairo_surface_t *image;
  double device_x_offset, device_y_offset;
  cairo_rectangle_int_t rect, e;
  int n_rects, i;
  GdkWindow *window;
  int unscaled_window_height;
  unsigned int texture_id;
  int window_scale;
  double sx, sy;
  float umax, vmax;
  gboolean use_texture_rectangle;
  guint target;
  paint_context = gdk_gl_context_get_current ();
  if ((_gdk_gl_flags & GDK_GL_SOFTWARE_DRAW_SURFACE) == 0 &&
      paint_context &&
      GDK_GL_CONTEXT_GET_CLASS (paint_context)->texture_from_surface &&
      GDK_GL_CONTEXT_GET_CLASS (paint_context)->texture_from_surface (paint_context, surface, region))
    return;

  /* Software fallback */
  use_texture_rectangle = gdk_gl_context_use_texture_rectangle (paint_context);

  window = gdk_gl_context_get_window (paint_context);
  window_scale = gdk_window_get_scale_factor (window);
  gdk_window_get_unscaled_size (window, NULL, &unscaled_window_height);

  sx = sy = 1;
  cairo_surface_get_device_scale (window->current_paint.surface, &sx, &sy);

  cairo_surface_get_device_offset (surface,
                                   &device_x_offset, &device_y_offset);

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

#define FLIP_Y(_y) (unscaled_window_height - (_y))

  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);

      glScissor (rect.x * window_scale, FLIP_Y ((rect.y + rect.height) * window_scale),
                 rect.width * window_scale, rect.height * window_scale);

      e = rect;
      e.x *= sx;
      e.y *= sy;
      e.x += (int)device_x_offset;
      e.y += (int)device_y_offset;
      e.width *= sx;
      e.height *= sy;
      image = cairo_surface_map_to_image (surface, &e);

      gdk_gl_context_upload_texture (paint_context, image, e.width, e.height, target);

      cairo_surface_unmap_image (surface, image);

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
          rect.x * window_scale, FLIP_Y(rect.y * window_scale),
          (rect.x + rect.width) * window_scale, FLIP_Y((rect.y + rect.height) * window_scale),
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
