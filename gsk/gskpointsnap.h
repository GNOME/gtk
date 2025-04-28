/* GSK - The GTK Scene Kit
 * Copyright © 2025 Red Hat, Inc.
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
 * GSK_POINT_SNAP_INIT:
 * @h: How to snap in the x direction
 * @v: How to snap in the y direction
 *
 * A macro that initializes a point snap value.
 *
 * Equivalent to calling [function@Gsk.point_snap_new].
 *
 * Returns: a description for how to snap points
 *
 * Since: 4.20
 */
#define GSK_POINT_SNAP_INIT(h, v) (((h) << 8) | (v))

/**
 * GSK_POINT_SNAP_NONE:
 *
 * Makes the point not snap at all.
 *
 * This is the default value for snapping.
 *
 * Since: 4.20
 */
#define GSK_POINT_SNAP_NONE GSK_POINT_SNAP_INIT (GSK_SNAP_NONE, GSK_SNAP_NONE)

GDK_AVAILABLE_IN_4_20
GskPointSnap            gsk_point_snap_new                      (GskSnapDirection        h,
                                                                 GskSnapDirection        v);

GDK_AVAILABLE_IN_4_20
GskSnapDirection        gsk_point_snap_get_direction            (GskRectSnap             snap,
                                                                 unsigned                direction);

