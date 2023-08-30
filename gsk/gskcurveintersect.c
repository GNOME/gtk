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

/* vim:set foldmethod=marker expandtab: */
