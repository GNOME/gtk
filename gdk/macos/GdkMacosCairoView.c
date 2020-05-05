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

#import "GdkMacosCairoView.h"

#include "gdkinternals.h"

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
  cairo_surface_t *dest;
  cairo_t *cr;
  guint n_rects;

  if (self->surface == NULL || self->region == NULL)
    return;

  dest = cairo_quartz_surface_create_for_cg_context (NSGraphicsContext.currentContext.CGContext,
                                                     self.bounds.size.width,
                                                     self.bounds.size.height);
  cr = cairo_create (dest);

#if 0
  n_rects = cairo_region_num_rectangles (self->region);

  g_print ("drawRect: %d rects\n", n_rects);

  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t r;

      cairo_region_get_rectangle (self->region, i, &r);
      cairo_rectangle (cr, r.x, r.y, r.width, r.height);
    }

  cairo_set_source_surface (cr, self->surface, 0, 0);
  cairo_rectangle (cr, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
  cairo_paint (cr);
#endif

  cairo_rectangle (cr, 0, 0, self.bounds.size.width, self.bounds.size.height);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_fill (cr);

  cairo_destroy (cr);

  cairo_surface_flush (dest);
  cairo_surface_destroy (dest);
}

@end
