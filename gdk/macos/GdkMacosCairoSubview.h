/* GdkMacosCairoSubview.h
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

#include <AppKit/AppKit.h>

#define GDK_IS_MACOS_CAIRO_SUBVIEW(obj) ((obj) && [obj isKindOfClass:[GdkMacosCairoSubview class]])

@interface GdkMacosCairoSubview : NSView
{
  BOOL             _isOpaque;
  cairo_surface_t *cairoSurface;
  cairo_region_t  *clip;
}

-(void)setOpaque:(BOOL)opaque;
-(void)setCairoSurface:(cairo_surface_t *)cairoSurface
            withDamage:(cairo_region_t *)region;
-(void)setClip:(cairo_region_t*)region;

@end
