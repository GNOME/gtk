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
#include "gdkinternals.h"

/**
 * SECTION:gdkcairocontext
 * @Title: GdkCairoContext
 * @Short_description: Cairo draw context
 *
 * #GdkCairoContext is an object representing the platform-specific
 * draw context.
 *
 * #GdkCairoContexts are created for a #GdkDisplay using
 * gdk_surface_create_cairo_context(), and the context can then be used
 * to draw on that #GdkSurface.
 */

/**
 * GdkCairoContext:
 *
 * The GdkCairoContext struct contains only private fields and should not
 * be accessed directly.
 */

typedef struct _GdkCairoContextPrivate GdkCairoContextPrivate;

struct _GdkCairoContextPrivate {
  gpointer unused;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdkCairoContext, gdk_cairo_context, GDK_TYPE_DRAW_CONTEXT,
                                  G_ADD_PRIVATE (GdkCairoContext))

static cairo_surface_t *
gdk_surface_ref_impl_surface (GdkSurface *surface)
{
  return GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->ref_cairo_surface (surface);
}

static cairo_content_t
gdk_surface_get_content (GdkSurface *surface)
{
  cairo_surface_t *cairo_surface;
  cairo_content_t content;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  cairo_surface = gdk_surface_ref_impl_surface (surface);
  content = cairo_surface_get_content (cairo_surface);
  cairo_surface_destroy (cairo_surface);

  return content;
}

static void
gdk_surface_clear_backing_region (GdkSurface *surface)
{
  cairo_t *cr;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  cr = cairo_create (surface->current_paint.surface);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  gdk_cairo_region (cr, surface->current_paint.region);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static void
gdk_surface_free_current_paint (GdkSurface *surface)
{
  cairo_surface_destroy (surface->current_paint.surface);
  surface->current_paint.surface = NULL;

  cairo_region_destroy (surface->current_paint.region);
  surface->current_paint.region = NULL;

  surface->current_paint.surface_needs_composite = FALSE;
}

static void
gdk_cairo_context_begin_frame (GdkDrawContext *draw_context,
                               cairo_region_t *region)
{
  GdkRectangle clip_box;
  GdkSurface *surface;
  GdkSurfaceImplClass *impl_class;
  double sx, sy;
  gboolean needs_surface;
  cairo_content_t surface_content;

  surface = gdk_draw_context_get_surface (draw_context);
  if (surface->current_paint.surface != NULL)
    {
      g_warning ("A paint operation on the surface is alredy in progress. "
                 "This is not allowed.");
      return;
    }

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  needs_surface = TRUE;
  if (impl_class->begin_paint)
    needs_surface = impl_class->begin_paint (surface);

  surface->current_paint.region = cairo_region_copy (region);
  cairo_region_get_extents (surface->current_paint.region, &clip_box);

  surface_content = gdk_surface_get_content (surface);

  if (needs_surface)
    {
      surface->current_paint.surface = gdk_surface_create_similar_surface (surface,
                                                                           surface_content,
                                                                           MAX (clip_box.width, 1),
                                                                           MAX (clip_box.height, 1));
      sx = sy = 1;
      cairo_surface_get_device_scale (surface->current_paint.surface, &sx, &sy);
      cairo_surface_set_device_offset (surface->current_paint.surface, -clip_box.x*sx, -clip_box.y*sy);
      gdk_cairo_surface_mark_as_direct (surface->current_paint.surface, surface);

      surface->current_paint.surface_needs_composite = TRUE;
    }
  else
    {
      surface->current_paint.surface = gdk_surface_ref_impl_surface (surface);
      surface->current_paint.surface_needs_composite = FALSE;
    }

  if (!cairo_region_is_empty (surface->current_paint.region))
    gdk_surface_clear_backing_region (surface);
}

static void
gdk_cairo_context_end_frame (GdkDrawContext *draw_context,
                             cairo_region_t *painted,
                             cairo_region_t *damage)
{
  GdkSurfaceImplClass *impl_class;
  GdkSurface *surface;
  cairo_t *cr;

  surface = gdk_draw_context_get_surface (draw_context);
  if (surface->current_paint.surface == NULL)
    {
      g_warning (G_STRLOC": no preceding call to gdk_surface_begin_draw_frame(), see documentation");
      return;
    }

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->end_paint)
    impl_class->end_paint (surface);

  if (surface->current_paint.surface_needs_composite)
    {
      cairo_surface_t *cairo_surface;

      cairo_surface = gdk_surface_ref_impl_surface (surface);
      cr = cairo_create (cairo_surface);

      cairo_set_source_surface (cr, surface->current_paint.surface, 0, 0);
      gdk_cairo_region (cr, surface->current_paint.region);
      cairo_clip (cr);

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);

      cairo_destroy (cr);

      cairo_surface_flush (cairo_surface);
      cairo_surface_destroy (cairo_surface);
    }

  gdk_surface_free_current_paint (surface);
}

static void
gdk_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
}

static void
gdk_cairo_context_class_init (GdkCairoContextClass *klass)
{
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = gdk_cairo_context_begin_frame;
  draw_context_class->end_frame = gdk_cairo_context_end_frame;
  draw_context_class->surface_resized = gdk_cairo_context_surface_resized;
}

static void
gdk_cairo_context_init (GdkCairoContext *self)
{
}

/**
 * gdk_cairo_context_cairo_create:
 * @context: a #GdkCairoContext that is currently drawing
 *
 * Retrieves a Cairo context to be used to draw on the #GdkSurface
 * of @context. A call to gdk_surface_begin_draw_frame() with this
 * @context must have been done or this function will return %NULL.
 *
 * The returned context is guaranteed to be valid until
 * gdk_surface_end_draw_frame() is called.
 *
 * Returns: (transfer full) (nullable): a Cairo context to be used
 *   to draw the contents of the #GdkSurface. %NULL is returned
 *   when @contet is not drawing.
 */
cairo_t *
gdk_cairo_context_cairo_create (GdkCairoContext *self)
{
  GdkSurface *surface;
  cairo_region_t *region;
  cairo_surface_t *cairo_surface;
  cairo_t *cr;

  g_return_val_if_fail (GDK_IS_CAIRO_CONTEXT (self), NULL);

  surface = gdk_draw_context_get_surface (GDK_DRAW_CONTEXT (self));
  if (surface->drawing_context == NULL ||
      gdk_drawing_context_get_paint_context (surface->drawing_context) != GDK_DRAW_CONTEXT (self))
    return NULL;

  cairo_surface = _gdk_surface_ref_cairo_surface (surface);
  cr = cairo_create (cairo_surface);

  region = gdk_surface_get_current_paint_region (surface);
  gdk_cairo_region (cr, region);
  cairo_clip (cr);

  cairo_region_destroy (region);
  cairo_surface_destroy (cairo_surface);

  return cr;
}

