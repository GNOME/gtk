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

#include "gdkconfig.h"

#import <CoreGraphics/CoreGraphics.h>
#ifdef GDK_RENDERING_VULKAN
#import <QuartzCore/CAMetalLayer.h>
#endif

#import "GdkMacosLayer.h"
#import "GdkMacosView.h"
#import "GdkMacosWindow.h"

@implementation GdkMacosView

-(id)initWithFrame:(NSRect)frame
{
  if ((self = [super initWithFrame:frame]))
    {
#ifdef GDK_RENDERING_VULKAN
      CALayer *layer = [CAMetalLayer layer];
      [layer setOpaque:NO];
#else
      GdkMacosLayer *layer = [GdkMacosLayer layer];

      [self setLayerContentsRedrawPolicy:NSViewLayerContentsRedrawNever];
#endif
      CGSize viewScale = [self convertSizeToBacking: CGSizeMake(1.0, 1.0)];
      layer.contentsScale = MIN(viewScale.width, viewScale.height);
      [self setLayer:layer];
      [self setWantsLayer:YES];
    }

  return self;
}

/**
 * If this view moves to a screen that has a different resolution scale (eg. Standard <=> Retina),
 * update the contentsScale of the layer, which will trigger a Vulkan VK_SUBOPTIMAL_KHR result, which
 * causes the  to replace the swapchain, in order to optimize rendering for the new resolution.
 */
-(BOOL)layer:(CALayer *)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow *)window
{
	if (newScale == layer.contentsScale)
    return NO;

	layer.contentsScale = newScale;
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

-(void)mouseDown:(NSEvent *)nsevent
{
  if ([(GdkMacosWindow *)[self window] needsMouseDownQuirk])
    /* We should only hit this when we are trying to click through
     * the shadow of a window into another window. Just request
     * that the application not activate this window on mouseUp.
     * See gdkmacosdisplay-translate.c for the other half of this.
     */
    [NSApp preventWindowOrdering];
  else
    [super mouseDown:nsevent];
}

-(void)setFrame:(NSRect)rect
{
  [super setFrame:rect];
  self->_nextFrameDirty = TRUE;
}

-(void)setOpaqueRegion:(const cairo_region_t *)opaqueRegion
{
  /* No-op for Vulkan/Metal layer */
  if (![[self layer] isKindOfClass: [GdkMacosLayer class]])
    return;

  [(GdkMacosLayer *)[self layer] setOpaqueRegion:opaqueRegion];
}

-(BOOL)wantsUpdateLayer
{
  return YES;
}

-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage
{
  /* No-op for Vulkan/Metal layer */
  if (![[self layer] isKindOfClass: [GdkMacosLayer class]])
    return;

  if (self->_nextFrameDirty)
    {
      self->_nextFrameDirty = FALSE;
      [[self layer] setFrame:[self frame]];
    }

  [(GdkMacosLayer *)[self layer] swapBuffer:buffer withDamage:damage];
}

@end
