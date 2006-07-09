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

#include <config.h>

#include "gdkprivate-quartz.h"

void
gdk_window_scroll (GdkWindow *window,
                   gint       dx,
                   gint       dy)
{
  NSRect visible_nsrect;
  GdkRectangle visible_rect, scrolled_rect;
  GdkRegion *visible_region, *scrolled_region;
  GdkRectangle *rects;
  gint n_rects, i;
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  GList *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  visible_nsrect = [impl->view visibleRect];

  visible_rect.x = visible_nsrect.origin.x;
  visible_rect.y = visible_nsrect.origin.y;
  visible_rect.width = visible_nsrect.size.width;
  visible_rect.height = visible_nsrect.size.height;
  
  scrolled_rect = visible_rect;
  scrolled_rect.x += dx;
  scrolled_rect.y += dy;
  
  gdk_rectangle_intersect (&visible_rect, &scrolled_rect, &scrolled_rect);
  
  visible_region = gdk_region_rectangle (&visible_rect);
  scrolled_region = gdk_region_rectangle (&scrolled_rect);

  gdk_region_subtract (visible_region, scrolled_region);

  [impl->view scrollRect:[impl->view bounds] by:NSMakeSize(dx, dy)];

  gdk_region_get_rectangles (visible_region, &rects, &n_rects);
  for (i = 0; i < n_rects; i++)
    [impl->view setNeedsDisplayInRect:NSMakeRect (rects[i].x, rects[i].y, rects[i].width, rects[i].height)];
  
  g_free (rects);

  gdk_region_destroy (visible_region);
  gdk_region_destroy (scrolled_region);

  /* Move child windows */
  for (list = private->children; list; list = list->next)
    {
      GdkWindowObject *child = GDK_WINDOW_OBJECT (list->data);

      gdk_window_move (list->data,
		       child->x + dx,
		       child->y + dy);
    }
}

void
gdk_window_move_region (GdkWindow *window,
			GdkRegion *region,
			gint       dx,
			gint       dy)
{
  /* FIXME: Implement */
}
