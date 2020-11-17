/*
 * Copyright © 2002 University of Southern California
 *             2020 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Carl D. Worth <cworth@cworth.org>
 */

#include "config.h"

#include "gsksplineprivate.h"

typedef struct
{
  graphene_point_t last_point;
  float last_progress;
  float tolerance_squared;
  GskSplineAddPointFunc func;
  gpointer user_data;
} GskCubicDecomposition;

static void
gsk_spline_decompose_add_point (GskCubicDecomposition  *decomp,
                                const graphene_point_t *pt,
                                float                   progress)
{
  if (graphene_point_equal (&decomp->last_point, pt))
    return;

  decomp->func (&decomp->last_point, pt, decomp->last_progress, decomp->last_progress + progress, decomp->user_data);
  decomp->last_point = *pt;
  decomp->last_progress += progress;
}

void
gsk_spline_split_cubic (const graphene_point_t pts[4],
                        graphene_point_t       result1[4],
                        graphene_point_t       result2[4],
                        float                  progress)
{
    graphene_point_t ab, bc, cd;
    graphene_point_t abbc, bccd;
    graphene_point_t final;

    graphene_point_interpolate (&pts[0], &pts[1], progress, &ab);
    graphene_point_interpolate (&pts[1], &pts[2], progress, &bc);
    graphene_point_interpolate (&pts[2], &pts[3], progress, &cd);
    graphene_point_interpolate (&ab, &bc, progress, &abbc);
    graphene_point_interpolate (&bc, &cd, progress, &bccd);
    graphene_point_interpolate (&abbc, &bccd, progress, &final);

    memcpy (result1, (graphene_point_t[4]) { pts[0], ab, abbc, final }, sizeof (graphene_point_t[4]));
    memcpy (result2, (graphene_point_t[4]) { final, bccd, cd, pts[3] }, sizeof (graphene_point_t[4]));
}

/* Return an upper bound on the error (squared) that could result from
 * approximating a spline as a line segment connecting the two endpoints. */
static float
gsk_spline_error_squared (const graphene_point_t pts[4])
{
  float bdx, bdy, berr;
  float cdx, cdy, cerr;

  /* We are going to compute the distance (squared) between each of the the b
   * and c control points and the segment a-b. The maximum of these two
   * distances will be our approximation error. */

  bdx = pts[1].x - pts[0].x;
  bdy = pts[1].y - pts[0].y;

  cdx = pts[2].x - pts[0].x;
  cdy = pts[2].y - pts[0].y;

  if (!graphene_point_equal (&pts[0], &pts[3]))
    {
      float dx, dy, u, v;

      /* Intersection point (px):
       *     px = p1 + u(p2 - p1)
       *     (p - px) ∙ (p2 - p1) = 0
       * Thus:
       *     u = ((p - p1) ∙ (p2 - p1)) / ∥p2 - p1∥²;
       */

      dx = pts[3].x - pts[0].x;
      dy = pts[3].y - pts[0].y;
      v = dx * dx + dy * dy;

      u = bdx * dx + bdy * dy;
      if (u <= 0)
        {
          /* bdx -= 0;
           * bdy -= 0;
           */
        }
      else if (u >= v)
        {
          bdx -= dx;
          bdy -= dy;
        }
      else
        {
          bdx -= u/v * dx;
          bdy -= u/v * dy;
        }

      u = cdx * dx + cdy * dy;
      if (u <= 0)
        {
          /* cdx -= 0;
           * cdy -= 0;
           */
        }
      else if (u >= v)
        {
          cdx -= dx;
          cdy -= dy;
        }
      else
        {
          cdx -= u/v * dx;
          cdy -= u/v * dy;
        }
    }

    berr = bdx * bdx + bdy * bdy;
    cerr = cdx * cdx + cdy * cdy;
    if (berr > cerr)
      return berr;
    else
      return cerr;
}

static void
gsk_spline_decompose_into (GskCubicDecomposition  *decomp,
                           const graphene_point_t  pts[4],
                           float                   progress)
{
  graphene_point_t left[4], right[4];

  if (gsk_spline_error_squared (pts) < decomp->tolerance_squared)
    {
      gsk_spline_decompose_add_point (decomp, &pts[3], progress);
      return;
    }

  gsk_spline_split_cubic (pts, left, right, 0.5);

  gsk_spline_decompose_into (decomp, left, progress / 2);
  gsk_spline_decompose_into (decomp, right, progress / 2);
}

void 
gsk_spline_decompose_cubic (const graphene_point_t pts[4],
                            float                  tolerance,
                            GskSplineAddPointFunc  add_point_func,
                            gpointer               user_data)
{
  GskCubicDecomposition decomp = { pts[0], 0.0f, tolerance * tolerance, add_point_func, user_data };

  gsk_spline_decompose_into (&decomp, pts, 1.0f);

  g_assert (graphene_point_equal (&decomp.last_point, &pts[3]));
  g_assert (decomp.last_progress == 1.0f || decomp.last_progress == 0.0f);
}

