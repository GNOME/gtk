/* gsknglrenderer.c
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
#include <gdk/gdksurfaceprivate.h>
#include <gsk/gskdebugprivate.h>
#include <gsk/gskrendererprivate.h>

#include "gsknglcommandqueueprivate.h"
#include "gskngldriverprivate.h"
#include "gsknglprogramprivate.h"
#include "gsknglrenderjobprivate.h"
#include "gsknglrendererprivate.h"

struct _GskNglRendererClass
{
  GskRendererClass parent_class;
};

struct _GskNglRenderer
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
  GskNglCommandQueue *command_queue;

  /* The driver manages our program state and command queues. It also
   * deals with caching textures, shaders, shadows, glyph, and icon
   * caches through various helpers.
   */
  GskNglDriver *driver;
};

G_DEFINE_TYPE (GskNglRenderer, gsk_ngl_renderer, GSK_TYPE_RENDERER)

/**
 * gsk_ngl_renderer_new:
 *
 * Creates a new #GskRenderer using the new OpenGL renderer.
 *
 * Returns: a new NGL renderer
 *
 * Since: 4.2
 */
GskRenderer *
gsk_ngl_renderer_new (void)
{
  return g_object_new (GSK_TYPE_NGL_RENDERER, NULL);
}

static gboolean
gsk_ngl_renderer_realize (GskRenderer  *renderer,
                          GdkSurface   *surface,
                          GError      **error)
{
  G_GNUC_UNUSED gint64 start_time = GDK_PROFILER_CURRENT_TIME;
  GskNglRenderer *self = (GskNglRenderer *)renderer;
  GdkGLContext *context = NULL;
  GdkGLContext *shared_context;
  GskNglDriver *driver = NULL;
  gboolean ret = FALSE;
  gboolean debug_shaders = FALSE;

  g_assert (GSK_IS_NGL_RENDERER (self));
  g_assert (GDK_IS_SURFACE (surface));

  if (self->context != NULL)
    return TRUE;

  g_assert (self->driver == NULL);
  g_assert (self->context == NULL);
  g_assert (self->command_queue == NULL);

  if (!(context = gdk_surface_create_gl_context (surface, error)) ||
      !gdk_gl_context_realize (context, error))
    goto failure;

  if (!(shared_context = gdk_surface_get_shared_data_gl_context (surface)))
    {
      g_set_error (error,
                   GDK_GL_ERROR,
                   GDK_GL_ERROR_NOT_AVAILABLE,
                   "Failed to locate shared GL context for driver");
      goto failure;
    }

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), SHADERS))
    debug_shaders = TRUE;
#endif

  if (!(driver = gsk_ngl_driver_from_shared_context (shared_context, debug_shaders, error)))
    goto failure;

  self->command_queue = gsk_ngl_driver_create_command_queue (driver, context);
  self->context = g_steal_pointer (&context);
  self->driver = g_steal_pointer (&driver);

  gsk_ngl_command_queue_set_profiler (self->command_queue,
                                      gsk_renderer_get_profiler (renderer));

  ret = TRUE;

failure:
  g_clear_object (&driver);
  g_clear_object (&context);

  gdk_profiler_end_mark (start_time, "GskNglRenderer realize", NULL);

  return ret;
}

static void
gsk_ngl_renderer_unrealize (GskRenderer *renderer)
{
  GskNglRenderer *self = (GskNglRenderer *)renderer;

  g_assert (GSK_IS_NGL_RENDERER (renderer));

  g_clear_object (&self->driver);
  g_clear_object (&self->context);
  g_clear_object (&self->command_queue);
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

static void
gsk_ngl_renderer_render (GskRenderer          *renderer,
                         GskRenderNode        *root,
                         const cairo_region_t *update_area)
{
  GskNglRenderer *self = (GskNglRenderer *)renderer;
  cairo_region_t *render_region;
  graphene_rect_t viewport;
  GskNglRenderJob *job;
  GdkSurface *surface;
  float scale_factor;

  g_assert (GSK_IS_NGL_RENDERER (renderer));
  g_assert (root != NULL);

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self->context));
  scale_factor = gdk_surface_get_scale_factor (surface);

  viewport.origin.x = 0;
  viewport.origin.y = 0;
  viewport.size.width = gdk_surface_get_width (surface) * scale_factor;
  viewport.size.height = gdk_surface_get_height (surface) * scale_factor;

  gdk_gl_context_make_current (self->context);
  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->context), update_area);

  /* Must be called *AFTER* gdk_draw_context_begin_frame() */
  render_region = get_render_region (surface, self->context);

  gsk_ngl_driver_begin_frame (self->driver, self->command_queue);
  job = gsk_ngl_render_job_new (self->driver, &viewport, scale_factor, render_region, 0);
