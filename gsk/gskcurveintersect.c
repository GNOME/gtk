/*
 * Copyright © 2020 Red Hat, Inc
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

#include "gskcurveprivate.h"

static inline gboolean
acceptable (float t)
{
  return 0 - FLT_EPSILON <= t && t <= 1 + FLT_EPSILON;
}


static int
line_intersect (const GskCurve   *curve1,
                const GskCurve   *curve2,
                float            *t1,
                float            *t2,
                graphene_point_t *p,
                int               n)
{
  const graphene_point_t *pts1 = curve1->line.points;
  const graphene_point_t *pts2 = curve2->line.points;
  float a1 = pts1[0].x - pts1[1].x;
  float b1 = pts1[0].y - pts1[1].y;
  float a2 = pts2[0].x - pts2[1].x;
  float b2 = pts2[0].y - pts2[1].y;
  float det = a1 * b2 - b1 * a2;

  if (fabs(det) > 0.01)
    {
      float tt =   ((pts1[0].x - pts2[0].x) * b2 - (pts1[0].y - pts2[0].y) * a2) / det;
      float ss = - ((pts1[0].y - pts2[0].y) * a1 - (pts1[0].x - pts2[0].x) * b1) / det;

      if (acceptable (tt) && acceptable (ss))
        {
          p[0].x = pts1[0].x + tt * (pts1[1].x - pts1[0].x);
          p[0].y = pts1[0].y + tt * (pts1[1].y - pts1[0].y);

          t1[0] = tt;
          t2[0] = ss;

          return 1;
        }
    }
  else /* parallel lines */
    {
      float r = a1 * (pts1[1].y - pts2[0].y) - (pts1[1].x - pts2[0].x) * b1;
      float dist = (r * r) / (a1 * a1 + b1 * b1);
      float t, s, tt, ss;

      if (dist > 0.01)
        return 0;

      if (pts1[1].x != pts1[0].x)
        {
          t = (pts2[0].x - pts1[0].x) / (pts1[1].x - pts1[0].x);
          s = (pts2[1].x - pts1[0].x) / (pts1[1].x - pts1[0].x);
        }
      else
        {
          t = (pts2[0].y - pts1[0].y) / (pts1[1].y - pts1[0].y);
          s = (pts2[1].y - pts1[0].y) / (pts1[1].y - pts1[0].y);
        }

      if ((t < 0 && s < 0) || (t > 1 && s > 1))
        return 0;

      if (acceptable (t))
        {
          t1[0] = t;
          t2[0] = 0;
          p[0] = pts2[0];
        }
      else if (t < 0)
        {
          if (pts2[1].x != pts2[0].x)
            tt = (pts1[0].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            tt = (pts1[0].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[0] = 0;
          t2[0] = tt;
          p[0] = pts1[0];
        }
      else
        {
          if (pts2[1].x != pts2[0].x)
            tt = (pts1[1].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            tt = (pts1[1].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[0] = 1;
          t2[0] = tt;
          p[0] = pts1[1];
        }

      if (acceptable (s))
        {
          if (t2[0] == 1)
            return 1;

          t1[1] = s;
          t2[1] = 1;
          p[1] = pts2[1];
        }
      else if (s < 0)
        {
          if (t1[0] == 0)
            return 1;

          if (pts2[1].x != pts2[0].x)
            ss = (pts1[0].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            ss = (pts1[0].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[1] = 0;
          t2[1] = ss;
          p[1] = pts1[0];
        }
      else
        {
          if (t1[0] == 1)
            return 1;

          if (pts2[1].x != pts2[0].x)
            ss = (pts1[1].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            ss = (pts1[1].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[1] = 1;
          t2[1] = ss;
          p[1] = pts1[1];
        }

      return 2;
    }

  return 0;
}

static void
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static void
align_points (const graphene_point_t *p,
              const graphene_point_t *a,
              const graphene_point_t *b,
              graphene_point_t       *q,
              int                     n)
{
  graphene_vec2_t n1;
  float angle;
  float s, c;
  float dist;

  get_tangent (a, b, &n1);
  angle = - atan2 (graphene_vec2_get_y (&n1), graphene_vec2_get_x (&n1));
  sincosf (angle, &s, &c);

  dist = sqrtf ((a->x - b->x)*(a->x - b->x) + (a->y - b->y)*(a->y - b->y));

  for (int i = 0; i < n; i++)
    {
      q[i].x = ((p[i].x - a->x) * c - (p[i].y - a->y) * s) / dist;
      q[i].y = ((p[i].x - a->x) * s + (p[i].y - a->y) * c) / dist;
    }
}

static void
find_point_on_line (const graphene_point_t *p1,
                    const graphene_point_t *p2,
                    const graphene_point_t *q,
                    float                  *t)
{
  if (p2->x != p1->x)
    *t = (q->x - p1->x) / (p2->x - p1->x);
  else
    *t = (q->y - p1->y) / (p2->y - p1->y);
}

static float
cuberoot (float v)
{
  if (v < 0)
    return -pow (-v, 1.f / 3);
  return pow (v, 1.f / 3);
}

/* Solve P = 0 where P is
 * P = (1-t)^3*pa + 3*t*(1-t)^2*pb + 3*t^2*(1-t)*pc + t^3*pd
 */
static int
get_cubic_roots (float pa, float pb, float pc, float pd, float roots[3])
{
  float a, b, c, d;
  float q, q2;
  float p, p3;
  float discriminant;
  float u1, v1, sd;
  int n_roots = 0;

  d = -pa + 3*pb - 3*pc + pd;
  a = 3*pa - 6*pb + 3*pc;
  b = -3*pa + 3*pb;
  c = pa;

  if (fabs (d) < 0.0001)
    {
      if (fabs (a) < 0.0001)
        {
          if (fabs (b) < 0.0001)
            return 0;

          if (acceptable (-c / b))
            {
              roots[0] = -c / b;

              return 1;
            }

          return 0;
        }
      q = sqrt (b*b - 4*a*c);
      roots[n_roots] = (-b + q) / (2 * a);
      if (acceptable (roots[n_roots]))
        n_roots++;

      roots[n_roots] = (-b - q) / (2 * a);
      if (acceptable (roots[n_roots]))
        n_roots++;

      return n_roots;
    }

  a /= d;
  b /= d;
  c /= d;

  p = (3*b - a*a)/3;
  p3 = p/3;
  q = (2*a*a*a - 9*a*b + 27*c)/27;
  q2 = q/2;
  discriminant = q2*q2 + p3*p3*p3;

  if (discriminant < 0)
    {
      float mp3 = -p/3;
      float mp33 = mp3*mp3*mp3;
      float r = sqrt (mp33);
      float t = -q / (2*r);
      float cosphi = t < -1 ? -1 : (t > 1 ? 1 : t);
      float phi = acos (cosphi);
      float crtr = cuberoot (r);
      float t1 = 2*crtr;

      roots[n_roots] = t1 * cos (phi/3) - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = t1 * cos ((phi + 2*M_PI) / 3) - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = t1 * cos ((phi + 4*M_PI) / 3) - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;

    return n_roots;
  }

  if (discriminant == 0)
    {
      u1 = q2 < 0 ? cuberoot (-q2) : -cuberoot (q2);
      roots[n_roots] = 2*u1 - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = -u1 - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;

      return n_roots;
    }

  sd = sqrt (discriminant);
  u1 = cuberoot (sd - q2);
  v1 = cuberoot (sd + q2);
  roots[n_roots] = u1 - v1 - a/3;
  if (acceptable (roots[n_roots]))
    n_roots++;

  return n_roots;
}

static int
line_curve_intersect (const GskCurve   *curve1,
                      const GskCurve   *curve2,
                      float            *t1,
                      float            *t2,
                      graphene_point_t *p,
                      int               n)
{
  const graphene_point_t *a = &curve1->line.points[0];
  const graphene_point_t *b = &curve1->line.points[1];
  graphene_point_t pts[4];
  float t[3];
  int m, i, j;

  /* Rotate things to place curve1 on the x axis,
   * then solve curve2 for y == 0.
   */
  align_points (curve2->curve.points, a, b, pts, 4);

  m = get_cubic_roots (pts[0].y, pts[1].y, pts[2].y, pts[3].y, t);

  j = 0;
  for (i = 0; i < m; i++)
    {
      t2[j] = t[i];
      gsk_curve_get_point (curve2, t2[j], &p[j]);
      find_point_on_line (a, b, &p[j], &t1[j]);
      if (acceptable (t1[j]))
        j++;
      if (j == n)
        break;
    }

  return j;
}

static void
curve_intersect_recurse (const GskCurve   *curve1,
                         const GskCurve   *curve2,
                         float             t1l,
                         float             t1r,
                         float             t2l,
                         float             t2r,
                         float            *t1,
                         float            *t2,
                         graphene_point_t *p,
                         int               n,
                         int              *pos,
                         float             tolerance)
{
  GskCurve p11, p12, p21, p22;
  GskBoundingBox b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  gsk_curve_get_tight_bounds (curve1, &b1);
  gsk_curve_get_tight_bounds (curve2, &b2);

  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.max.x - b1.min.x < tolerance && b1.max.y - b1.min.y < tolerance &&
      b2.max.x - b2.min.x < tolerance && b2.max.y - b2.min.y < tolerance)
    {
      graphene_point_t c;
      t1[*pos] = t1l + d1;
      t2[*pos] = t2l + d2;
      gsk_curve_get_point (curve1, 0.5, &c);

      for (int i = 0; i < *pos; i++)
        {
          if (graphene_point_near (&c, &p[i], 0.1))
            return;
        }

      p[*pos] = c;
      (*pos)++;

      return;
    }

  gsk_curve_split (curve1, 0.5, &p11, &p12);
  gsk_curve_split (curve2, 0.5, &p21, &p22);

  curve_intersect_recurse (&p11, &p21, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos, tolerance);
  curve_intersect_recurse (&p11, &p22, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos, tolerance);
  curve_intersect_recurse (&p12, &p21, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos, tolerance);
  curve_intersect_recurse (&p12, &p22, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos, tolerance);
}

static int
curve_intersect (const GskCurve   *curve1,
                 const GskCurve   *curve2,
                 float            *t1,
                 float            *t2,
                 graphene_point_t *p,
                 int               n,
                 float             tolerance)
{
  int pos = 0;

  curve_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos, tolerance);

  return pos;
}

static void
get_bounds (const GskCurve  *curve,
            float            tl,
            float            tr,
            GskBoundingBox  *bounds)
{
  GskCurve c;

  gsk_curve_segment (curve, tl, tr, &c);
  gsk_curve_get_tight_bounds (&c, bounds);
}

static void
general_intersect_recurse (const GskCurve   *curve1,
                           const GskCurve   *curve2,
                           float             t1l,
                           float             t1r,
                           float             t2l,
                           float             t2r,
                           float            *t1,
                           float            *t2,
                           graphene_point_t *p,
                           int               n,
                           int              *pos,
                           float             tolerance)
{
  GskBoundingBox b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  get_bounds (curve1, t1l, t1r, &b1);
  get_bounds (curve2, t2l, t2r, &b2);

  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.max.x - b1.min.x < tolerance && b1.max.y - b1.min.y < tolerance &&
      b2.max.x - b2.min.x < tolerance && b2.max.y - b2.min.y < tolerance)
    {
      graphene_point_t c;
      t1[*pos] = t1l + d1;
      t2[*pos] = t2l + d2;
      gsk_curve_get_point (curve1, t1[*pos], &c);

      for (int i = 0; i < *pos; i++)
        {
          if (graphene_point_near (&c, &p[i], tolerance))
            return;
        }

      p[*pos] = c;
      (*pos)++;

      return;
    }

  /* Note that in the conic case, we cannot just split the curves and
   * pass the two halves down, since splitting changes the parametrization,
   * and we need the t's to be valid parameters wrt to the original curve.
   *
   * So, instead, we determine the bounding boxes above by always starting
   * from the original curve. That is a bit less efficient, but also works
   * for conics.
   */
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos, tolerance);
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos, tolerance);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos, tolerance);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos, tolerance);
}

