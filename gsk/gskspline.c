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

#include <math.h>

/* Spline deviation from the circle in radius would be given by:

        error = sqrt (x**2 + y**2) - 1

   A simpler error function to work with is:

        e = x**2 + y**2 - 1

   From "Good approximation of circles by curvature-continuous Bezier
   curves", Tor Dokken and Morten Daehlen, Computer Aided Geometric
   Design 8 (1990) 22-41, we learn:

        abs (max(e)) = 4/27 * sin**6(angle/4) / cos**2(angle/4)

   and
        abs (error) =~ 1/2 * e

   Of course, this error value applies only for the particular spline
   approximation that is used in _cairo_gstate_arc_segment.
*/
static float
arc_error_normalized (float angle)
{
  return 2.0/27.0 * pow (sin (angle / 4), 6) / pow (cos (angle / 4), 2);
}

static float
arc_max_angle_for_tolerance_normalized (float tolerance)
{
  float angle, error;
  guint i;

  /* Use table lookup to reduce search time in most cases. */
  struct {
    float angle;
    float error;
  } table[] = {
    { G_PI / 1.0,   0.0185185185185185036127 },
    { G_PI / 2.0,   0.000272567143730179811158 },
    { G_PI / 3.0,   2.38647043651461047433e-05 },
    { G_PI / 4.0,   4.2455377443222443279e-06 },
    { G_PI / 5.0,   1.11281001494389081528e-06 },
    { G_PI / 6.0,   3.72662000942734705475e-07 },
    { G_PI / 7.0,   1.47783685574284411325e-07 },
    { G_PI / 8.0,   6.63240432022601149057e-08 },
    { G_PI / 9.0,   3.2715520137536980553e-08 },
    { G_PI / 10.0,  1.73863223499021216974e-08 },
    { G_PI / 11.0,  9.81410988043554039085e-09 },
  };

  for (i = 0; i < G_N_ELEMENTS (table); i++)
    {
      if (table[i].error < tolerance)
        return table[i].angle;
    }

  i++;
  do {
    angle = G_PI / i++;
    error = arc_error_normalized (angle);
  } while (error > tolerance);

  return angle;
}

static guint
arc_segments_needed (float angle,
                     float radius,
                     float tolerance)
{
  float max_angle;

  /* the error is amplified by at most the length of the
   * major axis of the circle; see cairo-pen.c for a more detailed analysis
   * of this. */
  max_angle = arc_max_angle_for_tolerance_normalized (tolerance / radius);

  return ceil (fabs (angle) / max_angle);
}

/* We want to draw a single spline approximating a circular arc radius
   R from angle A to angle B. Since we want a symmetric spline that
   matches the endpoints of the arc in position and slope, we know
   that the spline control points must be:

        (R * cos(A), R * sin(A))
        (R * cos(A) - h * sin(A), R * sin(A) + h * cos (A))
        (R * cos(B) + h * sin(B), R * sin(B) - h * cos (B))
        (R * cos(B), R * sin(B))

   for some value of h.

   "Approximation of circular arcs by cubic polynomials", Michael
   Goldapp, Computer Aided Geometric Design 8 (1991) 227-238, provides
   various values of h along with error analysis for each.

   From that paper, a very practical value of h is:

        h = 4/3 * R * tan(angle/4)

   This value does not give the spline with minimal error, but it does
   provide a very good approximation, (6th-order convergence), and the
   error expression is quite simple, (see the comment for
   _arc_error_normalized).
*/
static gboolean
gsk_spline_decompose_arc_segment (const graphene_point_t *center,
                                  float                   radius,
                                  float                   angle_A,
                                  float                   angle_B,
                                  GskSplineAddCurveFunc   curve_func,
                                  gpointer                user_data)
{
  float r_sin_A, r_cos_A;
  float r_sin_B, r_cos_B;
  float h;

  r_sin_A = radius * sin (angle_A);
  r_cos_A = radius * cos (angle_A);
  r_sin_B = radius * sin (angle_B);
  r_cos_B = radius * cos (angle_B);

  h = 4.0/3.0 * tan ((angle_B - angle_A) / 4.0);

  return curve_func ((graphene_point_t[4]) {
                       GRAPHENE_POINT_INIT (
                         center->x + r_cos_A,
                         center->y + r_sin_A
                       ),
                       GRAPHENE_POINT_INIT (
                         center->x + r_cos_A - h * r_sin_A,
                         center->y + r_sin_A + h * r_cos_A
                       ),
                       GRAPHENE_POINT_INIT (
                         center->x + r_cos_B + h * r_sin_B,
                         center->y + r_sin_B - h * r_cos_B
                       ),
                       GRAPHENE_POINT_INIT (
                         center->x + r_cos_B,
                         center->y + r_sin_B
                       )
                     },
                     user_data);
}

gboolean
gsk_spline_decompose_arc (const graphene_point_t *center,
                          float                   radius,
                          float                   tolerance,
                          float                   start_angle,
                          float                   end_angle,
                          GskSplineAddCurveFunc   curve_func,
                          gpointer                user_data)
{
  float step = start_angle - end_angle;
  guint i, n_segments;

  /* Recurse if drawing arc larger than pi */
  if (ABS (step) > G_PI)
    {
      float mid_angle = (start_angle + end_angle) / 2.0;

      return gsk_spline_decompose_arc (center, radius, tolerance, start_angle, mid_angle, curve_func, user_data)
          && gsk_spline_decompose_arc (center, radius, tolerance, mid_angle, end_angle, curve_func, user_data);
    }
  else if (ABS (step) < tolerance)
    {
      return gsk_spline_decompose_arc_segment (center, radius, start_angle, end_angle, curve_func, user_data);
    }

  n_segments = arc_segments_needed (ABS (step), radius, tolerance);
  step = (end_angle - start_angle) / n_segments;

  for (i = 0; i < n_segments - 1; i++, start_angle += step)
    {
      if (!gsk_spline_decompose_arc_segment (center, radius, start_angle, start_angle + step, curve_func, user_data))
        return FALSE;
    }
  return gsk_spline_decompose_arc_segment (center, radius, start_angle, end_angle, curve_func, user_data);
}

