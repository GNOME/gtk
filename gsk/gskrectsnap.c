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

#include "gskpointsnap.h"
#include "gskrectsnap.h"

/**
 * GskRectSnap:
 *
 * The ways a rectangle can be snapped to a grid.
 *
 * Since: 4.20
 */
typedef unsigned GskRectSnap;

/**
 * gsk_rect_snap_new:
 * @top: How to snap the top edge
 * @right: How to snap the right edge
 * @bottom: How to snap the bottom edge
 * @left: How to snap the left edge
 *
 * Creates a new way to snap rectangles for the 4 given sides.
 *
 * Returns: a description for how to snap rectangles
 *
 * Since: 4.20
 **/
GskRectSnap
gsk_rect_snap_new (GskSnapDirection top,
                   GskSnapDirection right,
                   GskSnapDirection bottom,
                   GskSnapDirection left)
{
  return GSK_RECT_SNAP_INIT (top, right, bottom, left);
}

#define RECT_LEFT   0
#define RECT_BOTTOM 1
#define RECT_RIGHT  2
#define RECT_TOP    3
#define POINT_VERTICAL   0
#define POINT_HORIZONTAL 1

/**
 * gsk_rect_snap_get_direction:
 * @snap: a rectangle snap
 * @border: the border to query
 *
 * Queries the way a given border is snapped.
 *
 * Returns: the direction the given border is snapped
 *
 * Since: 4.20
 **/
GskSnapDirection
gsk_rect_snap_get_direction (GskRectSnap snap,
                             unsigned    border)
{
  return (GskSnapDirection) ((snap >> (8 * border)) & 0xFF);
}

/**
 * gsk_rect_snap_get_origin_snap:
 * @snap: a rectangle snap
 *
 * Queries how the origin of the rectangle is snapped,
 * using the given rectangle snap.
 *
 * Returns: the point snap for the origin
 *
 * Since: 4.20
 */
GskPointSnap
gsk_rect_snap_get_origin_snap (GskRectSnap snap)
{
  return GSK_POINT_SNAP_INIT (gsk_rect_snap_get_direction (snap, RECT_LEFT),
                              gsk_rect_snap_get_direction (snap, RECT_TOP));
}

/**
 * gsk_rect_snap_get_opposite_snap:
 * @snap: a rectangle snap
 *
 * Queries how the opposite point of the rectangle is snapped,
 * using the given rectangle snap.
 *
 * Returns: the point snap for the opposite
 *
 * Since: 4.20
 */
GskPointSnap
gsk_rect_snap_get_opposite_snap (GskRectSnap snap)
{
  return GSK_POINT_SNAP_INIT (gsk_rect_snap_get_direction (snap, RECT_RIGHT),
                              gsk_rect_snap_get_direction (snap, RECT_BOTTOM));
}

/**
 * gsk_rect_snap_new_from_point_snaps:
 * @origin: point snap for the origin
 * @opposite: point snap for the oppposite
 *
 * Creates a rect snap that snaps the origin and opposite
 * points of a rectangle as specified.
 *
 * Returns: the rect snap
 *
 * Since: 4.20
 */
GskRectSnap
gsk_rect_snap_new_from_point_snaps (GskPointSnap origin,
                                    GskPointSnap opposite)
{
  return GSK_RECT_SNAP_INIT (gsk_point_snap_get_direction (origin, POINT_VERTICAL),
                             gsk_point_snap_get_direction (origin, POINT_HORIZONTAL),
                             gsk_point_snap_get_direction (opposite, POINT_VERTICAL),
                             gsk_point_snap_get_direction (opposite, POINT_HORIZONTAL));
}
