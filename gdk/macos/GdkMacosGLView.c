/* GdkMacosGLView.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
#include <OpenGL/gl.h>

#include "gdkinternals.h"
#include "gdkmacossurface-private.h"

#import "GdkMacosGLView.h"

@implementation GdkMacosGLView

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

-(void)lockFocus
{
  NSOpenGLContext *context;

  [super lockFocus];

  context = [self openGLContext];

  if ([context view] != self)
    [context setView: self];
}

-(void)drawRect:(NSRect)rect
{
}

-(void)clearGLContext
{
  if (_openGLContext != nil)
    [_openGLContext clearDrawable];

  _openGLContext = nil;
}

-(void)setOpenGLContext:(NSOpenGLContext*)context
{
  if (_openGLContext != context)
    {
      if (_openGLContext != nil)
        [_openGLContext clearDrawable];

      _openGLContext = context;

      if (_openGLContext != nil)
        {
          [_openGLContext setView:self];
          [self setWantsLayer:YES];
          [self.layer setContentsGravity:kCAGravityBottomLeft];
          [_openGLContext update];
        }
    }
}

-(NSOpenGLContext *)openGLContext
{
  return _openGLContext;
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

-(BOOL)acceptsFirstMouse
{
  return YES;
}

-(BOOL)mouseDownCanMoveWindow
{
  return NO;
}

-(void)invalidateRegion:(const cairo_region_t *)region
{
  if (region != NULL)
    {
      guint n_rects = cairo_region_num_rectangles (region);

      for (guint i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          NSRect nsrect;

          cairo_region_get_rectangle (region, i, &rect);
          nsrect = NSMakeRect (rect.x, rect.y, rect.width, rect.height);

          [self setNeedsDisplayInRect:nsrect];
        }
    }
}

G_GNUC_END_IGNORE_DEPRECATIONS

@end
