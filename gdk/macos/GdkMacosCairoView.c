/* GdkMacosCairoView.c
 *
 * Copyright © 2020 Red Hat, Inc.
 * Copyright © 2005-2007 Imendio AB
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <CoreGraphics/CoreGraphics.h>
#include <cairo-quartz.h>

#include "gdkinternals.h"

#import "GdkMacosCairoView.h"

#include "gdkmacossurface-private.h"

@implementation GdkMacosCairoView

-(void)dealloc
{
  g_clear_pointer (&self->surface, cairo_surface_destroy);
  [super dealloc];
}

-(BOOL)isOpaque
{
  return NO;
}

-(BOOL)isFlipped
{
  return YES;
}

-(void)setCairoSurfaceWithRegion:(cairo_surface_t *)cairoSurface
                     cairoRegion:(cairo_region_t *)cairoRegion
{
  guint n_rects;

  g_clear_pointer (&self->surface, cairo_surface_destroy);
  g_clear_pointer (&self->region, cairo_region_destroy);

  self->surface = cairoSurface;
  self->region = cairoRegion;

  n_rects = cairo_region_num_rectangles (cairoRegion);

  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (cairoRegion, i, &rect);
      [self setNeedsDisplayInRect:NSMakeRect (rect.x, rect.y, rect.width, rect.height)];
    }
}

-(void)drawRect:(NSRect)rect
{
  GdkMacosSurface *gdkSurface;
  CGContextRef cg_context;
  cairo_surface_t *dest;
  cairo_t *cr;
  int scale_factor;

  if (self->surface == NULL || self->region == NULL)
    return;

  gdkSurface = [self getGdkSurface];
  cg_context = _gdk_macos_surface_acquire_context (gdkSurface, TRUE, TRUE);
  scale_factor = gdk_surface_get_scale_factor (GDK_SURFACE (gdkSurface));

  dest = cairo_quartz_surface_create_for_cg_context (cg_context,
                                                     self.bounds.size.width * scale_factor,
                                                     self.bounds.size.height * scale_factor);
  cairo_surface_set_device_scale (dest, scale_factor, scale_factor);

  cr = cairo_create (dest);

  cairo_set_source_surface (cr, self->surface, 0, 0);
  gdk_cairo_region (cr, self->region);
  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_flush (dest);
  cairo_surface_destroy (dest);

  _gdk_macos_surface_release_context (gdkSurface, cg_context);
}

@end
