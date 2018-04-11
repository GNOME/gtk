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

#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

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

G_DEFINE_TYPE_WITH_CODE (GdkCairoContext, gdk_cairo_context, GDK_TYPE_DRAW_CONTEXT,
                         G_ADD_PRIVATE (GdkCairoContext))

void
gdk_surface_begin_paint_internal (GdkSurface            *surface,
                                  const cairo_region_t *region);

static void
gdk_cairo_context_begin_frame (GdkDrawContext *draw_context,
                               cairo_region_t *region)
{
  gdk_surface_begin_paint_internal (gdk_draw_context_get_surface (draw_context),
                                    region);
}

void
gdk_surface_end_paint_internal (GdkSurface *surface);

static void
gdk_cairo_context_end_frame (GdkDrawContext *draw_context,
                             cairo_region_t *painted,
                             cairo_region_t *damage)
{
  gdk_surface_end_paint_internal (gdk_draw_context_get_surface (draw_context));
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

