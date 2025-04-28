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

/**
 * GskPointSnap:
 *
 * The ways a point can be snapped to a grid.
 *
 * Since: 4.20
 */
typedef unsigned GskPointSnap;

/**
 * gsk_point_snap_new:
 * @h: How to snap the x coordinate
 * @v: How to snap the y coordinate
 *
 * Creates a new way to snap points.
 *
 * Returns: a description for how to snap points
 *
 * Since: 4.20
 **/
GskPointSnap
gsk_point_snap_new (GskSnapDirection h,
                    GskSnapDirection v)
{
  return GSK_POINT_SNAP_INIT (h, v);
}

/**
 * gsk_point_snap_get_direction:
 * @snap: a point snap
 * @border: the coordinate to query
 *
 * Queries the way a given coordinate is snapped.
 *
 * Returns: the direction the given coordinate is snapped
 *
 * Since: 4.20
 **/
GskSnapDirection
gsk_point_snap_get_direction (GskPointSnap snap,
                              unsigned     border)
{
  return (GskSnapDirection) ((snap >> (8 * border)) & 0xFF);
}
