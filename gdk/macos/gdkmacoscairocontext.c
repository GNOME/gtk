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

#import "GdkMacosCairoView.h"

#include "gdkmacoscairocontext-private.h"
#include "gdkmacossurface-private.h"

struct _GdkMacosCairoContext
{
  GdkCairoContext  parent_instance;

  cairo_surface_t *window_surface;
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
  int scale;
  int width;
  int height;

  g_assert (GDK_IS_MACOS_SURFACE (surface));

  scale = gdk_surface_get_scale_factor (surface);
  width = scale * gdk_surface_get_width (surface);
  height = scale * gdk_surface_get_height (surface);

  /* We use a cairo image surface here instead of a quartz surface because we
   * get strange artifacts with the quartz surface such as empty cross-fades
   * when hovering buttons. For performance, we want to be using GL rendering
   * so there isn't much point here as correctness is better.
   */
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

  return cairo_create (self->window_surface);
}

static void
_gdk_macos_cairo_context_begin_frame (GdkDrawContext *draw_context,
                                      gboolean        prefers_high_depth,
                                      cairo_region_t *region)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkSurface *surface;
  NSWindow *nswindow;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  surface = gdk_draw_context_get_surface (draw_context);
  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));

  if (self->window_surface == NULL)
    {
      self->window_surface = create_cairo_surface_for_surface (surface);
    }
  else
    {
      if (![nswindow isOpaque])
        {
          cairo_t *cr = cairo_create (self->window_surface);
          gdk_cairo_region (cr, region);
          cairo_set_source_rgba (cr, 0, 0, 0, 0);
          cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
          cairo_fill (cr);
          cairo_destroy (cr);
        }
    }
}

static void
_gdk_macos_cairo_context_end_frame (GdkDrawContext *draw_context,
                                    cairo_region_t *painted)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;
  GdkSurface *surface;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));
  g_assert (self->window_surface != NULL);

  surface = gdk_draw_context_get_surface (draw_context);
  nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface));

  if (GDK_IS_MACOS_CAIRO_VIEW (nsview))
    [(GdkMacosCairoView *)nsview setCairoSurface:self->window_surface
                                      withDamage:painted];
}

static void
_gdk_macos_cairo_context_surface_resized (GdkDrawContext *draw_context)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)draw_context;

  g_assert (GDK_IS_MACOS_CAIRO_CONTEXT (self));

  g_clear_pointer (&self->window_surface, cairo_surface_destroy);
}

static void
_gdk_macos_cairo_context_dispose (GObject *object)
{
  GdkMacosCairoContext *self = (GdkMacosCairoContext *)object;

  g_clear_pointer (&self->window_surface, cairo_surface_destroy);

  G_OBJECT_CLASS (_gdk_macos_cairo_context_parent_class)->dispose (object);
}

static void
_gdk_macos_cairo_context_class_init (GdkMacosCairoContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkCairoContextClass *cairo_context_class = GDK_CAIRO_CONTEXT_CLASS (klass);
  GdkDrawContextClass *draw_context_class = GDK_DRAW_CONTEXT_CLASS (klass);

  object_class->dispose = _gdk_macos_cairo_context_dispose;

  draw_context_class->begin_frame = _gdk_macos_cairo_context_begin_frame;
  draw_context_class->end_frame = _gdk_macos_cairo_context_end_frame;
  draw_context_class->surface_resized = _gdk_macos_cairo_context_surface_resized;

  cairo_context_class->cairo_create = _gdk_macos_cairo_context_cairo_create;
}

static void
_gdk_macos_cairo_context_init (GdkMacosCairoContext *self)
{
}
