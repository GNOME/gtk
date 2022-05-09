/* gskglrenderer.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdkprofilerprivate.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdksurfaceprivate.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkcolorspaceprivate.h>
#include <gsk/gskdebugprivate.h>
#include <gsk/gskrendererprivate.h>
#include <gsk/gskrendernodeprivate.h>

#include "gskglcommandqueueprivate.h"
#include "gskgldriverprivate.h"
#include "gskglprogramprivate.h"
#include "gskglrenderjobprivate.h"
#include "gskglrendererprivate.h"

struct _GskGLRendererClass
{
  GskRendererClass parent_class;
};

struct _GskGLRenderer
{
  GskRenderer parent_instance;

  /* This context is used to swap buffers when we are rendering directly
   * to a GDK surface. It is also used to locate the shared driver for
   * the display that we use to drive the command queue.
   */
  GdkGLContext *context;

  /* Our command queue is private to this renderer and talks to the GL
   * context for our target surface. This ensure that framebuffer 0 matches
   * the surface we care about. Since the context is shared with other
   * contexts from other renderers on the display, texture atlases,
   * programs, and other objects are available to them all.
   */
  GskGLCommandQueue *command_queue;

  /* The driver manages our program state and command queues. It also
   * deals with caching textures, shaders, shadows, glyph, and icon
   * caches through various helpers.
   */
  GskGLDriver *driver;
};

G_DEFINE_TYPE (GskGLRenderer, gsk_gl_renderer, GSK_TYPE_RENDERER)

/**
 * gsk_gl_renderer_new:
 *
 * Creates a new `GskRenderer` using the new OpenGL renderer.
 *
 * Returns: a new GL renderer
 *
 * Since: 4.2
 */
GskRenderer *
gsk_gl_renderer_new (void)
{
  return g_object_new (GSK_TYPE_GL_RENDERER, NULL);
}

static gboolean
gsk_gl_renderer_realize (GskRenderer  *renderer,
                         GdkSurface   *surface,
                         GError      **error)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  GskGLRenderer *self = (GskGLRenderer *)renderer;
  GdkGLContext *context = NULL;
  GskGLDriver *driver = NULL;
  GdkDisplay *display;
  gboolean ret = FALSE;
  gboolean debug_shaders = FALSE;
  GdkGLAPI api;

  if (self->context != NULL)
    return TRUE;

  g_assert (self->driver == NULL);
  g_assert (self->context == NULL);
  g_assert (self->command_queue == NULL);

  if (surface == NULL)
    {
      display = gdk_display_get_default (); /* FIXME: allow different displays somehow ? */
      context = gdk_display_create_gl_context (display, error);
    }
  else
    {
      display = gdk_surface_get_display (surface);
      context = gdk_surface_create_gl_context (surface, error);
    }

  if (!context || !gdk_gl_context_realize (context, error))
    goto failure;

  api = gdk_gl_context_get_api (context);
  if (api == GDK_GL_API_GLES)
    {
      gdk_gl_context_make_current (context);

      if (!gdk_gl_context_has_vertex_half_float (context))
        {
          int major, minor;

          gdk_gl_context_get_version (context, &major, &minor);
          g_set_error (error,
                       GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                       _("This GLES %d.%d implementation does not support half-float vertex data"),
                       major, minor);
          goto failure;
        }
    }

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), SHADERS))
    debug_shaders = TRUE;
#endif

  if (!(driver = gsk_gl_driver_for_display (display, debug_shaders, error)))
    goto failure;

  self->command_queue = gsk_gl_driver_create_command_queue (driver, context);
  self->context = g_steal_pointer (&context);
  self->driver = g_steal_pointer (&driver);

  gsk_gl_command_queue_set_profiler (self->command_queue,
                                     gsk_renderer_get_profiler (renderer));

  ret = TRUE;

failure:
  g_clear_object (&driver);
  g_clear_object (&context);

  gdk_profiler_end_mark (start_time, "realize GskGLRenderer", NULL);

  /* Assert either all or no state was set */
  g_assert ((ret && self->driver != NULL && self->context != NULL && self->command_queue != NULL) ||
            (!ret && self->driver == NULL && self->context == NULL && self->command_queue == NULL));

  return ret;
}

static void
gsk_gl_renderer_unrealize (GskRenderer *renderer)
{
  GskGLRenderer *self = (GskGLRenderer *)renderer;

  g_assert (GSK_IS_GL_RENDERER (renderer));

  gdk_gl_context_make_current (self->context);

  g_clear_object (&self->driver);
  g_clear_object (&self->command_queue);
  g_clear_object (&self->context);
}

