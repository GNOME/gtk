/* GdkMacosCairoView.h
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

#include <cairo.h>

#import "GdkMacosBaseView.h"

@interface GdkMacosCairoView : GdkMacosBaseView
{
  cairo_surface_t *surface;
  cairo_region_t  *region;
}

-(void)setCairoSurfaceWithRegion:(cairo_surface_t *)cairoSurface
                     cairoRegion:(cairo_region_t *)region;

@end
