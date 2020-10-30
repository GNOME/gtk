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
#import "GdkMacosCairoSubview.h"

#include "gdkmacossurface-private.h"

@implementation GdkMacosCairoView

-(void)dealloc
{
  g_clear_pointer (&self->opaque, g_ptr_array_unref);
  self->transparent = NULL;

  [super dealloc];
}

-(BOOL)isOpaque
{
  if ([self window])
    return [[self window] isOpaque];
  return YES;
}

-(BOOL)isFlipped
{
  return YES;
}

-(void)setNeedsDisplay:(BOOL)needsDisplay
{
  for (id child in [self subviews])
    [child setNeedsDisplay:needsDisplay];
}

-(void)setCairoSurface:(cairo_surface_t *)cairoSurface
            withDamage:(cairo_region_t *)cairoRegion
{
  for (id view in [self subviews])
    [(GdkMacosCairoSubview *)view setCairoSurface:cairoSurface
                                       withDamage:cairoRegion];
}

-(void)removeOpaqueChildren
{
  [[self->transparent subviews]
    makeObjectsPerformSelector:@selector(removeFromSuperview)];

  if (self->opaque->len)
    g_ptr_array_remove_range (self->opaque, 0, self->opaque->len);
}

-(void)setOpaqueRegion:(cairo_region_t *)region
{
  cairo_region_t *transparent_clip;
  NSRect abs_bounds;
  guint n_rects;

  if (region == NULL)
    return;

  abs_bounds = [self convertRect:[self bounds] toView:nil];
  n_rects = cairo_region_num_rectangles (region);

  /* First, we create a clip region for the transparent region to use so that
   * we dont end up exposing too much other than the corners on CSD.
   */
  transparent_clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
    abs_bounds.origin.x, abs_bounds.origin.y,
    abs_bounds.size.width, abs_bounds.size.height
  });
  cairo_region_subtract (transparent_clip, region);
  [(GdkMacosCairoSubview *)self->transparent setClip:transparent_clip];
  cairo_region_destroy (transparent_clip);

  /* The common case (at least for opaque windows and CSD) is that we will
   * have either one or two opaque rectangles. If we detect that the same
   * number of them are available as the previous, we can just resize the
   * previous ones to avoid adding/removing views at a fast rate while
   * resizing.
   */
  if (n_rects == self->opaque->len)
    {
      for (guint i = 0; i < n_rects; i++)
        {
          GdkMacosCairoSubview *child;
          cairo_rectangle_int_t rect;

          child = g_ptr_array_index (self->opaque, i);
          cairo_region_get_rectangle (region, i, &rect);

          [child setFrame:NSMakeRect (rect.x - abs_bounds.origin.x,
                                      rect.y - abs_bounds.origin.y,
                                      rect.width,
                                      rect.height)];
        }

      return;
    }

  [self removeOpaqueChildren];
  for (guint i = 0; i < n_rects; i++)
    {
      GdkMacosCairoSubview *child;
      cairo_rectangle_int_t rect;
      NSRect nsrect;

      cairo_region_get_rectangle (region, i, &rect);
      nsrect = NSMakeRect (rect.x - abs_bounds.origin.x,
                           rect.y - abs_bounds.origin.y,
                           rect.width,
                           rect.height);

      child = [[GdkMacosCairoSubview alloc] initWithFrame:nsrect];
      [child setOpaque:YES];
      [child setWantsLayer:YES];
      [self->transparent addSubview:child];
      g_ptr_array_add (self->opaque, child);
    }
}

-(NSView *)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame]))
    {
      /* An array to track all the opaque children placed into
       * the child self->transparent. This allows us to reuse them
       * when we receive a new opaque area instead of discarding
       * them on each draw.
       */
      self->opaque = g_ptr_array_new ();

      /* Setup our primary subview which will render all content that is not
       * within an opaque region (such as shadows for CSD windows). For opaque
       * windows, this will all be obscurred by other views, so it doesn't
       * matter much to have it here.
       */
      self->transparent = [[GdkMacosCairoSubview alloc] initWithFrame:frame];
      [self addSubview:self->transparent];

    }

  return self;
}

-(void)setFrame:(NSRect)rect
{
  [super setFrame:rect];
  [self->transparent setFrame:NSMakeRect (0, 0, rect.size.width, rect.size.height)];
}

-(BOOL)acceptsFirstMouse
{
  return YES;
}

-(BOOL)mouseDownCanMoveWindow
{
  return NO;
}

@end