static cairo_region_t *
get_render_region (GdkSurface   *surface,
                   GdkGLContext *context)
{
  const cairo_region_t *damage;
  GdkRectangle whole_surface;
  GdkRectangle extents;

  g_assert (GDK_IS_SURFACE (surface));
  g_assert (GDK_IS_GL_CONTEXT (context));

  whole_surface.x = 0;
  whole_surface.y = 0;
  whole_surface.width = gdk_surface_get_width (surface);
  whole_surface.height = gdk_surface_get_height (surface);

  /* Damage does not have scale factor applied so we can compare it to
   * @whole_surface which also doesn't have the scale factor applied.
   */
  damage = gdk_draw_context_get_frame_region (GDK_DRAW_CONTEXT (context));

  if (cairo_region_contains_rectangle (damage, &whole_surface) == CAIRO_REGION_OVERLAP_IN)
    return NULL;

  /* If the extents match the full-scene, do the same as above */
  cairo_region_get_extents (damage, &extents);
  if (gdk_rectangle_equal (&extents, &whole_surface))
    return NULL;

  /* Draw clipped to the bounding-box of the region. */
  return cairo_region_create_rectangle (&extents);
}

static gboolean
update_area_requires_clear (GdkSurface           *surface,
                            const cairo_region_t *update_area)
{
  cairo_rectangle_int_t rect;
  guint n_rects;

  g_assert (GDK_IS_SURFACE (surface));

  /* No opaque region, assume we have to clear */
  if (surface->opaque_region == NULL)
    return TRUE;

  /* If the update_area is the whole surface, then clear it
   * because many drivers optimize for this by avoiding extra
   * work to reload any contents.
   */
  if (update_area == NULL)
    return TRUE;

  if (cairo_region_num_rectangles (update_area) == 1)
    {
      cairo_region_get_rectangle (update_area, 0, &rect);

      if (rect.x == 0 &&
          rect.y == 0 &&
          rect.width == surface->width &&
          rect.height == surface->height)
        return TRUE;
    }

  /* If the entire surface is opaque, then we can skip clearing
   * (with the exception of full surface clearing above).
   */
  if (cairo_region_num_rectangles (surface->opaque_region) == 1)
    {
      cairo_region_get_rectangle (surface->opaque_region, 0, &rect);

      if (rect.x == 0 &&
          rect.y == 0 &&
          rect.width == surface->width &&
          rect.height == surface->height)
        return FALSE;
    }

  /* If any update_area rectangle overlaps our transparent
   * regions, then we need to clear the area.
   */
  n_rects = cairo_region_num_rectangles (update_area);
  for (guint i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (update_area, i, &rect);
      if (cairo_region_contains_rectangle (surface->opaque_region, &rect) != CAIRO_REGION_OVERLAP_IN)
        return TRUE;
    }

  return FALSE;
}

static void
gsk_gl_renderer_render (GskRenderer          *renderer,
                        GskRenderNode        *root,
                        const cairo_region_t *update_area)
{
  GskGLRenderer *self = (GskGLRenderer *)renderer;
  cairo_region_t *render_region;
  graphene_rect_t viewport;
  GskGLRenderJob *job;
  GdkSurface *surface;
  gboolean clear_framebuffer;
  float scale_factor;

  g_assert (GSK_IS_GL_RENDERER (renderer));
  g_assert (root != NULL);

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self->context));
  scale_factor = gdk_surface_get_scale_factor (surface);

  viewport.origin.x = 0;
  viewport.origin.y = 0;
  viewport.size.width = gdk_surface_get_width (surface) * scale_factor;
  viewport.size.height = gdk_surface_get_height (surface) * scale_factor;

  gdk_draw_context_begin_frame_full (GDK_DRAW_CONTEXT (self->context),
                                     gsk_render_node_prefers_high_depth (root),
                                     update_area);

  gdk_gl_context_make_current (self->context);

  /* Must be called *AFTER* gdk_draw_context_begin_frame() */
  render_region = get_render_region (surface, self->context);
  clear_framebuffer = update_area_requires_clear (surface, render_region);

  gsk_gl_driver_begin_frame (self->driver, self->command_queue);
  job = gsk_gl_render_job_new (self->driver, &viewport, scale_factor, render_region, 0, clear_framebuffer);
#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), FALLBACK))
    gsk_gl_render_job_set_debug_fallback (job, TRUE);
#endif
  gsk_gl_render_job_render (job, root);
  gsk_gl_driver_end_frame (self->driver);
  gsk_gl_render_job_free (job);

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->context));

  gsk_gl_driver_after_frame (self->driver);

  cairo_region_destroy (render_region);
}

