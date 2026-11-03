/* GSK - The GTK Scene Kit
 * Copyright © 2025  Benjamin Otte
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

#include "gskrectsnapprivate.h"

/**
 * GskRectSnap:
 *
 * The ways a rectangle can be snapped to a grid.
 *
 * A `GskRectSnap` describes how the 4 edges of a rectangle should be adjusted
 * to fit a grid - usually the rectangle describes the bounds or clip region
 * of a render operation and the grid is the pixel grid of a display.
 *
 * You can see [some use cases on the GTK blog](https://blogs.gnome.org/gtk/2026/05/28/snapping/).
 *
 * Commonly, you will want to use constants such as [const@Gsk.RECT_SNAP_ROUND],
 * [const@Gsk.RECT_SNAP_GROW], or [const@Gsk.RECT_SNAP_SHRINK], but you can
 * also construct an individual way to snap using [func@Gsk.rect_snap_new].
 *
 * Since: 4.24
 */
typedef unsigned GskRectSnap;

/**
 * gsk_rect_snap_new:
 * @top: How to snap the top edge
 * @right: How to snap the right edge
 * @bottom: How to snap the bottom edge
 * @left: How to snap the left edge
 *
 * Creates a new way to snap rectangles with individual values for the
 * top, right, bottom, and left side.
 *
 * Returns: a description for how to snap rectangles
 *
 * Since: 4.24
 **/
GskRectSnap
(gsk_rect_snap_new) (GskSnapDirection top,
                     GskSnapDirection right,
                     GskSnapDirection bottom,
                     GskSnapDirection left)
{
  return gsk_rect_snap_new (top, right, bottom, left);
}

/**
 * gsk_rect_snap_get_direction:
 * @snap: a rectangle snap
 * @side: the border to query
 *
 * Queries the way a given border is snapped.
 *
 * Returns: the direction the given border is snapped
 *
 * Since: 4.24
 **/
GskSnapDirection
(gsk_rect_snap_get_direction) (GskRectSnap snap,
                               GskSide     side)
{ 
  return gsk_rect_snap_get_direction (snap, side);
}

