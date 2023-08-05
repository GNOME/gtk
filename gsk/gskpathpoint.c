/*
 * Copyright © 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gskpathpointprivate.h"

#include "gskcontourprivate.h"
#include "gskpathmeasure.h"

#include "gdk/gdkprivate.h"


/**
 * GskPathPoint:
 *
 * `GskPathPoint` is an opaque type representing a point on a path.
 *
 * It can be queried for properties of the path at that point, such as its
 * tangent or its curvature.
 *
 * To obtain a `GskPathPoint`, use [method@Gsk.Path.get_closest_point]
 * or [method@Gsk.PathMeasure.get_point].
 *
 * Note that `GskPathPoint` structs are meant to be stack-allocated, and
 * don't a reference to the path object they are obtained from. It is the
 * callers responsibility to keep a reference to the path as long as the
 * `GskPathPoint` is used.
 */

G_DEFINE_BOXED_TYPE (GskPathPoint, gsk_path_point,
                     gsk_path_point_copy,
                     gsk_path_point_free)


GskPathPoint *
gsk_path_point_copy (GskPathPoint *point)
{
  GskPathPoint *copy;

  copy = g_new0 (GskPathPoint, 1);

  memcpy (copy, point, sizeof (GskRealPathPoint));

  return copy;
}

void
gsk_path_point_free (GskPathPoint *point)
{
  g_free (point);
}

GskPath *
gsk_path_point_get_path (GskPathPoint *point)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  return self->path;
}

const GskContour *
gsk_path_point_get_contour (GskPathPoint *point)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  return self->contour;
}

/**
 * gsk_path_point_get_position:
 * @point: a `GskPathPoint`
 * @position: (out caller-allocates): Return location for
 *   the coordinates of the point
 *
 * Gets the position of the point.
 *
 * Since: 4.14
 */
void
gsk_path_point_get_position (GskPathPoint     *point,
                             graphene_point_t *position)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  gsk_contour_get_position (self->contour, self, position);
}

/**
 * gsk_path_point_get_tangent:
 * @point: a `GskPathPoint`
 * @direction: the direction for which to return the tangent
 * @tangent: (out caller-allocates): Return location for
 *   the tangent at the point
 *
 * Gets the tangent of the path at the point.
 *
 * Note that certain points on a path may not have a single
 * tangent, such as sharp turns. At such points, there are
 * two tangents -- the direction of the path going into the
 * point, and the direction coming out of it. The @direction
 * argument lets you choose which one to get.
 *
 * Since: 4.14
 */
void
gsk_path_point_get_tangent (GskPathPoint     *point,
                            GskPathDirection  direction,
                            graphene_vec2_t  *tangent)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  gsk_contour_get_tangent (self->contour, self, direction, tangent);
}

/**
 * gsk_path_point_get_curvature:
 * @point: a `GskPathPoint`
 * @center: (out caller-allocates): Return location for
 *   the center of the osculating circle
 *
 * Calculates the curvature of the path at the point.
 *
 * Optionally, returns the center of the osculating circle as well.
 *
 * If the curvature is infinite (at line segments), zero is returned,
 * and @center is not modified.
 *
 * Returns: The curvature of the path at the given point
 *
 * Since: 4.14
 */
float
gsk_path_point_get_curvature (GskPathPoint     *point,
                              graphene_point_t *center)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  return gsk_contour_get_curvature (self->contour, self, center);
}

float
gsk_path_point_get_distance (GskPathPoint *point,
                             gpointer      measure_data)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;

  return gsk_contour_get_distance (self->contour, self, measure_data);
}
