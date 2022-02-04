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

#import "GdkMacosCairoSubview.h"
#import "GdkMacosCairoView.h"

#include "gdkmacossurface-private.h"

@implementation GdkMacosCairoSubview

-(void)dealloc
{
  g_clear_pointer (&self->clip, g_array_unref);
  g_clear_pointer (&self->damage, g_array_unref);
  g_clear_pointer (&self->image, CGImageRelease);
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
  NSView *root_view;
  CGRect image_rect;
  CGRect abs_bounds;
  CGSize scale;
  int abs_height;

  if (self->image == NULL)
    return;

  cgContext = [[NSGraphicsContext currentContext] CGContext];
  root_view = [[self window] contentView];
  abs_bounds = [self convertRect:[self bounds] toView:root_view];
  abs_height = CGImageGetHeight (self->image);

  CGContextSaveGState (cgContext);

  /* Clip while our context is still using matching coordinates
   * to the self->clip region. This is usually just on the views
   * for the shadow areas.
   */
  CGContextAddRect (cgContext, [self bounds]);
  if (self->clip != NULL)
    {
      for (guint i = 0; i < self->clip->len; i++)
        CGContextAddRect (cgContext, g_array_index (self->clip, CGRect, i));
    }
  if (self->damage != NULL)
    {
      for (guint i = 0; i < self->damage->len; i++)
        CGContextAddRect (cgContext, g_array_index (self->damage, CGRect, i));
    }
  CGContextClip (cgContext);

  /* Scale/Translate so that the CGImageRef draws in proper format/placement */
  scale = CGSizeMake (1.0, 1.0);
  scale = CGContextConvertSizeToDeviceSpace (cgContext, scale);
  CGContextScaleCTM (cgContext, 1.0 / scale.width, 1.0 / scale.height);
  CGContextTranslateCTM (cgContext, -abs_bounds.origin.x, -abs_bounds.origin.y);
  image_rect = CGRectMake (-abs_bounds.origin.x,
                           -abs_bounds.origin.y,
                           CGImageGetWidth (self->image),
                           CGImageGetHeight (self->image));
  CGContextDrawImage (cgContext, image_rect, self->image);

  CGContextRestoreGState (cgContext);
}

-(void)setImage:(CGImageRef)theImage
     withDamage:(cairo_region_t *)region
{
  if (theImage != image)
    {
      g_clear_pointer (&image, CGImageRelease);
      if (theImage)
        image = CGImageRetain (theImage);
    }

  [self convertRegion:region toArray:&self->damage andDisplay:YES];

  for (id view in [self subviews])
    [(GdkMacosCairoSubview *)view setImage:theImage withDamage:region];
}

-(void)setOpaque:(BOOL)opaque
{
  self->_isOpaque = opaque;
}

-(void)convertRegion:(const cairo_region_t *)region
             toArray:(GArray **)array
          andDisplay:(BOOL)display
{
  NSView *root_view;
  CGRect abs_bounds;
  guint n_rects;

  if (*array == NULL)
    *array = g_array_new (FALSE, FALSE, sizeof (CGRect));
  else
    g_array_set_size (*array, 0);

  root_view = [[self window] contentView];
  abs_bounds = [self convertRect:[self bounds] toView:root_view];
  n_rects = cairo_region_num_rectangles (region);

  for (guint i = 0; i < n_rects; i++)
    {
      cairo_rectangle_int_t rect;
      CGRect nsrect;

      cairo_region_get_rectangle (region, i, &rect);
      nsrect = CGRectIntersection (abs_bounds, CGRectMake (rect.x, rect.y, rect.width, rect.height));

      if (!CGRectIsNull (nsrect))
        g_array_append_val (*array, nsrect);

      if (display)
        [self setNeedsDisplayInRect:CGRectMake (rect.x - abs_bounds.origin.x,
                                                rect.y - abs_bounds.origin.y,
                                                rect.width, rect.height)];
    }
}

-(void)setClip:(cairo_region_t*)region
{
  [self convertRegion:region toArray:&self->clip andDisplay:NO];
}

@end
