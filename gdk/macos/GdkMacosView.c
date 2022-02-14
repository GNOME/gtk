/* GdkMacosView.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#import "GdkMacosLayer.h"
#import "GdkMacosView.h"

@implementation GdkMacosView

-(id)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame]))
    {
      GdkMacosLayer *layer = [GdkMacosLayer layer];

      [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
      [self setLayer:layer];
      [self setWantsLayer:YES];
    }

  return self;
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

-(void)setFrame:(NSRect)rect
{
  [super setFrame:rect];
  self->_nextFrameDirty = TRUE;
}

-(void)setOpaqueRegion:(const cairo_region_t *)opaqueRegion
{
  [(GdkMacosLayer *)[self layer] setOpaqueRegion:opaqueRegion];
}

-(BOOL)wantsUpdateLayer
{
  return YES;
}

-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage
{
  if (self->_nextFrameDirty)
    {
      self->_nextFrameDirty = FALSE;
      [[self layer] setFrame:[self frame]];
    }

  [(GdkMacosLayer *)[self layer] swapBuffer:buffer withDamage:damage];
}

@end
