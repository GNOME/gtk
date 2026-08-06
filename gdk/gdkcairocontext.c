/* GDK - The GIMP Drawing Kit
 *
 * gdkcairocontext.c: Cairo wrappers
 *
 * Copyright © 2018  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkcairocontext.h"

#include "gdkcairocontextprivate.h"

#include "gdkcairo.h"
#include "gdksurface.h"

/**
 * GdkCairoContext:
 *
 * Represents the platform-specific draw context.
 *
 * `GdkCairoContext`s are created for a surface using
 * [method@Gdk.Surface.create_cairo_context], and the context
 * can then be used to draw on that surface.
 */

typedef struct _GdkCairoContextPrivate GdkCairoContextPrivate;

struct _GdkCairoContextPrivate {
  gpointer unused;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkCairoContext, gdk_cairo_context, GDK_TYPE_DRAW_CONTEXT,
                                  G_ADD_PRIVATE (GdkCairoContext))

static void
gdk_cairo_context_finalize_frame (GdkDrawContext      *context,
                                  GdkDrawContextFrame *draw_frame)
{
  GdkCairoContextFrame *frame = (GdkCairoContextFrame *) draw_frame;

  g_clear_pointer (&frame->surface, cairo_surface_destroy);

  GDK_DRAW_CONTEXT_CLASS (gdk_cairo_context_parent_class)->finalize_frame (context, draw_frame);
}

static cairo_t *
gdk_cairo_context_default_cairo_create (GdkCairoContext *self)
{
  GdkCairoContextFrame *frame;
  cairo_t *cr;

  frame = (GdkCairoContextFrame *) gdk_draw_context_get_current_frame (GDK_DRAW_CONTEXT (self));
  if (frame == NULL || frame->surface == NULL)
    return NULL;

  cr = cairo_create (frame->surface);

  gdk_cairo_region (cr, gdk_draw_context_frame_get_damage ((GdkDrawContextFrame *) frame));
  cairo_clip (cr);

  return cr;
}

static void
gdk_cairo_context_class_init (GdkCairoContextClass *klass)
{
  GdkDrawContextClass *context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  context_class->frame_size = sizeof (GdkCairoContextFrame);

  context_class->finalize_frame = gdk_cairo_context_finalize_frame;

  klass->cairo_create = gdk_cairo_context_default_cairo_create;
}

static void
gdk_cairo_context_init (GdkCairoContext *self)
{
}

/**
 * gdk_cairo_context_cairo_create:
 * @self: a `GdkCairoContext` that is currently drawing
 *
 * Retrieves a Cairo context to be used to draw on the `GdkSurface`
 * of @context.
 *
 * A call to [method@Gdk.DrawContext.begin_frame] with this
 * @context must have been done or this function will return %NULL.
 *
 * The returned context is guaranteed to be valid until
 * [method@Gdk.DrawContext.end_frame] is called.
 *
 * Returns: (transfer full) (nullable): a Cairo context
 *   to draw on `GdkSurface
 *
 * Deprecated: 4.18: Drawing content with Cairo should be done via
 *   Cairo rendernodes, not by using renderers.
 */
cairo_t *
gdk_cairo_context_cairo_create (GdkCairoContext *self)
{
  GdkDrawContext *draw_context;
  cairo_t *cr;
  double scale;

  g_return_val_if_fail (GDK_IS_CAIRO_CONTEXT (self), NULL);

  draw_context = GDK_DRAW_CONTEXT (self);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!gdk_draw_context_is_in_frame (draw_context))
    return NULL;
G_GNUC_END_IGNORE_DEPRECATIONS

  cr = GDK_CAIRO_CONTEXT_GET_CLASS (self)->cairo_create (self);

  gdk_cairo_region (cr, gdk_draw_context_get_render_region (draw_context));
  cairo_clip (cr);

  scale = gdk_surface_get_scale (gdk_draw_context_get_surface (draw_context));
  cairo_scale (cr, scale, scale);

  return cr;
}

/**
 * gdk_cairo_context_frame_set_surface:
 * @frame: the frame
 * @surface: (transfer full) (nullable): the surface to use for rendering to this frame
 *
 * This API is called by backends to set the surface that belongs to this frame.
 *
 * The surface must have been set after begin_frame() and can be unset again in
 * end_frame(), but it must exist inbetween.
 *
 * Backends tend to use one of two methods here: Either they create and assign a
 * surface during initialize_frame() and keep reusing the surface or they recreate
 * a new surfaces every frame in begin_frame() and unset them again in end_frame().
 **/
void
gdk_cairo_context_frame_set_surface (GdkCairoContextFrame *frame,
                                     cairo_surface_t     *surface)
{
  g_clear_pointer (&frame->surface, cairo_surface_destroy);
  frame->surface = surface;
}

/*
 * gdk_cairo_context_frame_get_surface:
 * @frame: the frame
 *
 * Queries the surface that's used for this frame. A surface may only exist
 * between begin/end_frame() calls, backends are free to release the surface
 * afterwards.
 *
 * Returns: the surface to render to or NULL if none exists yet
 **/
cairo_surface_t *
gdk_cairo_context_frame_get_surface (GdkCairoContextFrame *frame)
{
  return frame->surface;
}

