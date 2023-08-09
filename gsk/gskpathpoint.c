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

#include "gdk/gdkprivate.h"

/**
 * GskPathPoint:
 *
 * `GskPathPoint` is an opaque type representing a point on a path.
 *
 * It can be queried for properties of the path at that point, such as its
 * tangent or its curvature.
 *
 * To obtain a `GskPathPoint`, use [method@Gsk.Path.get_closest_point].
 *
 * Note that `GskPathPoint` structs are meant to be stack-allocated, and
 * don't a reference to the path object they are obtained from. It is the
 * callers responsibility to keep a reference to the path as long as the
 * `GskPathPoint` is used.
 *
 * Since: 4.14
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

/**
 * gsk_path_point_equal:
 * @point1: a `GskPathPoint`
 * @point2: another `GskPathPoint`
 *
 * Returns whether the two path points refer to the same
 * location.
 *
 * Return: `TRUE` if @point1 and @point2 are equal
 */
gboolean
gsk_path_point_equal (const GskPathPoint *point1,
                      const GskPathPoint *point2)
{
  const GskRealPathPoint *p1 = (const GskRealPathPoint *) point1;
  const GskRealPathPoint *p2 = (const GskRealPathPoint *) point2;

  if (p1->contour == p2->contour)
    {
      if ((p1->idx     == p2->idx     && p1->t == p2->t) ||
          (p1->idx + 1 == p2->idx     && p1->t == 1 && p2->t == 0) ||
          (p1->idx     == p2->idx + 1 && p1->t == 0 && p2->t == 1))
        return TRUE;
    }

  return FALSE;
}

/**
 * gsk_path_point_compare:
 * @point1: a `GskPathPoint`
 * @point2: another `GskPathPoint`
 *
 * Returns whether @point1 is before or after @point2.
 *
 * Returns: -1 if @point1 is before @point2,
 *   1 if @point1 is after @point2,
 *   0 if they are equal
 */
int
gsk_path_point_compare (const GskPathPoint *point1,
                        const GskPathPoint *point2)
{
  const GskRealPathPoint *p1 = (const GskRealPathPoint *) point1;
  const GskRealPathPoint *p2 = (const GskRealPathPoint *) point2;

  if (gsk_path_point_equal (point1, point2))
    return 0;

  if (p1->contour < p2->contour)
    return -1;
  else if (p1->contour > p2->contour)
    return 1;
  else if (p1->idx < p2->idx)
    return -1;
  else if (p1->idx > p2->idx)
    return 1;
  else if (p1->t < p2->t)
    return -1;
  else if (p1->t > p2->t)
    return 1;

  return 0;
}

/**
 * gsk_path_point_get_position:
 * @path: a `GskPath`
 * @point: a `GskPathPoint` on @path
 * @position: (out caller-allocates): Return location for
 *   the coordinates of the point
 *
 * Gets the position of the point.
 *
 * Since: 4.14
 */
void
gsk_path_point_get_position (GskPath            *path,
                             const GskPathPoint *point,
                             graphene_point_t   *position)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;
  const GskContour *contour;

  g_return_if_fail (path != NULL);
  g_return_if_fail (point != NULL);
  g_return_if_fail (position != NULL);
  g_return_if_fail (self->contour < gsk_path_get_n_contours (path));

  contour = gsk_path_get_contour (path, self->contour),
  gsk_contour_get_position (contour, self, position);
}

/**
 * gsk_path_point_get_tangent:
 * @path: a `GskPath`
 * @point: a `GskPathPoint` on @path
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
gsk_path_point_get_tangent (GskPath            *path,
                            const GskPathPoint *point,
                            GskPathDirection    direction,
                            graphene_vec2_t    *tangent)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;
  const GskContour *contour;

  g_return_if_fail (path != NULL);
  g_return_if_fail (point != NULL);
  g_return_if_fail (tangent != NULL);
  g_return_if_fail (self->contour < gsk_path_get_n_contours (path));

  contour = gsk_path_get_contour (path, self->contour),
  gsk_contour_get_tangent (contour, self, direction, tangent);
}

/**
 * gsk_path_point_get_curvature:
 * @path: a `GskPath`
 * @point: a `GskPathPoint` on @path
 * @center: (out caller-allocates) (nullable): Return location for
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
gsk_path_point_get_curvature (GskPath            *path,
                              const GskPathPoint *point,
                              graphene_point_t   *center)
{
  GskRealPathPoint *self = (GskRealPathPoint *) point;
  const GskContour *contour;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (point != NULL, 0);
  g_return_val_if_fail (self->contour < gsk_path_get_n_contours (path), 0);

  contour = gsk_path_get_contour (path, self->contour);
  return gsk_contour_get_curvature (contour, self, center);
}
