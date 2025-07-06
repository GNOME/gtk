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

#include <math.h>

#include "gskcurveprivate.h"

/* {{{ Utilities */

static inline void
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static inline gboolean
acceptable (float t)
{
  return 0 - FLT_EPSILON <= t && t <= 1 + FLT_EPSILON;
}

static inline void
_sincosf (float  angle,
          float *out_s,
          float *out_c)
{
#ifdef HAVE_SINCOSF
      sincosf (angle, out_s, out_c);
#else
      *out_s = sinf (angle);
      *out_c = cosf (angle);
#endif
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

  get_tangent (a, b, &n1);
  angle = - atan2f (graphene_vec2_get_y (&n1), graphene_vec2_get_x (&n1));
  _sincosf (angle, &s, &c);

  for (int i = 0; i < n; i++)
    {
      q[i].x = (p[i].x - a->x) * c - (p[i].y - a->y) * s;
      q[i].y = (p[i].x - a->x) * s + (p[i].y - a->y) * c;
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

static inline gboolean
curve_near (const GskCurve *curve1,
            const GskCurve *curve2,
            float           epsilon)
{
  if (curve1->op != curve2->op)
    return FALSE;

  switch (curve1->op)
    {
    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      return graphene_point_near (&curve1->line.points[0], &curve2->line.points[0], epsilon) &&
             graphene_point_near (&curve1->line.points[1], &curve2->line.points[1], epsilon);

    case GSK_PATH_QUAD:
      return graphene_point_near (&curve1->quad.points[0], &curve2->quad.points[0], epsilon) &&
             graphene_point_near (&curve1->quad.points[1], &curve2->quad.points[1], epsilon) &&
             graphene_point_near (&curve1->quad.points[2], &curve2->quad.points[2], epsilon);

    case GSK_PATH_CUBIC:
      return graphene_point_near (&curve1->cubic.points[0], &curve2->cubic.points[0], epsilon) &&
             graphene_point_near (&curve1->cubic.points[1], &curve2->cubic.points[1], epsilon) &&
             graphene_point_near (&curve1->cubic.points[2], &curve2->cubic.points[2], epsilon) &&
             graphene_point_near (&curve1->cubic.points[3], &curve2->cubic.points[3], epsilon);

    case GSK_PATH_CONIC:
      return graphene_point_near (&curve1->conic.points[0], &curve2->conic.points[0], epsilon) &&
             graphene_point_near (&curve1->conic.points[1], &curve2->conic.points[1], epsilon) &&
             graphene_point_near (&curve1->conic.points[3], &curve2->conic.points[3], epsilon) &&
             fabsf (curve1->conic.points[2].x - curve2->conic.points[2].x) < epsilon;

    case GSK_PATH_MOVE:
    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Math */

/* find solutions for at^2 + bt + c = 0 */
static int
solve_quadratic (float a, float b, float c, float t[2])
{
  float d;
  int n = 0;

  if (fabsf (a) > 0.0001)
    {
      if (b*b > 4*a*c)
        {
          d = sqrtf (b*b - 4*a*c);
          t[n++] = (-b + d)/(2*a);
          t[n++] = (-b - d)/(2*a);
        }
      else
        {
          t[n++] = -b / (2*a);
        }
    }
  else if (fabsf (b) > 0.0001)
    {
      t[n++] = -c / b;
    }

  return n;
}

static int
filter_allowable (float t[3],
                  int   n)
{
  float g[3];
  int j = 0;

  for (int i = 0; i < n; i++)
    if (0 < t[i] && t[i] < 1)
      g[j++] = t[i];
  for (int i = 0; i < j; i++)
    t[i] = g[i];
  return j;
}

/* Solve P = 0 where P is
 * P = (1-t)^2*pa + 2*t*(1-t)*pb + t^2*pc
 */
static int
get_quadratic_roots (float pa, float pb, float pc, float roots[2])
{
  float a, b, c, d;
  int n_roots = 0;

  a = pa - 2 * pb + pc;
  b = 2 * (pb - pa);
  c = pa;

  d = b*b - 4*a*c;

  if (d > 0.0001)
    {
      float q = sqrtf (d);
      roots[n_roots] = (-b + q) / (2 * a);
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = (-b - q) / (2 * a);
      if (acceptable (roots[n_roots]))
        n_roots++;
    }
  else if (fabsf (d) < 0.0001)
    {
      roots[n_roots] = -b / (2 * a);
      if (acceptable (roots[n_roots]))
        n_roots++;
    }

  return n_roots;
}

static float
cuberoot (float v)
{
  if (v < 0)
    return -powf (-v, 1.f / 3);
  return powf (v, 1.f / 3);
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

  if (fabsf (d) < 0.0001)
    {
      if (fabsf (a) < 0.0001)
        {
          if (fabsf (b) < 0.0001)
            return 0;

          if (acceptable (-c / b))
            {
              roots[0] = -c / b;

              return 1;
            }

          return 0;
        }
      q = sqrtf (b*b - 4*a*c);
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
      float r = sqrtf (mp33);
      float t = -q / (2*r);
      float cosphi = CLAMP (t, -1, 1);
      float phi = acosf (cosphi);
      float crtr = cuberoot (r);
      float t1 = 2*crtr;

      roots[n_roots] = t1 * cosf (phi/3) - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = t1 * cosf ((phi + 2*M_PI) / 3) - a/3;
      if (acceptable (roots[n_roots]))
        n_roots++;
      roots[n_roots] = t1 * cosf ((phi + 4*M_PI) / 3) - a/3;
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

  sd = sqrtf (discriminant);
  u1 = cuberoot (sd - q2);
  v1 = cuberoot (sd + q2);
  roots[n_roots] = u1 - v1 - a/3;
  if (acceptable (roots[n_roots]))
    n_roots++;

  return n_roots;
}

/* }}} */
/* {{{ Cusps and inflections */

/* Get the points where the curvature of the curve
 * is zero, or a maximum or minimum, inside the open
 * interval from 0 to 1.
 *
 * Only cubics can have such points.
 */
int
gsk_curve_get_curvature_points (const GskCurve *curve,
                                float           t[3])
{
  const graphene_point_t *pts = curve->cubic.points;
  graphene_point_t p[4];
  float a, b, c, d;
  float x, y, z;
  int n;

  if (curve->op != GSK_PATH_CUBIC)
    return 0;

  align_points (pts, &pts[0], &pts[3], p, 4);

  a = p[2].x * p[1].y;
  b = p[3].x * p[1].y;
  c = p[1].x * p[2].y;
  d = p[3].x * p[2].y;

  x = - 3*a + 2*b + 3*c - d;
  y = 3*a - b - 3*c;
  z = c - a;

  n = solve_quadratic (x, y, z, t);
  return filter_allowable (t, n);
}

/* Find cusps inside the open interval from 0 to 1.
 *
 * Only cubics can have such points.
 *
 * According to Stone & deRose, A Geometric Characterization
 * of Parametric Cubic curves, a necessary and sufficient
 * condition is that the first derivative vanishes.
 */
int
gsk_curve_get_cusps (const GskCurve *curve,
                     float           t[2])
{
  const graphene_point_t *pts = curve->cubic.points;
  graphene_point_t p[3];
  float ax, bx, cx;
  float ay, by, cy;
  float tx[3];
  int nx;
  int n = 0;

  if (curve->op != GSK_PATH_CUBIC)
    return 0;

  p[0].x = 3 * (pts[1].x - pts[0].x);
  p[0].y = 3 * (pts[1].y - pts[0].y);
  p[1].x = 3 * (pts[2].x - pts[1].x);
  p[1].y = 3 * (pts[2].y - pts[1].y);
  p[2].x = 3 * (pts[3].x - pts[2].x);
  p[2].y = 3 * (pts[3].y - pts[2].y);

  ax = p[0].x - 2 * p[1].x + p[2].x;
  bx = - 2 * p[0].x + 2 * p[1].x;
  cx = p[0].x;

  nx = solve_quadratic (ax, bx, cx, tx);
  nx = filter_allowable (tx, nx);

  ay = p[0].y - 2 * p[1].y + p[2].y;
  by = - 2 * p[0].y + 2 * p[1].y;
  cy = p[0].y;

  for (int i = 0; i < nx; i++)
    {
      float ti = tx[i];

      if (0 < ti && ti < 1 &&
          fabs (ay * ti * ti + by * ti + cy) < 0.001)
        t[n++] = ti;
    }

  return n;
}

/* }}} */
/* {{{ Intersection with lines */

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

  if (fabsf (det) > 0.01)
    {
      float tt =   ((pts1[0].x - pts2[0].x) * b2 - (pts1[0].y - pts2[0].y) * a2) / det;
      float ss = - ((pts1[0].y - pts2[0].y) * a1 - (pts1[0].x - pts2[0].x) * b1) / det;

      if (acceptable (tt) && acceptable (ss))
        {
          p[0].x = pts1[0].x + tt * (pts1[1].x - pts1[0].x);
          p[0].y = pts1[0].y + tt * (pts1[1].y - pts1[0].y);

          t1[0] = fabsf (tt);
          t2[0] = fabsf (ss);

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
          t1[0] = fabsf (t);
          t2[0] = 0.f;
          p[0] = pts2[0];
        }
      else if (t < 0)
        {
          if (pts2[1].x != pts2[0].x)
            tt = (pts1[0].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            tt = (pts1[0].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[0] = 0.f;
          t2[0] = fabsf (tt);
          p[0] = pts1[0];
        }
      else
        {
          if (pts2[1].x != pts2[0].x)
            tt = (pts1[1].x - pts2[0].x) / (pts2[1].x - pts2[0].x);
          else
            tt = (pts1[1].y - pts2[0].y) / (pts2[1].y - pts2[0].y);

          t1[0] = 1.f;
          t2[0] = fabsf (tt);
          p[0] = pts1[1];
        }

      if (acceptable (s))
        {
          if (t2[0] == 1)
            return 1;

          t1[1] = fabsf (s);
          t2[1] = 1.f;
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

          t1[1] = 0.f;
          t2[1] = fabsf (ss);
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

          t1[1] = 1.f;
          t2[1] = fabsf (ss);
          p[1] = pts1[1];
        }

      if (t1[0] > t1[1])
        {
          t = t1[0]; t1[0] = t1[1]; t1[1] = t;
          t = t2[0]; t2[0] = t2[1]; t2[1] = t;
          t = p[0].x; p[0].x = p[1].x; p[1].x = t;
          t = p[0].y; p[0].y = p[1].y; p[1].y = t;
        }

      return 2;
    }

  return 0;
}

static int
line_quad_intersect (const GskCurve   *curve1,
                     const GskCurve   *curve2,
                     float            *t1,
                     float            *t2,
                     graphene_point_t *p,
                     int               n)
{
  const graphene_point_t *a = &curve1->line.points[0];
  const graphene_point_t *b = &curve1->line.points[1];
  graphene_point_t pts[4];
  float t[2];
  int m, i, j;

  /* Rotate things to place curve1 on the x axis,
   * then solve curve2 for y == 0.
   */
  align_points (curve2->quad.points, a, b, pts, 3);

  m = get_quadratic_roots (pts[0].y, pts[1].y, pts[2].y, t);

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

static int
line_cubic_intersect (const GskCurve   *curve1,
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
  align_points (curve2->cubic.points, a, b, pts, 4);

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

/* }}} */
/* {{{ Intersection with cubics */

#define MAX_LEVEL 25
#define TOLERANCE 0.001

static void
cubic_intersect_recurse (const GskCurve   *curve1,
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
                         int               level)
{
  GskCurve p11, p12, p21, p22;
  GskBoundingBox b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  if (level == MAX_LEVEL)
    return;

  gsk_curve_get_bounds (curve1, &b1);
  gsk_curve_get_bounds (curve2, &b2);
  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  gsk_curve_get_tight_bounds (curve1, &b1);
  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  gsk_curve_get_tight_bounds (curve2, &b2);
  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.max.x - b1.min.x < TOLERANCE && b1.max.y - b1.min.y < TOLERANCE &&
      b2.max.x - b2.min.x < TOLERANCE && b2.max.y - b2.min.y < TOLERANCE)
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

  cubic_intersect_recurse (&p11, &p21, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos, level + 1);
  cubic_intersect_recurse (&p11, &p22, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos, level + 1);
  cubic_intersect_recurse (&p12, &p21, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos, level + 1);
  cubic_intersect_recurse (&p12, &p22, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos, level + 1);
}

static int
cubic_intersect (const GskCurve   *curve1,
                 const GskCurve   *curve2,
                 float            *t1,
                 float            *t2,
                 graphene_point_t *p,
                 int               n)
{
  int pos = 0;

  cubic_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos, 0);

  return pos;
}

/* }}} */
/* {{{ General intersection */

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
                           int               level)
{
  GskBoundingBox b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  if (level == MAX_LEVEL)
    return;

  get_bounds (curve1, t1l, t1r, &b1);
  get_bounds (curve2, t2l, t2r, &b2);

  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.max.x - b1.min.x < TOLERANCE && b1.max.y - b1.min.y < TOLERANCE &&
      b2.max.x - b2.min.x < TOLERANCE && b2.max.y - b2.min.y < TOLERANCE)
    {
      graphene_point_t c;
      t1[*pos] = t1l + d1;
      t2[*pos] = t2l + d2;
      gsk_curve_get_point (curve1, t1[*pos], &c);

      for (int i = 0; i < *pos; i++)
        {
          if (graphene_point_near (&c, &p[i], 0.1))
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
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos, level + 1);
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos, level + 1);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos, level + 1);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos, level + 1);
}

static int
general_intersect (const GskCurve   *curve1,
                   const GskCurve   *curve2,
                   float            *t1,
                   float            *t2,
                   graphene_point_t *p,
                   int               n)
{
  int pos = 0;

  general_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos, 0);

  return pos;
}

/* }}} */
/* {{{ Helpers */

/* Place intersections between the curves in p, and their
 * Bézier positions in t1 and t2, up to n. Return the number
 * of intersections found.
 *
 * We special-case line intersections, since we can solve
 * them directly. Everything else is done via bisection.
 *
 * Note that two cubic Beziers can have up to 9 intersections.
 */
static int
curve_intersect (const GskCurve   *curve1,
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

  if (op1 == GSK_PATH_LINE && op2 == GSK_PATH_LINE)
    return line_intersect (curve1, curve2, t1, t2, p, n);
  else if (op1 == GSK_PATH_LINE && op2 == GSK_PATH_QUAD)
    return line_quad_intersect (curve1, curve2, t1, t2, p, n);
  else if (op1 == GSK_PATH_QUAD && op2 == GSK_PATH_LINE)
    return line_quad_intersect (curve2, curve1, t2, t1, p, n);
  else if (op1 == GSK_PATH_LINE && op2 == GSK_PATH_CUBIC)
    return line_cubic_intersect (curve1, curve2, t1, t2, p, n);
  else if (op1 == GSK_PATH_CUBIC && op2 == GSK_PATH_LINE)
    return line_cubic_intersect (curve2, curve1, t2, t1, p, n);
  else if ((op1 == GSK_PATH_QUAD || op1 == GSK_PATH_CUBIC) &&
           (op2 == GSK_PATH_QUAD || op2 == GSK_PATH_CUBIC))
    return cubic_intersect (curve1, curve2, t1, t2, p, n);
  else
    return general_intersect (curve1, curve2, t1, t2, p, n);
}

static gboolean
find_coincidence (const GskCurve  *curve1,
                  const GskCurve  *curve2,
                  float            t1[2],
                  float            t2[2],
                  graphene_point_t p[2])
{
  float d, t;
  GskCurve c1, c2;

  if (gsk_curve_get_closest_point (curve1, gsk_curve_get_start_point (curve2), INFINITY, &d, &t) && d < 0.5)
    {
      t1[0] = t;
      t2[0] = 0;
      p[0] = *gsk_curve_get_start_point (curve2);
    }
  else if (gsk_curve_get_closest_point (curve2, gsk_curve_get_start_point (curve1), INFINITY, &d, &t) &&
           d < 0.5)
    {
      t1[0] = 0;
      t2[0] = t;
      p[0] = *gsk_curve_get_start_point (curve1);
    }
  else
    return FALSE;

  if (gsk_curve_get_closest_point (curve1, gsk_curve_get_end_point (curve2), INFINITY, &d, &t) &&
      d < 0.5)
    {
      t1[1] = t;
      t2[1] = 1;
      p[1] = *gsk_curve_get_end_point (curve2);
    }
  else if (gsk_curve_get_closest_point (curve2, gsk_curve_get_end_point (curve1), INFINITY, &d, &t) &&
           d < 0.5)
    {
      t1[1] = 1;
      t2[1] = t;
      p[1] = *gsk_curve_get_end_point (curve1);
    }
  else
    return FALSE;

  if (t1[1] < t1[0])
    {
      float s;
      graphene_point_t q;
      s = t1[0]; t1[0] = t1[1]; t1[1] = s;
      s = t2[0]; t2[0] = t2[1]; t2[1] = s;
      q = p[0]; p[0] = p[1]; p[1] = q;
    }

  gsk_curve_segment (curve1, t1[0], t1[1], &c1);

  if (t2[0] < t2[1])
    gsk_curve_segment (curve2, t2[0], t2[1], &c2);
  else
    {
      GskCurve c;
      gsk_curve_segment (curve2, t2[1], t2[0], &c);
      gsk_curve_reverse (&c, &c2);
    }

  return curve_near (&c1, &c2, 0.1);
}

/* }}} */
/* {{{ API */

int
gsk_curve_intersect (const GskCurve      *curve1,
                     const GskCurve      *curve2,
                     float               *t1,
                     float               *t2,
                     graphene_point_t    *p,
                     GskPathIntersection *kind,
                     int                  n)
{
  float tt1[10], tt2[10];
  graphene_point_t pp[10];
  int nn;
  GskBoundingBox b1, b2;
  int degree[] = { 1, 1, 1, 2, 3, 2 };

  gsk_curve_get_bounds (curve1, &b1);
  gsk_curve_get_bounds (curve2, &b2);

  if (!gsk_bounding_box_intersection (&b1, &b2, NULL))
    return 0;

  if (curve_near (curve1, curve2, 0.1))
    {
      t1[0] = 0;
      t2[0] = 0;
      p[0] = *gsk_curve_get_start_point (curve1);
      kind[0] = GSK_PATH_INTERSECTION_START;

      t1[1] = 1;
      t2[1] = 1;
      p[1] = *gsk_curve_get_end_point (curve1);
      kind[1] = GSK_PATH_INTERSECTION_END;

      return 2;
    }

  nn = curve_intersect (curve1, curve2, tt1, tt2, pp, 10);

  if (nn > degree[curve1->op] * degree[curve2->op])
    {
      float s1[2], s2[2];
      graphene_point_t q[2];

      if (find_coincidence (curve1, curve2, s1, s2, q))
        {
          t1[0] = s1[0];
          t2[0] = s2[0];
          p[0] = q[0];
          kind[0] = GSK_PATH_INTERSECTION_START;

          t1[1] = s1[1];
          t2[1] = s2[1];
          p[1] = q[1];
          kind[1] = GSK_PATH_INTERSECTION_END;

          return 2;
        }
    }

  if (n < nn)
    nn = n;

  for (int i = 0; i < nn; i++)
    {
      t1[i] = tt1[i];
      t2[i] = tt2[i];
      p[i] = pp[i];
      kind[i] = GSK_PATH_INTERSECTION_NORMAL;
    }

  return nn;
}

int
gsk_curve_self_intersect (const GskCurve   *curve,
                          float            *t,
                          graphene_point_t *p,
                          int               n)
{
  float tt[3], ss[3], s;
  graphene_point_t pp[3];
  int m;
  GskCurve cs, ce;

  if (curve->op != GSK_PATH_CUBIC)
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

  m = cubic_intersect (&cs, &ce, tt, ss, pp, 3);

  if (m > 1)
    {
      int num = 0;

      /* One of the (at most 2) intersections we found
       * must be the common point where we split the curve.
       * It will have a t value of 1 and an s value of 0.
       */
      if (fabs (tt[0] - 1) > 1e-3)
        {
          t[num] = tt[0] * s;
          p[num] = pp[0];
          num++;
        }
      else if (fabs (tt[1] - 1) > 1e-3)
        {
          t[num] = tt[1] * s;
          p[num] = pp[1];
          num++;
        }

      if (n == num)
        return num;

      if (fabs (ss[0]) > 1e-3)
        {
          t[num] = s + ss[0] * (1 - s);
          p[num] = pp[0];
          num++;
        }
      else if (fabs (ss[1]) > 1e-3)
        {
          t[num] = s + ss[1] * (1 - s);
          p[num] = pp[1];
          num++;
        }

      return num;
    }

  return 0;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