static int
general_intersect (const GskCurve   *curve1,
                   const GskCurve   *curve2,
                   float            *t1,
                   float            *t2,
                   graphene_point_t *p,
                   int               n,
                   float             tolerance)
{
  int pos = 0;

  general_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos, tolerance);

  return pos;
}

static int
curve_self_intersect (const GskCurve   *curve,
                      float            *t1,
                      float            *t2,
                      graphene_point_t *p,
                      int               n)
{
  float tt[3], ss[3], s;
  graphene_point_t pp[3];
  int m;
  GskCurve cs, ce;

  if (curve->op != GSK_PATH_CURVE)
    return 0;

  s = 0.5;
  m = gsk_curve_get_curvature_points (curve, tt);
  for (int i = 0; i < m; i++)
    {
      if (gsk_curve_get_curvature (curve, tt[i], NULL) == 0)
        {
          s = tt[i];
          break;
        }
    }

  gsk_curve_split (curve, s, &cs, &ce);

  m = curve_intersect (&cs, &ce, tt, ss, pp, 3, 0.001);

  if (m > 1)
    {
      /* One of the (at most 2) intersections we found
       * must be the common point where we split the curve.
       * It will have a t value of 1 and an s value of 0.
       */
      if (fabs (tt[0] - 1) > 1e-3)
        {
          t1[0] = t2[0] = tt[0] * s;
          p[0] = pp[0];
        }
      else if (fabs (tt[1] - 1) > 1e-3)
        {
          t1[0] = t2[0] = tt[1] * s;
          p[0] = pp[1];
        }
      if (fabs (ss[0]) > 1e-3)
        {
          t1[1] = t2[1] = s + ss[0] * (1 - s);
          p[1] = pp[0];
        }
      else if (fabs (ss[1]) > 1e-3)
        {
          t1[1] = t2[1] = s + ss[1] * (1 - s);
          p[1] = pp[1];
        }

      return 2;
    }

  return 0;
}

