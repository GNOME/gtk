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
 * location on all paths.
 *
 * Note that the start- and endpoint of a closed contour
 * will compare nonequal according to this definition.
 * Use [method@Gsk.Path.is_closed] to find out if the
 * start- and endpoint of a concrete path refer to the
 * same location.
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
