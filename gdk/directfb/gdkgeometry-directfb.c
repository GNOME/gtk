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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"        /* For gdk_rectangle_intersect */

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"
#include "gdkalias.h"


void
_gdk_directfb_window_get_offsets (GdkWindow *window,
                                  gint      *x_offset,
                                  gint      *y_offset)
{
  if (x_offset)
    *x_offset = 0;
  if (y_offset)
    *y_offset = 0;
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow *window,
                                        GdkRegion *area)
{
  return FALSE;
}

/**
 * gdk_window_scroll:
 * @window: a #GdkWindow
 * @dx: Amount to scroll in the X direction
 * @dy: Amount to scroll in the Y direction
 *
 * Scroll the contents of its window, both pixels and children, by
 * the given amount. Portions of the window that the scroll operation
 * brings in from offscreen areas are invalidated.
 **/
void
_gdk_directfb_window_scroll (GdkWindow *window,
                             gint       dx,
                             gint       dy)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *invalidate_region = NULL;
  GList                   *list;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  if (dx == 0 && dy == 0)
    return;

  /* Move the current invalid region */
  if (private->update_area)
    gdk_region_offset (private->update_area, dx, dy);

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      GdkRectangle  clip_rect = {  0,  0, impl->width, impl->height };
      GdkRectangle  rect      = { dx, dy, impl->width, impl->height };

      invalidate_region = gdk_region_rectangle (&clip_rect);

      if (gdk_rectangle_intersect (&rect, &clip_rect, &rect) &&
          (!private->update_area ||
           !gdk_region_rect_in (private->update_area, &rect)))
        {
          GdkRegion *region;

          region = gdk_region_rectangle (&rect);
          gdk_region_subtract (invalidate_region, region);
          gdk_region_destroy (region);

          if (impl->surface)
            {
              DFBRegion update = { rect.x, rect.y,
                                   rect.x + rect.width  - 1,
                                   rect.y + rect.height - 1 };

              impl->surface->SetClip (impl->surface, &update);
              impl->surface->Blit (impl->surface, impl->surface, NULL, dx, dy);
              impl->surface->SetClip (impl->surface, NULL);
              impl->surface->Flip (impl->surface, &update, 0);
            }
        }
    }

  for (list = private->children; list; list = list->next)
    {
      GdkWindowObject         *obj      = GDK_WINDOW_OBJECT (list->data);
      GdkDrawableImplDirectFB *obj_impl = GDK_DRAWABLE_IMPL_DIRECTFB (obj->impl);

      _gdk_directfb_move_resize_child (list->data,
                                       obj->x + dx,
                                       obj->y + dy,
                                       obj_impl->width,
                                       obj_impl->height);
    }

  if (invalidate_region)
    {
      gdk_window_invalidate_region (window, invalidate_region, TRUE);
      gdk_region_destroy (invalidate_region);
    }
}

/**
 * gdk_window_move_region:
 * @window: a #GdkWindow
 * @region: The #GdkRegion to move
 * @dx: Amount to move in the X direction
 * @dy: Amount to move in the Y direction
 *
 * Move the part of @window indicated by @region by @dy pixels in the Y
 * direction and @dx pixels in the X direction. The portions of @region
 * that not covered by the new position of @region are invalidated.
 *
 * Child windows are not moved.
 *
 * Since: 2.8
 **/
void
_gdk_directfb_window_move_region (GdkWindow       *window,
                                  const GdkRegion *region,
                                  gint             dx,
                                  gint             dy)
{
  GdkWindowObject         *private;
  GdkDrawableImplDirectFB *impl;
  GdkRegion               *window_clip;
  GdkRegion               *src_region;
  GdkRegion               *brought_in;
  GdkRegion               *dest_region;
  GdkRegion               *moving_invalid_region;
  GdkRectangle             dest_extents;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (region != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  private = GDK_WINDOW_OBJECT (window);
  impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

  if (dx == 0 && dy == 0)
    return;

  GdkRectangle  clip_rect = {  0,  0, impl->width, impl->height };
  window_clip = gdk_region_rectangle (&clip_rect);

  /* compute source regions */
  src_region = gdk_region_copy (region);
  brought_in = gdk_region_copy (region);
  gdk_region_intersect (src_region, window_clip);

  gdk_region_subtract (brought_in, src_region);
  gdk_region_offset (brought_in, dx, dy);

  /* compute destination regions */
  dest_region = gdk_region_copy (src_region);
  gdk_region_offset (dest_region, dx, dy);
  gdk_region_intersect (dest_region, window_clip);
  gdk_region_get_clipbox (dest_region, &dest_extents);

  gdk_region_destroy (window_clip);

  /* calculating moving part of current invalid area */
  moving_invalid_region = NULL;
  if (private->update_area)
    {
      moving_invalid_region = gdk_region_copy (private->update_area);
      gdk_region_intersect (moving_invalid_region, src_region);
      gdk_region_offset (moving_invalid_region, dx, dy);
    }

  /* invalidate all of the src region */
  gdk_window_invalidate_region (window, src_region, FALSE);

  /* un-invalidate destination region */
  if (private->update_area)
    gdk_region_subtract (private->update_area, dest_region);

  /* invalidate moving parts of existing update area */
  if (moving_invalid_region)
    {
      gdk_window_invalidate_region (window, moving_invalid_region, FALSE);
      gdk_region_destroy (moving_invalid_region);
    }

  /* invalidate area brought in from off-screen */
  gdk_window_invalidate_region (window, brought_in, FALSE);
  gdk_region_destroy (brought_in);

  /* Actually do the moving */
  if (impl->surface)
    {
      DFBRectangle source = { dest_extents.x - dx,
                              dest_extents.y - dy,
                              dest_extents.width,
                              dest_extents.height};
      DFBRegion destination = { dest_extents.x,
                                dest_extents.y,
                                dest_extents.x + dest_extents.width - 1,
                                dest_extents.y + dest_extents.height - 1};

      impl->surface->SetClip (impl->surface, &destination);
      impl->surface->Blit (impl->surface, impl->surface, &source, dx, dy);
      impl->surface->SetClip (impl->surface, NULL);
      impl->surface->Flip (impl->surface, &destination, 0);
    }
  gdk_region_destroy (src_region);
  gdk_region_destroy (dest_region);
}

#define __GDK_GEOMETRY_X11_C__
#include "gdkaliasdef.c"
