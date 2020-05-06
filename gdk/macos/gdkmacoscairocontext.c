/*
 * Copyright © 2016 Benjamin Otte
 * Copyright © 2020 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkconfig.h"

#include <CoreGraphics/CoreGraphics.h>
#include <cairo-quartz.h>

#include "gdkmacoscairocontext-private.h"
#include "gdkmacossurface-private.h"

struct _GdkMacosCairoContext
{
  GdkCairoContext parent_instance;

  cairo_surface_t *window_surface;
  cairo_surface_t *paint_surface;
};

struct _GdkMacosCairoContextClass
{
  GdkCairoContextClass parent_class;
};

G_DEFINE_TYPE (GdkMacosCairoContext, _gdk_macos_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static cairo_surface_t *
create_cairo_surface_for_surface (GdkSurface *surface)
{
  cairo_surface_t *cairo_surface;
  GdkDisplay *display;
  int scale;
  int width;
  int height;

  g_assert (GDK_IS_MACOS_SURFACE (surface));

  display = gdk_surface_get_display (surface);
  scale = gdk_surface_get_scale_factor (surface);
  width = scale * gdk_surface_get_width (surface);
  height = scale * gdk_surface_get_height (surface);

  cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  if (cairo_surface != NULL)
    cairo_surface_set_device_scale (cairo_surface, scale, scale);

  return cairo_surface;
}

static cairo_t *
_gdk_macos_cairo_context_cairo_create (GdkCairoContext *cairo_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)cairo_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  return cairo_create (self->paint_surface);
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                      cairo_region_t *region)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkRectangle clip_box;
  GdkSurface *surface;
  double sx = 1.0;
  double sy = 1.0;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));
  g_assert (self->paint_surface == NULL);

  surface = gdk_draw_context_get_surface (draw_context);

  if (self->window_surface == NULL)
    self->window_surface = create_cairo_surface_for_surface (surface);

  cairo_region_get_extents (region, &clip_box);
  self->paint_surface = gdk_surface_create_similar_surface (surface,
                                                            cairo_surface_get_content (self->window_surface),
                                                            MAX (clip_box.width, 1),
                                                            MAX (clip_box.height, 1));
  sx = sy = 1;
  cairo_surface_get_device_scale (self->paint_surface, &sx, &sy);
  cairo_surface_set_device_offset (self->paint_surface, -clip_box.x*sx, -clip_box.y*sy);
}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkSurface *surface;
  cairo_t *cr;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));
  g_assert (self->paint_surface != NULL);
  g_assert (self->window_surface != NULL);

  cr = cairo_create (self->window_surface);

  cairo_set_source_surface (cr, self->paint_surface, 0, 0);
  gdk_cairo_region (cr, painted);
  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_flush (self->window_surface);

  surface = gdk_draw_context_get_surface (draw_context);
  _gdk_macos_surface_damage_cairo (GDK_MACOS_SURFACE (surface),
                                   self->window_surface,
                                   painted);
  g_clear_pointer (&self->paint_surface, cairo_surface_destroy);
}

static void
_gdk_macos_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  g_clear_pointer (&self->window_surface, cairo_surface_destroy);
  g_clear_pointer (&self->paint_surface, cairo_surface_destroy);
}

static void
_gdk_macos_cairo_context_class_init (GdkMacosCairoContextClass *klass)
{
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  draw_context_class->begin_frame = _gdk_macos_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_macos_cairo_context_end_frame;
  draw_context_class->surface_resized = _gdk_macos_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_macos_cairo_context_cairo_create;
}

static void
_gdk_macos_cairo_context_init (GdkMacosCairoContext *self)
{
}
