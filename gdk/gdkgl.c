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

static cairo_user_data_key_t direct_key;

void
gdk_cairo_surface_mark_as_direct (cairo_surface_t *surface,
                                  GdkWindow *window)
{
  cairo_surface_set_user_data (surface, &direct_key,
                               g_object_ref (window),  g_object_unref);
}

void
gdk_gl_texture_quad (float x1, float y1,
                     float x2, float y2,
                     float u1, float v1,
                     float u2, float v2)
{
  glBegin (GL_QUADS);
  glTexCoord2f (u1, v2);
  glVertex2f (x1, y2);

  glTexCoord2f (u2, v2);
  glVertex2f (x2, y2);

  glTexCoord2f (u2, v1);
  glVertex2f (x2, y1);

  glTexCoord2f (u1, v1);
  glVertex2f (x1, y1);
  glEnd();
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
  GdkGLContext *context;
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

  impl_window = window->impl_window;

  window_scale = gdk_window_get_scale_factor (impl_window);

  context = gdk_window_get_paint_gl_context (window, NULL);
  if (context == NULL)
    {
      g_warning ("gdk_cairo_draw_gl_render_buffer failed - no paint context");
      return;
    }

  clip_region = gdk_cairo_region_from_clip (cr);

  gdk_gl_context_make_current (context);
  paint_data = gdk_gl_context_get_paint_data (context);

  if (paint_data->tmp_framebuffer == 0)
    glGenFramebuffersEXT (1, &paint_data->tmp_framebuffer);

  if (source_type == GL_RENDERBUFFER)
    {
      glBindRenderbufferEXT (GL_RENDERBUFFER_EXT, source);
      glGetRenderbufferParameteriv (GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE,  &alpha_size);
    }
  else if (source_type == GL_TEXTURE)
    {
      glBindTexture (GL_TEXTURE_2D, source);

      glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE,  &alpha_size);
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
  if (source_type == GL_RENDERBUFFER &&
      alpha_size == 0 &&
      direct_window != NULL &&
      direct_window->current_paint.use_gl &&
      trivial_transform &&
      clip_region != NULL)
    {
      int window_height;
      int i;

      /* Create a framebuffer with the source renderbuffer and
         make it the current target for reads */
      framebuffer = paint_data->tmp_framebuffer;
      glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, framebuffer);
      glFramebufferRenderbufferEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                    GL_RENDERBUFFER_EXT, source);
      glBindFramebufferEXT (GL_DRAW_FRAMEBUFFER_EXT, 0);

      /* Translate to impl coords */
      cairo_region_translate (clip_region, dx, dy);

      glEnable (GL_SCISSOR_TEST);

      window_height = gdk_window_get_height (impl_window);
      glDrawBuffer (GL_BACK);

#define FLIP_Y(_y) (window_height*window_scale - (_y))

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
              glBlitFramebufferEXT(clipped_src_x, clipped_src_y,
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

      glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);

#undef FLIP_Y

    }
  /* For direct paint of alpha or non-alpha textures we can use texturing */
  else if (source_type == GL_TEXTURE &&
           direct_window != NULL &&
           direct_window->current_paint.use_gl &&
           trivial_transform &&
           clip_region != NULL)
    {
      int window_height;
      GLint texture_width;
      GLint texture_height;
      int i;

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

      glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &texture_width);
      glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,  &texture_height);

      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_TEXTURE_2D, source, 0);

      glEnable (GL_SCISSOR_TEST);
      glEnable (GL_TEXTURE_2D);

      window_height = gdk_window_get_height (impl_window);

#define FLIP_Y(_y) (window_height*window_scale - (_y))

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

              gdk_gl_texture_quad (dest.x, FLIP_Y(dest.y),
                                   dest.x + dest.width, FLIP_Y(dest.y + dest.height),
                                   clipped_src_x / (float)texture_width, (clipped_src_y + dest.height) / (float)texture_height,
                                   (clipped_src_x + dest.width) / (float)texture_width, clipped_src_y / (float)texture_height);

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

      if (alpha_size != 0)
        glDisable (GL_BLEND);

      glDisable (GL_TEXTURE_2D);
      glDisable (GL_SCISSOR_TEST);

#undef FLIP_Y

    }
  else
    {
      /* Software fallback */

      /* TODO: avoid reading back non-required data due to dest clip */
      image = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                  (alpha_size == 0) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32,
                                                  width, height);

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
      cairo_surface_set_device_scale (image, buffer_scale, buffer_scale);
#endif

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

      glReadPixels (x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
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
    }

  if (clip_region)
    cairo_region_destroy (clip_region);
}

void
gdk_gl_texture_from_surface (cairo_surface_t *surface,
			     cairo_region_t  *region)
{
  GdkGLContext *current;
  cairo_surface_t *image;
  double device_x_offset, device_y_offset;
  cairo_rectangle_int_t rect, e;
  int n_rects, i;
  GdkWindow *window;
  int window_height;
  unsigned int texture_id;
  int window_scale;
  double sx, sy;
  float umax, vmax;
  gboolean use_texture_rectangle;
  guint target;

  current = gdk_gl_context_get_current ();
  if (current &&
      GDK_GL_CONTEXT_GET_CLASS (current)->texture_from_surface &&
      GDK_GL_CONTEXT_GET_CLASS (current)->texture_from_surface (current, surface, region))
    return;

  /* Software fallback */
  use_texture_rectangle = gdk_gl_context_use_texture_rectangle (current);

  window = gdk_gl_context_get_window (current);
  window_scale = gdk_window_get_scale_factor (window);
  window_height = gdk_window_get_height (window);

  sx = sy = 1;
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
  cairo_surface_get_device_scale (window->current_paint.surface, &sx, &sy);
#endif

  cairo_surface_get_device_offset (surface,
                                   &device_x_offset, &device_y_offset);

  glGenTextures (1, &texture_id);
  if (use_texture_rectangle)
    target = GL_TEXTURE_RECTANGLE_ARB;
  else
    target = GL_TEXTURE_2D;

  glBindTexture (target, texture_id);
  glEnable (target);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  n_rects = cairo_region_num_rectangles (region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (region, i, &rect);

      glScissor (rect.x * window_scale, (window_height - rect.y - rect.height) * window_scale,
                 rect.width * window_scale, rect.height * window_scale);

      e = rect;
      e.x *= sx;
      e.y *= sy;
      e.x += (int)device_x_offset;
      e.y += (int)device_y_offset;
      e.width *= sx;
      e.height *= sy;
      image = cairo_surface_map_to_image (surface, &e);

      glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
      glPixelStorei (GL_UNPACK_ROW_LENGTH, cairo_image_surface_get_stride (image)/4);
      glTexImage2D (target, 0, 4, e.width, e.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    cairo_image_surface_get_data (image));
      glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);

      cairo_surface_unmap_image (surface, image);

#define FLIP_Y(_y) (window_height - (_y))

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

      gdk_gl_texture_quad (rect.x * window_scale, FLIP_Y(rect.y) * window_scale,
                           (rect.x + rect.width) * window_scale, FLIP_Y(rect.y + rect.height) * window_scale,
                           0, 0,
                           umax, vmax);
    }

  glDisable (target);
  glDeleteTextures (1, &texture_id);
}
