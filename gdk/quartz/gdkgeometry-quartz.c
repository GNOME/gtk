/* gdkgeometry-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkprivate-quartz.h"

void
_gdk_quartz_window_translate (GdkWindow      *window,
                              cairo_region_t *area,
                              gint            dx,
                              gint            dy)
{
  cairo_region_t *invalidate, *scrolled;
  GdkWindowImplQuartz *impl = (GdkWindowImplQuartz *)window->impl;
  GdkRectangle extents;

  cairo_region_get_extents (area, &extents);

  [impl->view scrollRect:NSMakeRect (extents.x - dx, extents.y - dy,
                                     extents.width, extents.height)
              by:NSMakeSize (dx, dy)];

  if (impl->needs_display_region)
    {
      cairo_region_t *intersection;

      /* Invalidate already invalidated area that was moved at new
       * location.
       */
      intersection = cairo_region_copy (impl->needs_display_region);
      cairo_region_intersect (intersection, area);
      cairo_region_translate (intersection, dx, dy);

      _gdk_quartz_window_set_needs_display_in_region (window, intersection);
      cairo_region_destroy (intersection);
    }

  /* Calculate newly exposed area that needs invalidation */
  scrolled = cairo_region_copy (area);
  cairo_region_translate (scrolled, dx, dy);

  invalidate = cairo_region_copy (area);
  cairo_region_subtract (invalidate, scrolled);
  cairo_region_destroy (scrolled);

  _gdk_quartz_window_set_needs_display_in_region (window, invalidate);
  cairo_region_destroy (invalidate);
}

gboolean
_gdk_quartz_window_queue_antiexpose (GdkWindow *window,
                                     cairo_region_t *area)
{
  return FALSE;
}