#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), FALLBACK))
    gsk_ngl_render_job_set_debug_fallback (job, TRUE);
#endif
  gsk_ngl_render_job_render (job, root);
  gsk_ngl_driver_end_frame (self->driver);
  gsk_ngl_render_job_free (job);

  gdk_gl_context_make_current (self->context);
  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->context));

  gsk_ngl_driver_after_frame (self->driver);

  cairo_region_destroy (render_region);
}

static GdkTexture *
gsk_ngl_renderer_render_texture (GskRenderer           *renderer,
                                 GskRenderNode         *root,
                                 const graphene_rect_t *viewport)
{
  GskNglRenderer *self = (GskNglRenderer *)renderer;
  GskNglRenderTarget *render_target;
  GskNglRenderJob *job;
  GdkTexture *texture = NULL;
  guint texture_id;
  int width;
  int height;

  g_assert (GSK_IS_NGL_RENDERER (renderer));
  g_assert (root != NULL);

  width = ceilf (viewport->size.width);
  height = ceilf (viewport->size.height);

  if (gsk_ngl_driver_create_render_target (self->driver,
                                           width, height,
                                           GL_NEAREST, GL_NEAREST,
                                           &render_target))
    {
      gsk_ngl_driver_begin_frame (self->driver, self->command_queue);
      job = gsk_ngl_render_job_new (self->driver, viewport, 1, NULL, render_target->framebuffer_id);
#ifdef G_ENABLE_DEBUG
      if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (self), FALLBACK))
        gsk_ngl_render_job_set_debug_fallback (job, TRUE);
#endif
      gsk_ngl_render_job_render_flipped (job, root);
      texture_id = gsk_ngl_driver_release_render_target (self->driver, render_target, FALSE);
      texture = gsk_ngl_driver_create_gdk_texture (self->driver, texture_id);
      gsk_ngl_driver_end_frame (self->driver);
      gsk_ngl_render_job_free (job);

      gsk_ngl_driver_after_frame (self->driver);
    }

  return g_steal_pointer (&texture);
}

static void
gsk_ngl_renderer_dispose (GObject *object)
{
#ifdef G_ENABLE_DEBUG
  GskNglRenderer *self = (GskNglRenderer *)object;

  g_assert (self->driver == NULL);
#endif

  G_OBJECT_CLASS (gsk_ngl_renderer_parent_class)->dispose (object);
}

static void
gsk_ngl_renderer_class_init (GskNglRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  object_class->dispose = gsk_ngl_renderer_dispose;

  renderer_class->realize = gsk_ngl_renderer_realize;
  renderer_class->unrealize = gsk_ngl_renderer_unrealize;
  renderer_class->render = gsk_ngl_renderer_render;
  renderer_class->render_texture = gsk_ngl_renderer_render_texture;
}

static void
gsk_ngl_renderer_init (GskNglRenderer *self)
{
}

gboolean
gsk_ngl_renderer_try_compile_gl_shader (GskNglRenderer  *renderer,
                                        GskGLShader     *shader,
                                        GError         **error)
{
  GskNglProgram *program;

  g_return_val_if_fail (GSK_IS_NGL_RENDERER (renderer), FALSE);
  g_return_val_if_fail (shader != NULL, FALSE);

  program = gsk_ngl_driver_lookup_shader (renderer->driver, shader, error);

  return program != NULL;
}
