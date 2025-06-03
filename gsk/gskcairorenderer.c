/*
 * Copyright © 2016  Endless 
 *             2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskcairorenderer.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdktextureprivate.h"

typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;

struct _GskCairoRenderer
{
  GskRenderer parent_instance;

  GdkCairoContext *cairo_context;

  ProfileTimers profile_timers;
};

struct _GskCairoRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskCairoRenderer, gsk_cairo_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_cairo_renderer_realize (GskRenderer  *renderer,
                            GdkDisplay   *display,
                            GdkSurface   *surface,
                            gboolean      attach,
                            GError      **error)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (surface)
    self->cairo_context = gdk_surface_create_cairo_context (surface);
G_GNUC_END_IGNORE_DEPRECATIONS
  if (attach && !gdk_draw_context_attach (GDK_DRAW_CONTEXT (self->cairo_context), error))
    {
      g_clear_object (&self->cairo_context);
      return FALSE;
    }

  return TRUE;
}

static void
gsk_cairo_renderer_unrealize (GskRenderer *renderer)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);

  if (self->cairo_context)
    {
      gdk_draw_context_detach (GDK_DRAW_CONTEXT (self->cairo_context));

      g_clear_object (&self->cairo_context);
    }
}

static GdkTexture *
gsk_cairo_renderer_render_texture (GskRenderer           *renderer,
                                   GskRenderNode         *root,
                                   const graphene_rect_t *viewport)
{
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width, height;
  /* limit from cairo's source code */
#define MAX_IMAGE_SIZE 32767

  width = ceil (viewport->size.width);
  height = ceil (viewport->size.height);
  if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE)
    {
      gsize x, y, size, stride;
      GBytes *bytes;
      guchar *data;

      stride = width * 4;
      size = stride * height;
      data = g_malloc_n (stride, height);

      for (y = 0; y < height; y += MAX_IMAGE_SIZE)
        {
          for (x = 0; x < width; x += MAX_IMAGE_SIZE)
            {
              texture = gsk_cairo_renderer_render_texture (renderer, root, 
                                                           &GRAPHENE_RECT_INIT (x, y,
                                                                                MIN (MAX_IMAGE_SIZE, viewport->size.width - x),
                                                                                MIN (MAX_IMAGE_SIZE, viewport->size.height - y)));
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

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_render_node_draw_with_color_state (root, cr, GDK_COLOR_STATE_SRGB);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_cairo_renderer_render (GskRenderer          *renderer,
                           GskRenderNode        *root,
                           const cairo_region_t *region)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  graphene_rect_t opaque_tmp;
  const graphene_rect_t *opaque;
  cairo_t *cr;

  if (gsk_render_node_get_opaque_rect (root, &opaque_tmp))
    opaque = &opaque_tmp;
  else
    opaque = NULL;
  gdk_draw_context_begin_frame_full (GDK_DRAW_CONTEXT (self->cairo_context),
                                     NULL,
                                     GDK_MEMORY_U8,
                                     region,
                                     opaque);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  cr = gdk_cairo_context_cairo_create (self->cairo_context);
G_GNUC_END_IGNORE_DEPRECATIONS

  g_return_if_fail (cr != NULL);

  if (GSK_RENDERER_DEBUG_CHECK (renderer, GEOMETRY))
    {
      GdkSurface *surface = gsk_renderer_get_surface (renderer);

      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr,
                       0, 0,
                       gdk_surface_get_width (surface), gdk_surface_get_height (surface));
      cairo_set_source_rgba (cr, 0, 0, 0.85, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  gsk_render_node_draw_with_color_state (root, cr, gdk_draw_context_get_color_state (GDK_DRAW_CONTEXT (self->cairo_context)));

  cairo_destroy (cr);

  gdk_draw_context_end_frame_full (GDK_DRAW_CONTEXT (self->cairo_context), NULL);
}

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->render = gsk_cairo_renderer_render;
  renderer_class->render_texture = gsk_cairo_renderer_render_texture;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{
}

/**
 * gsk_cairo_renderer_new:
 *
 * Creates a new Cairo renderer.
 *
 * The Cairo renderer is the fallback renderer drawing in ways similar
 * to how GTK 3 drew its content. Its primary use is as comparison tool.
 *
 * The Cairo renderer is incomplete. It cannot render 3D transformed
 * content and will instead render an error marker. Its usage should be
 * avoided.
 *
 * Returns: a new Cairo renderer.
 **/
GskRenderer *
gsk_cairo_renderer_new (void)
{
  return g_object_new (GSK_TYPE_CAIRO_RENDERER, NULL);
}