/* Place intersections between the curves in p, and their Bezier positions
 * in t1 and t2, up to n. Return the number of intersections found.
 *
 * Note that two cubic Beziers can have up to 9 intersections.
 */
int
gsk_curve_intersect (const GskCurve   *curve1,
                     const GskCurve   *curve2,
                     float            *t1,
                     float            *t2,
                     graphene_point_t *p,
                     int               n)
{
  GskPathOperation op1 = curve1->op;
  GskPathOperation op2 = curve2->op;

  if (op1 == GSK_PATH_CLOSE)
    op1 = GSK_PATH_LINE;

  if (op2 == GSK_PATH_CLOSE)
    op2 = GSK_PATH_LINE;

  if (memcmp (curve1, curve2, sizeof (GskCurve)) == 0)
    return curve_self_intersect (curve1, t1, t2, p, n);

  /* We special-case line-line and line-curve intersections,
   * since we can solve them directly.
   * Everything else is done via bisection.
   */
  if (op1 == GSK_PATH_LINE && op2 == GSK_PATH_LINE)
    return line_intersect (curve1, curve2, t1, t2, p, n);
  else if (op1 == GSK_PATH_LINE && op2 == GSK_PATH_CURVE)
    return line_curve_intersect (curve1, curve2, t1, t2, p, n);
  else if (op1 == GSK_PATH_CURVE && op2 == GSK_PATH_LINE)
    return line_curve_intersect (curve2, curve1, t2, t1, p, n);
  else if (op1 == GSK_PATH_CURVE && op2 == GSK_PATH_CURVE)
    return curve_intersect (curve1, curve2, t1, t2, p, n, 0.001);
  else
    return general_intersect (curve1, curve2, t1, t2, p, n, 0.001);

}
