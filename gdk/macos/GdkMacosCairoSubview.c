/* GdkMacosCairoSubview.c
 *
 * Copyright © 2020 Red Hat, Inc.
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

#import "GdkMacosCairoSubview.h"
#import "GdkMacosCairoView.h"

#include "gdkmacossurface-private.h"

@implementation GdkMacosCairoSubview

-(void)dealloc
{
  g_clear_pointer (&self->clip, cairo_region_destroy);
  [super dealloc];
}

-(BOOL)isOpaque
{
  return _isOpaque;
}

-(BOOL)isFlipped
{
  return YES;
}

-(GdkSurface *)gdkSurface
{
  return GDK_SURFACE ([(GdkMacosBaseView *)[self superview] gdkSurface]);
}

-(void)drawRect:(NSRect)rect
{
  CGContextRef cgContext;
  GdkSurface *gdk_surface;
  cairo_surface_t *dest;
  const NSRect *rects = NULL;
  NSView *root_view;
  NSInteger n_rects = 0;
  NSRect abs_bounds;
  cairo_t *cr;
  CGSize scale;
  int scale_factor;

  if (self->cairoSurface == NULL)
    return;

  /* Acquire everything we need to do translations, drawing, etc */
  gdk_surface = [self gdkSurface];
  scale_factor = gdk_surface_get_scale_factor (gdk_surface);
  root_view = [[self window] contentView];
  cgContext = [[NSGraphicsContext currentContext] CGContext];
  abs_bounds = [self convertRect:[self bounds] toView:root_view];

  CGContextSaveGState (cgContext);

  /* Translate scaling to remove HiDPI scaling from CGContext as
   * cairo will be doing that for us already.
   */
  scale = CGSizeMake (1.0, 1.0);
  scale = CGContextConvertSizeToDeviceSpace (cgContext, scale);
  CGContextScaleCTM (cgContext, 1.0 / scale.width, 1.0 / scale.height);

  /* Create the cairo surface to draw to the CGContext and translate
   * coordinates so we can pretend we are in the same coordinate system
   * as the GDK surface.
   */
  dest = cairo_quartz_surface_create_for_cg_context (cgContext,
                                                     gdk_surface->width * scale_factor,
                                                     gdk_surface->height * scale_factor);
  cairo_surface_set_device_scale (dest, scale_factor, scale_factor);

  /* Create cairo context and translate things into the origin of
   * the topmost contentView so that we just draw at 0,0 with a
   * clip region to paint the surface.
   */
  cr = cairo_create (dest);
  cairo_translate (cr, -abs_bounds.origin.x, -abs_bounds.origin.y);

  /* Apply the clip if provided one */
  if (self->clip != NULL)
    {
      cairo_rectangle_int_t area;

      n_rects = cairo_region_num_rectangles (self->clip);
      for (guint i = 0; i < n_rects; i++)
        {
          cairo_region_get_rectangle (self->clip, i, &area);
          cairo_rectangle (cr, area.x, area.y, area.width, area.height);
        }

      cairo_clip (cr);
    }

  /* Clip the cairo context based on the rectangles to be drawn
   * within the bounding box :rect.
   */
  [self getRectsBeingDrawn:&rects count:&n_rects];
  for (NSInteger i = 0; i < n_rects; i++)
    {
      NSRect area = [self convertRect:rects[i] toView:root_view];
      cairo_rectangle (cr,
                       area.origin.x, area.origin.y,
                       area.size.width, area.size.height);
    }
  cairo_clip (cr);

  /* Now paint the surface (without blending) as we do not need
   * any compositing here. The transparent regions (like shadows)
   * are already on non-opaque layers.
   */
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, self->cairoSurface, 0, 0);
  cairo_paint (cr);

  /* Cleanup state, flush the surface to the backing layer, and
   * restore GState for future use.
   */
  cairo_destroy (cr);
  cairo_surface_flush (dest);
  cairo_surface_destroy (dest);
  CGContextRestoreGState (cgContext);
}

-(void)setCairoSurface:(cairo_surface_t *)surface
            withDamage:(cairo_region_t *)region
{
  if (surface != self->cairoSurface)
    {
      g_clear_pointer (&self->cairoSurface, cairo_surface_destroy);
      if (surface != NULL)
        self->cairoSurface = cairo_surface_reference (surface);
    }

  if (region != NULL)
    {
      NSView *root_view = [[self window] contentView];
      NSRect abs_bounds = [self convertRect:[self bounds] toView:root_view];
      guint n_rects = cairo_region_num_rectangles (region);

      for (guint i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          NSRect nsrect;

          cairo_region_get_rectangle (region, i, &rect);
          nsrect = NSMakeRect (rect.x, rect.y, rect.width, rect.height);

          if (NSIntersectsRect (abs_bounds, nsrect))
            {
              nsrect.origin.x -= abs_bounds.origin.x;
              nsrect.origin.y -= abs_bounds.origin.y;
              [self setNeedsDisplayInRect:nsrect];
            }
        }
    }

  for (id view in [self subviews])
    [(GdkMacosCairoSubview *)view setCairoSurface:surface
                                       withDamage:region];
}

-(void)setOpaque:(BOOL)opaque
{
  self->_isOpaque = opaque;
}

-(void)setClip:(cairo_region_t*)region
{
  if (region != self->clip)
    {
      g_clear_pointer (&self->clip, cairo_region_destroy);
      if (region != NULL)
        self->clip = cairo_region_reference (region);
    }
}

@end
