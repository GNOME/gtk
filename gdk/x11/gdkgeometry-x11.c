/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 */

#include "config.h"

#include "gdksurface-x11.h"


void
_gdk_x11_surface_process_expose (GdkSurface    *surface,
                                gulong        serial,
                                GdkRectangle *area)
{
  cairo_region_t *invalidate_region = cairo_region_create_rectangle (area);

  if (!cairo_region_is_empty (invalidate_region))
    _gdk_surface_invalidate_for_expose (surface, invalidate_region);

  cairo_region_destroy (invalidate_region);
}