static GdkTexture *
gsk_gl_renderer_render_texture (GskRenderer           *renderer,
                                GskRenderNode         *root,
                                const graphene_rect_t *viewport)
{
  GskGLRenderer *self = (GskGLRenderer *)renderer;
  GskGLRenderTarget *render_target;
  GskGLRenderJob *job;
  GdkTexture *texture;
  guint texture_id;
  int width, height, max_size;
  int format;

  g_assert (GSK_IS_GL_RENDERER (renderer));
  g_assert (root != NULL);

  width = ceilf (viewport->size.width);
  height = ceilf (viewport->size.height);
  max_size = self->command_queue->max_texture_size;
  if (width > max_size || height > max_size)
    {
      gsize x, y, size, stride;
      GBytes *bytes;
      guchar *data;

      stride = width * 4;
      size = stride * height;
      data = g_malloc_n (stride, height);

      for (y = 0; y < height; y += max_size)
        {
          for (x = 0; x < width; x += max_size)
            {
              texture = gsk_gl_renderer_render_texture (renderer, root,
                                                        &GRAPHENE_RECT_INIT (x, y,
                                                                             MIN (max_size, viewport->size.width - x),
                                                                             MIN (max_size, viewport->size.height - y)));
              gdk_texture_download (texture,
                                    data + stride * y + x * 4,
                                    stride);
              g_object_unref (texture);
            }
        }

      bytes = g_bytes_new_take (data, size);
      texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT, bytes, stride);
      g_bytes_unref (bytes);
      return texture;
    }

  format = gsk_render_node_prefers_high_depth (root) ? GL_RGBA32F : GL_RGBA8;

  gdk_gl_context_make_current (self->context);

  if (gsk_gl_driver_create_render_target (self->driver,
                                          width, height,
                                          format,
                                          GL_NEAREST, GL_NEAREST,
                                          &render_target))
    {
      gsk_gl_driver_begin_frame (self->driver, self->command_queue);
      job = gsk_gl_render_job_new (self->driver, viewport, 1, NULL, render_target->framebuffer_id, TRUE);
#ifdef G_ENABLE_DEBUG
      if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), FALLBACK))
        gsk_gl_render_job_set_debug_fallback (job, TRUE);
#endif
      gsk_gl_render_job_render_flipped (job, root);
      texture_id = gsk_gl_driver_release_render_target (self->driver, render_target, FALSE);
      texture = gsk_gl_driver_create_gdk_texture (self->driver,
                                                  texture_id,
                                                  GDK_GL_TEXTURE_PREMULTIPLIED,
                                                  gdk_color_space_get_srgb_linear ());
      gsk_gl_driver_end_frame (self->driver);
      gsk_gl_render_job_free (job);

      gsk_gl_driver_after_frame (self->driver);
    }
  else
    {
      g_assert_not_reached ();
    }

  return g_steal_pointer (&texture);
}

static void
gsk_gl_renderer_dispose (GObject *object)
{
  GskGLRenderer *self = (GskGLRenderer *)object;

  if (self->driver != NULL)
    g_critical ("Attempt to dispose %s without calling gsk_renderer_unrealize()",
                G_OBJECT_TYPE_NAME (self));

  G_OBJECT_CLASS (gsk_gl_renderer_parent_class)->dispose (object);
}

static void
gsk_gl_renderer_class_init (GskGLRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  object_class->dispose = gsk_gl_renderer_dispose;

  renderer_class->realize = gsk_gl_renderer_realize;
  renderer_class->unrealize = gsk_gl_renderer_unrealize;
  renderer_class->render = gsk_gl_renderer_render;
  renderer_class->render_texture = gsk_gl_renderer_render_texture;
}

static void
gsk_gl_renderer_init (GskGLRenderer *self)
{
}

gboolean
gsk_gl_renderer_try_compile_gl_shader (GskGLRenderer  *renderer,
                                       GskGLShader    *shader,
                                       GError        **error)
{
  GskGLProgram *program;

  g_return_val_if_fail (GSK_IS_GL_RENDERER (renderer), FALSE);
  g_return_val_if_fail (shader != NULL, FALSE);

  program = gsk_gl_driver_lookup_shader (renderer->driver, shader, error);

  return program != NULL;
}

typedef struct {
  GskRenderer parent_instance;
} GskNglRenderer;

typedef struct {
  GskRendererClass parent_class;
} GskNglRendererClass;

G_DEFINE_TYPE (GskNglRenderer, gsk_ngl_renderer, GSK_TYPE_RENDERER)

static void
gsk_ngl_renderer_init (GskNglRenderer *renderer)
{
}

static gboolean
gsk_ngl_renderer_realize (GskRenderer  *renderer,
                          GdkSurface   *surface,
                          GError      **error)
{
  g_set_error_literal (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "please use the GL renderer instead");
  return FALSE;
}

static void
gsk_ngl_renderer_class_init (GskNglRendererClass *class)
{
  GSK_RENDERER_CLASS (class)->realize = gsk_ngl_renderer_realize;
}

/**
 * gsk_ngl_renderer_new:
 *
 * Same as gsk_gl_renderer_new().
 *
 * Returns: (transfer full): a new GL renderer
 *
 * Deprecated: 4.4: Use gsk_gl_renderer_new()
 */
GskRenderer *
gsk_ngl_renderer_new (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_object_new (gsk_ngl_renderer_get_type (), NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
}
