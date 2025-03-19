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

#pragma once

#if !defined (__GSK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gsk/gsk.h> can be included directly."
#endif

#include <gsk/gsktypes.h>

/**
 * GSK_RECT_SNAP_INIT:
 * @top: How to snap the top edge
 * @right: How to snap the right edge
 * @bottom: How to snap the bottom edge
 * @left: How to snap the left edge
 *
 * A macro that initializes a gsk_rect_snap() value. equivalent to calling
 * gsk_rect_snap_new().
 *
 * Returns: a description for how to snap rectangles
 *
 * Since: 4.20
 */
#define GSK_RECT_SNAP_INIT(top, right, bottom, left) (((((((left) << 8) | (bottom)) << 8) | (right)) << 8) | (top)) 

/**
 * GSK_RECT_SNAP_NONE:
 *
 * Makes the rectangle not snap at all.
 *
 * This is the default value for snapping.
 *
 * Since: 4.20
 */
#define GSK_RECT_SNAP_NONE GSK_RECT_SNAP_INIT (GSK_SNAP_NONE, GSK_SNAP_NONE, GSK_SNAP_NONE, GSK_SNAP_NONE)

/**
 * GSK_RECT_SNAP_GROW:
 *
 * Makes the rectangle grow in every direction.
 *
 * This is useful to avoid seams but can lead to overlap with adjacent content.
 *
 * Since: 4.20
 */
#define GSK_RECT_SNAP_GROW GSK_RECT_SNAP_INIT (GSK_SNAP_FLOOR, GSK_SNAP_CEIL, GSK_SNAP_CEIL, GSK_SNAP_FLOOR)

/**
 * GSK_RECT_SNAP_SHRINK:
 *
 * Makes the rectangle shrink in every direction.
 *
 * This is useful to make sure the rectangle fits into the allocated area and does not overlap content
 * that is not snapped.
 *
 * Since: 4.20
 */
#define GSK_RECT_SNAP_SHRINK GSK_RECT_SNAP_INIT (GSK_SNAP_CEIL, GSK_SNAP_FLOOR, GSK_SNAP_FLOOR, GSK_SNAP_CEIL)

/**
 * GSK_RECT_SNAP_ROUND:
 *
 * Makes the rectangle round to the closest pixel edge on all sides.
 *
 * This is useful when multiple rectangles are placed next to each other at the same coordinate, and they should
 * do so without any seams.
 *
 * Since: 4.20
 */
#define GSK_RECT_SNAP_ROUND GSK_RECT_SNAP_INIT (GSK_SNAP_ROUND, GSK_SNAP_ROUND, GSK_SNAP_ROUND, GSK_SNAP_ROUND)

GDK_AVAILABLE_IN_4_20
GskRectSnap             gsk_rect_snap_new                       (GskSnapDirection        top,
                                                                 GskSnapDirection        right,
                                                                 GskSnapDirection        bottom,
                                                                 GskSnapDirection        left);

GDK_AVAILABLE_IN_4_20
GskSnapDirection        gsk_rect_snap_get_direction             (GskRectSnap             snap,
                                                                 unsigned                dir);

