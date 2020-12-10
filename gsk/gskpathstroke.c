/*
 * Copyright © 2020 Red Hat, Inc.
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

#define STROKE_DEBUG 1

#include "config.h"

#include "gskpathprivate.h"

#include "gskpathbuilder.h"
#include "gsksplineprivate.h"

#include "gskstrokeprivate.h"
#include "gskpathdashprivate.h"

#include "gdk/gdk-private.h"

#ifdef STROKE_DEBUG
static const char * op_to_string (GskPathOperation op) G_GNUC_UNUSED;
#endif

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))
#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))

/* Set n to the normal of the line through p0 and p1 */
static void
normal_vector (const graphene_point_t *p0,
               const graphene_point_t *p1,
               graphene_vec2_t        *n)
{
  graphene_vec2_init (n, p0->y - p1->y, p1->x - p0->x);
  graphene_vec2_normalize (n, n);
}

static void
direction_vector (const graphene_point_t *p0,
                  const graphene_point_t *p1,
                  graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static void
find_point_on_line (const graphene_point_t *p1,
                    const graphene_point_t *p2,
                    const graphene_point_t *q,
                    float                  *t)
{
  float tx = p2->x - p1->x;
  float ty = p2->y - p1->y;
  float sx = q->x - p1->x;
  float sy = q->y - p1->y;

  *t = (tx*sx + ty*sy) / (tx*tx + ty*ty);
}

static void
get_bezier (const graphene_point_t *points,
            int                     length,
            float                   t,
            graphene_point_t       *p)
{
  if (length == 1)
    *p = points[0];
  else
    {
      graphene_point_t *newpoints;
      int i;

      newpoints = g_alloca (sizeof (graphene_point_t) * (length - 1));
      for (i = 0; i < length - 1; i++)
        graphene_point_interpolate (&points[i], &points[i + 1], t, &newpoints[i]);
      get_bezier (newpoints, length - 1, t, p);
    }
}

static void
get_cubic (const graphene_point_t  points[4],
           float                   t,
           graphene_point_t       *p)
{
  get_bezier (points, 4, t, p);
}

static void
split_bezier_recurse (const graphene_point_t *points,
                      int                     length,
                      float                   t,
                      graphene_point_t       *left,
                      graphene_point_t       *right,
                      int                    *lpos,
                      int                    *rpos)
{
  if (length == 1)
    {
      left[*lpos] = points[0];
      (*lpos)++;
      right[*rpos] = points[length - 1];
      (*rpos)--;
    }
  else
    {
      graphene_point_t *newpoints;
      int i;

      newpoints = g_alloca (sizeof (graphene_point_t) * (length - 1));
      for (i = 0; i < length - 1; i++)
        {
          if (i == 0)
            {
              left[*lpos] = points[i];
              (*lpos)++;
            }
          if (i + 1 == length - 1)
            {
              right[*rpos] = points[i + 1];
              (*rpos)--;
            }
          graphene_point_interpolate (&points[i], &points[i + 1], t, &newpoints[i]);
        }
      split_bezier_recurse (newpoints, length - 1, t, left, right, lpos, rpos);
    }
}

/* Given Bezier control points and a t value between 0 and 1,
 * return new Bezier control points for two segments in left
 * and right that are obtained by splitting the curve at the
 * point for t.
 */
static void
split_bezier (const graphene_point_t *points,
              int                     length,
              float                   t,
              graphene_point_t       *left,
              graphene_point_t       *right)
{
  int lpos = 0;
  int rpos = length - 1;

  split_bezier_recurse (points, length, t, left, right, &lpos, &rpos);
}

static void
get_rational_bezier (const graphene_point_t *p,
                     float                  *w,
                     int                     l,
                     float                   t,
                     graphene_point_t       *q)
{
  if (l == 1)
    {
      q->x = p[0].x;
      q->y = p[0].y;
    }
  else
    {
      graphene_point_t *np;
      float *nw;
      int i;

      np = g_alloca (sizeof (graphene_point3d_t) * (l - 1));
      nw = g_alloca (sizeof (float) * (l - 1));

      for (i = 0; i < l - 1; i++)
        {
          nw[i] = (1 - t) * w[i] + t * w[i + 1];
          np[i].x = (1 - t) * (w[i]/nw[i]) * p[i].x + t * (w[i + 1]/nw[i]) * p[i + 1].x;
          np[i].y = (1 - t) * (w[i]/nw[i]) * p[i].y + t * (w[i + 1]/nw[i]) * p[i + 1].y;
        }
      get_rational_bezier (np, nw, l - 1, t, q);
    }
}

/* Given control points and weight for a rational quadratic Bezier
 * and a t in the range [0,1], compute the point on the curve at t
 */
static void
get_conic (const graphene_point_t points[3],
           float                  weight,
           float                  t,
           graphene_point_t      *p)
{
  get_rational_bezier (points, (float [3]) { 1, weight, 1}, 3, t, p);
}

static void
split_bezier3d_recurse (const graphene_point3d_t *p,
                        int                       l,
                        float                     t,
                        graphene_point3d_t       *left,
                        graphene_point3d_t       *right,
                        int                      *lpos,
                        int                      *rpos)
{
  if (l == 1)
    {
      left[*lpos] = p[0];
      right[*rpos] = p[0];
    }
  else
    {
      graphene_point3d_t *np;
      int i;

      np = g_alloca (sizeof (graphene_point3d_t) * (l - 1));
      for (i = 0; i < l - 1; i++)
        {
          if (i == 0)
            {
              left[*lpos] = p[i];
              (*lpos)++;
            }
          if (i + 1 == l - 1)
            {
              right[*rpos] = p[i + 1];
              (*rpos)--;
            }
          graphene_point3d_interpolate (&p[i], &p[i + 1], t, &np[i]);
        }
      split_bezier3d_recurse (np, l - 1, t, left, right, lpos, rpos);
    }
}

static void
split_bezier3d (const graphene_point3d_t *p,
                int                       l,
                float                     t,
                graphene_point3d_t       *left,
                graphene_point3d_t       *right)
{
  int lpos = 0;
  int rpos = l - 1;
  split_bezier3d_recurse (p, l, t, left, right, &lpos, &rpos);
}

/* not sure this is useful for anything in particular */
static void
get_conic_shoulder_point (const graphene_point_t  p[3],
                          float                   w,
                          graphene_point_t       *q)
{
  graphene_point_t m;

  graphene_point_interpolate (&p[0], &p[2], 0.5, &m);
  graphene_point_interpolate (&m, &p[1], w / (1 + w), q);
}

static gboolean
acceptable (float t)
{
  return 0 <= t && t <= 1;
}

/* compute the angle between a, b and c in the range of [0, 360] */
static float
three_point_angle (const graphene_point_t *a,
                   const graphene_point_t *b,
                   const graphene_point_t *c)
{
  float angle = atan2 (c->y - b->y, c->x - b->x) - atan2 (a->y - b->y, a->x - b->x);

  if (angle < 0)
    angle += 2 * M_PI;

  return RAD_TO_DEG (angle);
}

static gboolean
cubic_is_simple (const graphene_point_t *pts)
{
  float a1, a2, s;
  graphene_vec2_t n1, n2;

  a1 = three_point_angle (&pts[0], &pts[1], &pts[2]);
  a2 = three_point_angle (&pts[1], &pts[2], &pts[3]);

  if ((a1 < 180.f && a2 > 180.f) || (a1 > 180.f && a2 < 180.f))
    return FALSE;

  normal_vector (&pts[0], &pts[1], &n1);
  normal_vector (&pts[2], &pts[3], &n2);

  s = graphene_vec2_dot (&n1, &n2);

  if (fabs (acos (s)) >= M_PI / 3.f)
    return FALSE;

  return TRUE;
}

static float
cuberoot (float v)
{
  if (v < 0)
    return -pow (-v, 1.f / 3);
  return pow (v, 1.f / 3);
}

static int
get_cubic_roots (float pa,
                 float pb,
                 float pc,
                 float pd,
                 float roots[3])
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

/* Compute q = p + d * n */
static void
scale_point (const graphene_point_t *p,
             const graphene_vec2_t  *n,
             float                   d,
             graphene_point_t       *q)
{
  q->x = p->x + d * graphene_vec2_get_x (n);
  q->y = p->y + d * graphene_vec2_get_y (n);
}

static void
midpoint (const graphene_point_t *a,
          const graphene_point_t *b,
          graphene_point_t       *q)
{
  q->x = (a->x + b->x) / 2;
  q->y = (a->y + b->y) / 2;
}

/* Set p to the intersection of the lines through a, b and c, d.
 * Return the number of intersections found (0 or 1) */
static int
line_intersection (const graphene_point_t *a,
                   const graphene_point_t *b,
                   const graphene_point_t *c,
                   const graphene_point_t *d,
                   graphene_point_t       *p)
{
  float a1 = b->y - a->y;
  float b1 = a->x - b->x;
  float c1 = a1*a->x + b1*a->y;

  float a2 = d->y - c->y;
  float b2 = c->x - d->x;
  float c2 = a2*c->x+ b2*c->y;

  float det = a1*b2 - a2*b1;

  if (det == 0)
    {
      p->x = NAN;
      p->y = NAN;
      return 0;
    }
  else
    {
      p->x = (b2*c1 - b1*c2) / det;
      p->y = (a1*c2 - a2*c1) / det;
      return 1;
    }
}

static void
align_point (const graphene_point_t *p0,
             const graphene_point_t *a,
             const graphene_point_t *b,
             graphene_point_t       *q)
{
  graphene_vec2_t n;
  float angle;

  direction_vector (a, b, &n);
  angle = -atan2 (graphene_vec2_get_y (&n), graphene_vec2_get_x (&n));
  q->x = (p0->x - a->x) * cos (angle) - (p0->y - a->y) * sin (angle);
  q->y = (p0->x - a->x) * sin (angle) + (p0->y - a->y) * cos (angle);
}

static int
cmpfloat (const void *p1, const void *p2)
{
  const float *f1 = p1;
  const float *f2 = p2;
  return f1 < f2 ? -1 : (f1 > f2 ? 1 : 0);
}

/* Place intersections between the line and the curve in q,
 * and their Bezier positions in t.
 * Return the number of intersections found (0 to 3).
 */
static int
line_curve_intersection (const graphene_point_t *a,
                         const graphene_point_t *b,
                         const graphene_point_t  pts[4],
                         float                   t[3],
                         graphene_point_t        q[3])
{
  graphene_point_t p[4];
  int n, i;

  align_point (&pts[0], a, b, &p[0]);
  align_point (&pts[1], a, b, &p[1]);
  align_point (&pts[2], a, b, &p[2]);
  align_point (&pts[3], a, b, &p[3]);

  n = get_cubic_roots (p[0].y, p[1].y, p[2].y, p[3].y, t);
  qsort (t, n, sizeof (float), cmpfloat);
  for (i = 0; i < n; i++)
    get_cubic (pts, t[i], &q[i]);

  return n;
}

typedef struct _Curve Curve;
typedef struct _CurveClass CurveClass;

struct _Curve
{
  CurveClass *class;
  graphene_point_t p[4];
  float weight;
};

struct _CurveClass
{
  const char *name;
  void (* get_bounds) (Curve *self, graphene_rect_t *bounds);
  void (* get_point)  (Curve *self, float t, graphene_point_t *point);
  void (* split)      (Curve *self, float t, Curve *left, Curve *right);
};

static void
line_segment_get_bounds (Curve *c, graphene_rect_t *bounds)
{
  graphene_rect_init (bounds, c->p[0].x, c->p[0].y, 0, 0);
  graphene_rect_expand (bounds, &c->p[1], bounds);
}

static void
line_segment_get_point (Curve *c, float t, graphene_point_t *point)
{
  point->x = (1 - t) * c->p[0].x + t * c->p[1].x;
  point->y = (1 - t) * c->p[0].y + t * c->p[1].y;
}

static void init_line_segment (Curve            *c,
                               graphene_point_t *a,
                               graphene_point_t *b);

static void
line_segment_split (Curve *c, float t, Curve *left, Curve *right)
{
  graphene_point_t p;

  line_segment_get_point (c, t, &p);
  init_line_segment (left, &c->p[0], &p);
  init_line_segment (right, &p, &c->p[1]);
}

static CurveClass LINE_SEGMENT_CLASS =
{
  "LineSegment",
  line_segment_get_bounds,
  line_segment_get_point,
  line_segment_split
};

static void
init_line_segment (Curve            *c,
                   graphene_point_t *a,
                   graphene_point_t *b)
{
  c->class = &LINE_SEGMENT_CLASS;
  c->p[0] = *a;
  c->p[1] = *b;
}

static void
cubic_get_point (Curve *c, float t, graphene_point_t *point)
{
  get_bezier (c->p, 4, t, point);
}

static int
get_cubic_extrema (float pa,
                   float pb,
                   float pc,
                   float pd,
                   float roots[2])
{
  float a, b, c;
  float d, t;
  int n_roots = 0;

  a = 3 * (pd - 3*pc + 3*pb - pa);
  b = 6 * (pc - 2*pb + pa);
  c = 3 * (pb - pa);

  if (fabs (a) > 0.0001)
    {
      if (b*b > 4*a*c)
        {
          d = sqrt (b*b - 4*a*c);
          t = (-b + d)/(2*a);
          if (acceptable (t))
            roots[n_roots++] = t;
          t = (-b - d)/(2*a);
          if (acceptable (t))
            roots[n_roots++] = t;
        }
      else
        {
          t = -b / (2*a);
          if (acceptable (t))
            roots[n_roots++] = t;
        }
    }
  else if (fabs (b) > 0.0001)
    {
      t = -c / b;
      if (acceptable (t))
        roots[n_roots++] = t;
    }

  return n_roots;
}

static void
cubic_get_bounds (Curve *c, graphene_rect_t *bounds)
{
  graphene_point_t p;
  float t[4];
  int n, i;

  graphene_rect_init (bounds, c->p[0].x, c->p[0].y, 0, 0);
  graphene_rect_expand (bounds, &c->p[3], bounds);

  n = get_cubic_extrema (c->p[0].x, c->p[1].x, c->p[2].x, c->p[3].x, t);
  n += get_cubic_extrema (c->p[0].y, c->p[1].y, c->p[2].y, c->p[3].y, &t[n]);
  for (i = 0; i < n; i++)
    {
      cubic_get_point (c, t[i], &p);
      graphene_rect_expand (bounds, &p, bounds);
    }
}

static void init_cubic (Curve            *c,
                        graphene_point_t  p[4]);

static void
cubic_split (Curve *c, float t, Curve *left, Curve *right)
{
  split_bezier (c->p, 4, t, left->p, right->p);
  init_cubic (left, left->p);
  init_cubic (right, right->p);
}

static CurveClass CUBIC_CLASS =
{
  "Cubic",
  cubic_get_bounds,
  cubic_get_point,
  cubic_split
};

static void
init_cubic (Curve            *c,
            graphene_point_t  p[4])
{
  c->class = &CUBIC_CLASS;
  c->p[0] = p[0];
  c->p[1] = p[1];
  c->p[2] = p[2];
  c->p[3] = p[3];
}

static void
conic_get_point (Curve *c, float t, graphene_point_t *point)
{
  get_rational_bezier (c->p, (float [3]) { 1, c->weight, 1}, 3, t, point);
}

/* Solve N = 0 where N is the numerator of derivative of P/Q, with
 * P = (1-t)^a + 2t(1-t)wb + t^2c
 * Q = (1-t)^2 + 2t(1-t)w + t^2
 */
static int
get_conic_extrema (float a, float b, float c, float w, float t[10])
{
  float q, tt;
  int n = 0;
  float w2 = w*w;
  float wac = (w - 1)*(a - c);

  if (wac != 0)
    {
      q = - sqrt (a*a - 4*a*b*w2 + 4*a*c*w2 - 2*a*c + 4*b*b*w2 - 4*b*c*w2 + c*c);

      tt = (- q + 2*a*w - a - 2*b*w + c)/(2*wac);

      if (acceptable (tt))
        t[n++] = tt;

      tt = (q + 2*a*w - a - 2*b*w + c)/(2*wac);

      if (acceptable (tt))
        t[n++] = tt;
    }

  if (w * (b - c) != 0 && a == c)
    t[n++] = 0.5;

  if (w == 1 && a - 2*b + c != 0)
    {
      tt = (a - b) / (a - 2*b + c);
      if (acceptable (tt))
        t[n++] = tt;
    }

  return n;
}

static void
conic_get_bounds (Curve           *c,
                  graphene_rect_t *bounds)
{
  graphene_point_t p;
  float t[10];
  int i, n;

  graphene_rect_init (bounds, c->p[0].x, c->p[0].y, 0, 0);
  graphene_rect_expand (bounds, &c->p[2], bounds);

  n = get_conic_extrema (c->p[0].x, c->p[1].x, c->p[2].x, c->weight, t);
  n += get_conic_extrema (c->p[0].y, c->p[1].y, c->p[2].y, c->weight, &t[n]);

  for (i = 0; i < n; i++)
    {
      conic_get_point (c, t[i], &p);
      graphene_rect_expand (bounds, &p, bounds);
    }
}

static void init_conic (Curve                  *c,
                        const graphene_point_t  p[3],
                        float                   weight);

static void
conic_split (Curve *c, float t, Curve *left, Curve *right)
{
  /* Given control points and weight for a rational quadratic
   * Bezier and t, create two sets of the same that give the
   * same curve as the original and split the curve at t.
   */
  graphene_point3d_t p[3];
  graphene_point3d_t l[3], r[3];
  int i;

  /* do de Casteljau in homogeneous coordinates... */
  for (i = 0; i < 3; i++)
    {
      p[i].x = c->p[i].x;
      p[i].y = c->p[i].y;
      p[i].z = 1;
    }

  p[1].x *= c->weight;
  p[1].y *= c->weight;
  p[1].z *= c->weight;

  split_bezier3d (p, 3, t, l, r);

  /* then project the control points down */
  for (i = 0; i < 3; i++)
    {
      left->p[i].x = l[i].x / l[i].z;
      left->p[i].y = l[i].y / l[i].z;
      right->p[i].x = r[i].x / r[i].z;
      right->p[i].y = r[i].y / r[i].z;
    }

  /* normalize the outer weights to be 1 by using
   * the fact that weights w_i and c*w_i are equivalent
   * for any nonzero constant c
   */
  for (i = 0; i < 3; i++)
    {
      l[i].z /= l[0].z;
      r[i].z /= r[2].z;
    }

  /* normalize the inner weight to be 1 by using
   * the fact that w_0*w_2/w_1^2 is a constant for
   * all equivalent weights.
   */
  left->weight = l[1].z / sqrt (l[2].z);
  right->weight = r[1].z / sqrt (r[0].z);

  init_conic (left, left->p, left->weight);
  init_conic (right, right->p, right->weight);
}

static CurveClass CONIC_CLASS =
{
  "Conic",
  conic_get_bounds,
  conic_get_point,
  conic_split
};

static void
init_conic (Curve                  *c,
            const graphene_point_t  p[3],
            float                   weight)
{
  c->class = &CONIC_CLASS;
  c->p[0] = p[0];
  c->p[1] = p[1];
  c->p[2] = p[2];
  c->weight = weight;
};

static void
curve_get_bounds (Curve *curve, graphene_rect_t *bounds)
{
  curve->class->get_bounds (curve, bounds);
}

static void
curve_get_point (Curve *curve, float t, graphene_point_t *point)
{
  curve->class->get_point (curve, t, point);
}

static void
curve_split (Curve *curve, float t, Curve *left, Curve *right)
{
  curve->class->split (curve, t, left, right);
}

static void
init_curve (Curve            *c,
            GskPathOperation  op,
            graphene_point_t  p[4],
            float             w)
{
  switch (op)
    {
    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      init_line_segment (c, &p[0], &p[1]);
      break;

    case GSK_PATH_CURVE:
      init_cubic (c, p);
      break;

    case GSK_PATH_CONIC:
      init_conic (c, p, w);
      break;

    case GSK_PATH_MOVE:
    default:
      g_assert_not_reached ();
    }
}

static int
line_segment_intersection (const graphene_point_t *p1,
                           const graphene_point_t *p2,
                           const graphene_point_t *p3,
                           const graphene_point_t *p4,
                           float                  *t,
                           float                  *s,
                           graphene_point_t       *p)
{
  float a1 = p2->x - p1->x;
  float b1 = p2->y - p1->y;

  float a2 = p4->x - p3->x;
  float b2 = p4->y - p3->y;

  float det = a1 * b2 - a2 * b1;
  float tt, ss;

  if (det != 0)
    {
      tt = ((p3->x - p1->x) * b2 - (p3->y - p1->y) * a2) / det;
      ss = ((p3->y - p1->y) * a2 - (p3->x - p1->x) * b1) / det;

      if (acceptable (tt) && acceptable (ss))
        {
          p->x = p1->x + tt * (p2->x - p1->x);
          p->y = p1->y + tt * (p2->y - p1->y);

          *t = tt;
          *s = ss;
          return 1;
        }
    }

  return 0;
}

static void
intersection_recurse (Curve            *c1,
                      Curve            *c2,
                      float             t1,
                      float             t2,
                      float             s1,
                      float             s2,
                      float            *t,
                      float            *s,
                      graphene_point_t *q,
                      int               n,
                      int              *pos)
{
  graphene_rect_t b1, b2;
  float d;
  float e;
  Curve p11, p12, p21, p22;

  if (*pos == 9)
    return;

  curve_get_bounds (c1, &b1);
  curve_get_bounds (c2, &b2);

  if (!graphene_rect_intersection (&b1, &b2, NULL))
    return;

  d = (t2 - t1) / 2;
  e = (s2 - s1) / 2;

  if (b1.size.width < 0.1 && b1.size.height < 0.1 &&
      b2.size.width < 0.1 && b2.size.height < 0.1)
    {
      t[*pos] = t1 + d;
      s[*pos] = s1 + e;
      curve_get_point (c1, 0.5, &q[*pos]);
      (*pos)++;
      return;
    }

  curve_split (c1, 0.5, &p11, &p12);
  curve_split (c2, 0.5, &p21, &p22);

  intersection_recurse (&p11, &p21, t1, t1 + d, s1, s1 + e, t, s, q, n, pos);
  intersection_recurse (&p11, &p22, t1, t1 + d, s1 + e, s2, t, s, q, n, pos);
  intersection_recurse (&p12, &p21, t1 + d, t2, s1, s1 + e, t, s, q, n, pos);
  intersection_recurse (&p12, &p22, t1 + d, t2, s1 + e, s2, t, s, q, n, pos);
}

/* Place intersections between the curves in q, and
 * their Bezier positions in t and s, up to n.
 * Return the number of intersections found.
 */
static int
curve_intersection (Curve            *c1,
                    Curve            *c2,
                    float            *t,
                    float            *s,
                    graphene_point_t *q,
                    int               n)
{
  int i, pos;

  if (c1->class == &LINE_SEGMENT_CLASS && c2->class == &LINE_SEGMENT_CLASS)
    {
      return line_segment_intersection (&c1->p[0], &c1->p[1], &c2->p[0], &c2->p[1], t, s, q);
    }
  else if (c1->class == &LINE_SEGMENT_CLASS && c2->class == &CUBIC_CLASS)
    {
      pos = line_curve_intersection (&c1->p[0], &c1->p[1], c2->p, s, q);
      for (i = 0; i < pos; i++)
        find_point_on_line (&c1->p[0], &c1->p[1], &q[i], &t[i]);
      return pos;
    }
  else if (c1->class == &CUBIC_CLASS && c2->class == &LINE_SEGMENT_CLASS)
    {
      pos = line_curve_intersection (&c2->p[0], &c2->p[1], c1->p, t, q);
      for (i = 0; i < pos; i++)
        find_point_on_line (&c2->p[0], &c2->p[1], &q[i], &s[i]);
      return pos;
    }
  else
    {
      pos = 0;
      intersection_recurse (c1, c2, 0, 1, 0, 1, t, s, q, n, &pos);
      return pos;
    }
}

typedef struct
{
  GskCurve curve;

  graphene_point_t r[4]; /* offset to the right */
  graphene_point_t l[4]; /* offset to the left */
  graphene_point_t re[2]; /* intersection of adjacent r lines of this and next op */
  graphene_point_t le[2]; /* intersection of adjacent l lines of this and next op */
  float angle[2]; /* angles between tangents at the both ends */
} PathOpData;

/* Compute the curve that results from offsetting the control points */
static void
offset_curve (const GskCurve *c,
              float           distance,
              GskCurve       *o)
{
  graphene_vec2_t n;
  graphene_point_t p[4];

  switch (c->op)
    {
    case GSK_PATH_LINE:
      normal_vector (&c->line.points[0], &c->line.points[1], &n);
      scale_point (&c->line.points[0], &n, distance, &p[0]);
      scale_point (&c->line.points[1], &n, distance, &p[1]);
      gsk_curve_initialize (o, gsk_patho_encode (GSK_PATH_LINE, p);
      break;
    case GSK_PATH_CURVE:
      normal_vector (&c->curve.points[0], &c->curve.points[1], &n1);
      scale_point (&c->curve.points[0], &n, distance, &p[0]);
      scale_point (&c->curve.points[1], &n, distance, &p[1]);
      break;
    case GSK_PATH_CONIC:
      break;
    case GSK_PATH_CLOSE:
    case GSK_PATH_MOVE:
    default:
      g_assert_not_reached ();
    }
}

static void
compute_offsets (PathOpData *op,
                 float       d)
{
  graphene_vec2_t n1, n2, n3;

  normal_vector (&op->pts[0], &op->pts[1], &n1);
  normal_vector (&op->pts[op->n_pts - 1], &op->pts[op->n_pts - 2], &n3);

  scale_point (&op->pts[0], &n1, d, &op->r[0]);
  scale_point (&op->pts[0], &n1, -d, &op->l[0]);

  scale_point (&op->pts[op->n_pts - 1], &n3, -d, &op->r[op->n_pts - 1]);
  scale_point (&op->pts[op->n_pts - 1], &n3, d, &op->l[op->n_pts - 1]);

  if (op->op == GSK_PATH_CURVE)
    {
      graphene_point_t m1, m2, m3, m4;

      /* Simply scale control points, a la Tiller and Hanson */

      normal_vector (&op->pts[1], &op->pts[2], &n2);

      scale_point (&op->pts[1], &n1, d, &m1);
      scale_point (&op->pts[2], &n3, -d, &m4);

      scale_point (&op->pts[1], &n2, d, &m2);
      scale_point (&op->pts[2], &n2, d, &m3);

      if (!line_intersection (&op->r[0], &m1, &m2, &m3, &op->r[1]))
        op->r[1] = m1;

      if (!line_intersection (&m2, &m3, &m4, &op->r[3], &op->r[2]))
        op->r[2] = m4;

      scale_point (&op->pts[1], &n1, -d, &m1);
      scale_point (&op->pts[2], &n3, d, &m4);

      scale_point (&op->pts[1], &n2, -d, &m2);
      scale_point (&op->pts[2], &n2, -d, &m3);

      if (!line_intersection (&op->l[0], &m1, &m2, &m3, &op->l[1]))
        op->l[1] = m1;

      if (!line_intersection (&m2, &m3, &m4, &op->l[3], &op->l[2]))
        op->l[2] = m4;
    }
  else if (op->op == GSK_PATH_CONIC)
    {
      graphene_point_t m1, m2;

      scale_point (&op->pts[1], &n1, d, &m1);
      scale_point (&op->pts[1], &n3, -d, &m2);
      line_intersection (&op->r[0], &m1, &op->r[2], &m2, &op->r[1]);

      scale_point (&op->pts[1], &n1, -d, &m1);
      scale_point (&op->pts[1], &n3, d, &m2);
      line_intersection (&op->l[0], &m1, &op->l[2], &m2, &op->l[1]);
    }

  op->re[0] = op->r[0];
  op->le[0] = op->l[0];
  op->re[1] = op->r[op->n_pts - 1];
  op->le[1] = op->l[op->n_pts - 1];
  op->angle[0] = op->angle[1] = 180.f;
}

static int
find_smallest (float t[], int n)
{
  float d = t[0];
  int i0 = 0;
  int i;

  for (i = 1; i < n; i++)
    {
      if (t[i] < d)
        {
          d = t[i];
          i0 = i;
        }
    }

  return i0;
}

static int
find_largest (float t[], int n)
{
  float d = t[0];
  int i0 = 0;
  int i;

  for (i = 1; i < n; i++)
    {
      if (t[i] > d)
        {
          d = t[i];
          i0 = i;
        }
    }

  return i0;
}

static void
compute_intersections (PathOpData *op1,
                       PathOpData *op2)
{
  op1->angle[1] = three_point_angle (&op1->pts[op1->n_pts - 2],
                                     &op1->pts[op1->n_pts - 1],
                                     &op2->pts[1]);

  op2->angle[0] = op1->angle[1];

  if (fabs (op1->angle[1] - 180.f) >= 1)
    {
      graphene_point_t p[9];
      float t[9];
      float s[9];
      int i, n;
      Curve c1, c2, cl, cr;

      if (!line_intersection (&op1->r[op1->n_pts - 2], &op1->r[op1->n_pts - 1],
                              &op2->r[0], &op2->r[1],
                              &op1->re[1]))
        midpoint (&op1->r[op1->n_pts - 1], &op2->r[0], &op1->re[1]);

      if (!line_intersection (&op1->l[op1->n_pts - 2], &op1->l[op1->n_pts - 1],
                              &op2->l[0], &op2->l[1],
                              &op1->le[1]))
        midpoint (&op1->l[op1->n_pts - 1], &op2->l[0], &op1->le[1]);

      if (op1->angle[1] > 180.f)
        {
          init_curve (&c1, op1->op, op1->r, op1->w);
          init_curve (&c2, op2->op, op2->r, op2->w);
          n = curve_intersection (&c1, &c2, t, s, p, 9);
          if (n > 0)
            {
              i = find_largest (t, n);
              curve_split (&c1, t[i], &cl, &cr);
              for (i = 0; i < op1->n_pts; i++)
                op1->r[i] = cl.p[i];
              op1->re[1] = op1->r[op1->n_pts - 1];
              i = find_smallest (s, n);
              curve_split (&c2, s[i], &cl, &cr);
              for (i = 0; i < op2->n_pts; i++)
                op2->r[i] = cr.p[i];
            }
        }
      else
        {
          init_curve (&c1, op1->op, op1->l, op1->w);
          init_curve (&c2, op2->op, op2->l, op2->w);
          n = curve_intersection (&c1, &c2, t, s, p, 9);
          if (n > 0)
            {
              i = find_largest (t, n);
              curve_split (&c1, t[i], &cl, &cr);
              for (i = 0; i < op1->n_pts; i++)
                op1->l[i] = cl.p[i];
              op1->le[1] = op1->l[op1->n_pts - 1];
              i = find_smallest (s, n);
              curve_split (&c2, s[i], &cl, &cr);
              for (i = 0; i < op2->n_pts; i++)
                op2->l[i] = cr.p[i];
            }
        }
    }
  else
    {
      midpoint (&op1->r[op1->n_pts - 1], &op2->r[0], &op1->re[1]);
      midpoint (&op1->l[op1->n_pts - 1], &op2->l[0], &op1->le[1]);
    }

  op2->re[0] = op1->re[1];
  op2->le[0] = op1->le[1];
}

static PathOpData *
path_op_data_new (GskPathOperation        op,
                  const graphene_point_t *pts,
                  float                   w,
                  gsize                   n_pts)
{
  PathOpData *d;
  int i;

  d = g_new0 (PathOpData, 1);
  if (op == GSK_PATH_CONIC)
    gsk_curve_init (&d->curve, gsk_pathop_encode (op, { pts[0], pts[1], { w,}, pts[2] }));
  else
    gsk_curve_init (&d->curve, gsk_pathop_encode (op, pts));

#if STROKE_DEBUG
  /* detect uninitialized values */
  for (i = 0; i < n_pts; i++)
    d->l[i].x = d->l[i].y = d->r[i].x = d->r[i].y = NAN;
  for (i = 0; i < 2; i++)
    {
      d->le[i].x = d->le[i].y = d->re[i].x = d->re[i].y = NAN;
      d->angle[i] = NAN;
    }
#endif

  return d;
}

typedef struct
{
  GskPathBuilder *builder;
  GskStroke *stroke;
  GList *ops;
  graphene_point_t start;
  gboolean has_start;
  gboolean print;
} AddOpData;

static void
subdivide_and_add (const graphene_point_t  pts[4],
                   AddOpData              *data,
                   int                     level)
{
  if (level == 0 || cubic_is_simple (pts))
    data->ops = g_list_prepend (data->ops,
                                path_op_data_new (GSK_PATH_CURVE, pts, 0, 4));
  else
    {
      graphene_point_t left[4];
      graphene_point_t right[4];

      split_bezier (pts, 4, 0.5, left, right);

      subdivide_and_add (left, data, level - 1);
      subdivide_and_add (right, data, level - 1);
    }
}

static void
subdivide_and_add_conic (const graphene_point_t  pts[4],
                         float                   weight,
                         AddOpData              *data)
{
  Curve c, c1, c2;

  init_conic (&c, pts, weight);
  curve_split (&c, 0.5, &c1, &c2);
  data->ops = g_list_prepend (data->ops, path_op_data_new (GSK_PATH_CONIC, c1.p, c1.weight, 3));
  data->ops = g_list_prepend (data->ops, path_op_data_new (GSK_PATH_CONIC, c2.p, c2.weight, 3));
}

#ifdef STROKE_DEBUG

static const char *
op_to_string (GskPathOperation op)
{
  const char *names[] = { "MOVE", "CLOSE", "LINE", "CURVE" };
  return names[op];
}

enum {
  STROKE_DEBUG_LEFT_CURVES         = 1 << 0,
  STROKE_DEBUG_RIGHT_CURVES        = 1 << 1,
  STROKE_DEBUG_LEFT_POINTS         = 1 << 2,
  STROKE_DEBUG_RIGHT_POINTS        = 1 << 3,
  STROKE_DEBUG_OFFSET_LINES        = 1 << 4,
  STROKE_DEBUG_LEFT_INTERSECTIONS  = 1 << 5,
  STROKE_DEBUG_RIGHT_INTERSECTIONS = 1 << 6,
  STROKE_DEBUG_CURVE_POINTS        = 1 << 7,
  STROKE_DEBUG_CURVE_LINES         = 1 << 8
};

static void
emit_debug (GskPathBuilder *builder,
            GList          *ops)
{
  GList *l;
  PathOpData *op;
  int i;
  const GdkDebugKey debug_keys[] = {
    { "left-curves", STROKE_DEBUG_LEFT_CURVES, "Show left offset curve" },
    { "right-curves", STROKE_DEBUG_RIGHT_CURVES, "Show right offset curve" },
    { "offset-curves", STROKE_DEBUG_LEFT_CURVES|STROKE_DEBUG_RIGHT_CURVES, "Show offset curves" },
    { "left-points", STROKE_DEBUG_LEFT_POINTS, "Show left offset points" },
    { "right-points", STROKE_DEBUG_RIGHT_POINTS, "Show right offset points" },
    { "offset-points", STROKE_DEBUG_LEFT_POINTS|STROKE_DEBUG_RIGHT_POINTS, "Show offset points" },
    { "offset-lines", STROKE_DEBUG_OFFSET_LINES, "Show offset lines" },
    { "left-intersections", STROKE_DEBUG_LEFT_INTERSECTIONS, "Show left intersection" },
    { "right-intersections", STROKE_DEBUG_RIGHT_INTERSECTIONS, "Show right intersections" },
    { "intersections", STROKE_DEBUG_LEFT_INTERSECTIONS|STROKE_DEBUG_RIGHT_INTERSECTIONS, "Show intersection" },
    { "curve-points", STROKE_DEBUG_CURVE_POINTS, "Show curve points" },
    { "curve-lines", STROKE_DEBUG_CURVE_LINES, "Show curve lines" },
  };
  static guint debug;
  static int initialized = 0;

  if (!initialized)
    {
      debug = gdk_parse_debug_var ("STROKE_DEBUG", debug_keys, G_N_ELEMENTS (debug_keys));
      initialized = 1;
    }

  for (l = ops; l; l = l->next)
    {
      op = l->data;

      if (debug & STROKE_DEBUG_LEFT_CURVES)
        {
          gsk_path_builder_move_to (builder, op->l[0].x, op->l[0].y);
          if (op->op == GSK_PATH_CURVE)
            gsk_path_builder_curve_to (builder,
                                       op->l[1].x, op->l[1].y,
                                       op->l[2].x, op->l[2].y,
                                       op->l[3].x, op->l[3].y);
          else if (op->op == GSK_PATH_LINE)
            gsk_path_builder_line_to (builder, op->l[1].x, op->l[1].y);
        }
      if (debug & STROKE_DEBUG_RIGHT_CURVES)
        {
          gsk_path_builder_move_to (builder, op->r[0].x, op->r[0].y);
          if (op->op == GSK_PATH_CURVE)
            gsk_path_builder_curve_to (builder,
                                       op->r[1].x, op->r[1].y,
                                       op->r[2].x, op->r[2].y,
                                       op->r[3].x, op->r[3].y);
          else if (op->op == GSK_PATH_LINE)
            gsk_path_builder_line_to (builder, op->r[1].x, op->r[1].y);
        }

      for (i = 0; i < op->n_pts; i++)
        {
          if (debug & STROKE_DEBUG_LEFT_POINTS)
            gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (op->l[i].x, op->l[i].y), 2);
          if (debug & STROKE_DEBUG_RIGHT_POINTS)
            gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (op->r[i].x, op->r[i].y), 2);
          if (debug & STROKE_DEBUG_CURVE_POINTS)
            gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (op->pts[i].x, op->pts[i].y), 2);
          if (debug & STROKE_DEBUG_OFFSET_LINES)
            {
              gsk_path_builder_move_to (builder, op->r[i].x, op->r[i].y);
              gsk_path_builder_line_to (builder, op->pts[i].x, op->pts[i].y);
              gsk_path_builder_line_to (builder, op->l[i].x, op->l[i].y);
            }
       }
      for (i = 0; i < 1; i++)
        {
          if (debug & STROKE_DEBUG_LEFT_INTERSECTIONS)
            gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (op->le[i].x, op->le[i].y), 2);
          if (debug & STROKE_DEBUG_RIGHT_INTERSECTIONS)
            gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (op->re[i].x, op->re[i].y), 2);
        }

      if (debug & STROKE_DEBUG_CURVE_LINES)
        {
          gsk_path_builder_move_to (builder, op->pts[0].x, op->pts[0].y);
          for (i = 1; i < op->n_pts; i++)
            gsk_path_builder_line_to (builder, op->pts[i].x, op->pts[i].y);
        }
    }
}
#endif

static void
add_cap (GskPathBuilder         *builder,
         const graphene_point_t *s,
         const graphene_point_t *e,
         GskStroke              *stroke)
{
  switch (stroke->line_cap)
    {
    case GSK_LINE_CAP_BUTT:
      gsk_path_builder_line_to (builder, e->x, e->y);
      break;

    case GSK_LINE_CAP_ROUND:
      gsk_path_builder_svg_arc_to (builder,
                                   stroke->line_width / 2, stroke->line_width / 2,
                                   0, 1, 0,
                                   e->x, e->y);
      break;

    case GSK_LINE_CAP_SQUARE:
      {
        float cx = (s->x + e->x) / 2;
        float cy = (s->y + e->y) / 2;
        float dx = s->y - cy;
        float dy = - s->x + cx;

        gsk_path_builder_line_to (builder, s->x + dx, s->y + dy);
        gsk_path_builder_line_to (builder, e->x + dx, e->y + dy);
        gsk_path_builder_line_to (builder, e->x, e->y);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
stroke_ops (GList            *ops,
            graphene_point_t *start,
            GskStroke        *stroke,
            GskPathBuilder   *builder)
{
  GList *l;
  GList *last = NULL;
  PathOpData *op;
  PathOpData *op1;
  gboolean draw_caps;

  /* Compute offset start and end points */
  for (l = ops; l; l = l->next)
    {
      op = l->data;
      last = l;
      compute_offsets (op, stroke->line_width / 2);
    }

  /* Compute intersections */
  for (l = ops; l; l = l->next)
    {
      op = l->data;
      last = l;
      if (l->next != NULL)
        {
          op1 = l->next->data;
          if (op1->op == GSK_PATH_CLOSE)
            {
              if (graphene_point_near (&op1->pts[0], &op1->pts[1], 0.0001))
                {
                  compute_intersections (op, (PathOpData *)ops->data);
                  op1->re[0] = op1->r[0] = op->re[op1->n_pts - 1];
                  op1->le[0] = op1->l[0] = op->le[op1->n_pts - 1];

                  op1->re[1] = op1->r[1] = ((PathOpData *)ops->data)->re[0];
                  op1->le[1] = op1->l[1] = ((PathOpData *)ops->data)->le[0];
                }
              else
                {
                  compute_intersections (op, op1);
                  compute_intersections (op1, (PathOpData *)ops->data);
                }
            }
          else
            {
              compute_intersections (op, op1);
            }
        }
    }

#if 0
  for (l = ops; l; l = l->next)
    {
      int i;

      op = l->data;

      for (i = 0; i < op->n_pts; i++)
        {
          g_assert_true (!isnan (op->l[i].x) && !isnan (op->l[i].y));
          g_assert_true (!isnan (op->r[i].x) && !isnan (op->r[i].y));
        }
      for (i = 0; i < 2; i++)
        {
          g_assert_true (!isnan (op->le[i].x) && !isnan (op->le[i].y));
          g_assert_true (!isnan (op->re[i].x) && !isnan (op->re[i].y));
          g_assert_true (!isnan (op->angle[i]));
        }
    }
#endif

  draw_caps = TRUE;

  if (ops == NULL && stroke->line_cap == GSK_LINE_CAP_BUTT)
    draw_caps = FALSE; /* isolated points have no butts */

  /* Walk the ops for the right edge */
  last = NULL;
  for (l = ops; l; l = l->next)
    {
      op = l->data;
      last = l;

      if (l->prev == NULL)
        gsk_path_builder_move_to (builder, op->re[0].x, op->re[0].y);
      switch (op->op)
        {
        case GSK_PATH_MOVE:
          g_assert_not_reached ();
          break;

        case GSK_PATH_CLOSE:
          draw_caps = FALSE;
          if (graphene_point_near (&op->pts[0], &op->pts[1], 0.0001))
            break;
          G_GNUC_FALLTHROUGH;

        case GSK_PATH_LINE:
          if (op->angle[1] >= 181)
            gsk_path_builder_line_to (builder, op->re[1].x, op->re[1].y);
          else
            gsk_path_builder_line_to (builder, op->r[1].x, op->r[1].y);
          break;

        case GSK_PATH_CURVE:
          if (op->angle[1] >= 181)
            gsk_path_builder_curve_to (builder, op->r[1].x, op->r[1].y,
                                                op->r[2].x, op->r[2].y,
                                                op->re[1].x, op->re[1].y);
          else
            gsk_path_builder_curve_to (builder, op->r[1].x, op->r[1].y,
                                                op->r[2].x, op->r[2].y,
                                                op->r[3].x, op->r[3].y);
          break;

        case GSK_PATH_CONIC:
          if (op->angle[1] >= 181)
            gsk_path_builder_conic_to (builder, op->r[1].x, op->r[1].y,
                                                op->re[1].x, op->re[1].y,
                                                op->w);
          else
            gsk_path_builder_conic_to (builder, op->r[1].x, op->r[1].y,
                                                op->r[2].x, op->r[2].y,
                                                op->w);
          break;

        default:
          g_assert_not_reached ();
        }

      if (l->next || op->op == GSK_PATH_CLOSE)
        {
          op1 = l->next ? l->next->data : ops->data;

          if (op->angle[1] <= 179) /* avoid rounding */
            {
              /* Deal with joins */
              switch (stroke->line_join)
                {
                case GSK_LINE_JOIN_MITER:
                case GSK_LINE_JOIN_MITER_CLIP:
                  if (op->angle[1] != 0 && fabs (1 / sin (DEG_TO_RAD (op->angle[1]) / 2) <= stroke->miter_limit))
                    {
                      gsk_path_builder_line_to (builder, op->re[1].x, op->re[1].y);
                      gsk_path_builder_line_to (builder, op1->r[0].x, op1->r[0].y);
                    }
                  else if (stroke->line_join == GSK_LINE_JOIN_MITER_CLIP)
                    {
                      graphene_point_t p, q, p1, p2;
                      graphene_vec2_t t, n;

                      direction_vector (&op->pts[op->n_pts - 1], &op->re[1], &t);
                      scale_point (&op->pts[op->n_pts - 1], &t, stroke->line_width * stroke->miter_limit / 2, &p);

                      normal_vector (&op->pts[op->n_pts - 1], &op->re[1], &n);
                      scale_point (&p, &n, 1, &q);

                      line_intersection (&p, &q, &op->r[1], &op->re[1], &p1);
                      line_intersection (&p, &q, &op1->r[0], &op->re[1], &p2);

                      gsk_path_builder_line_to (builder, p1.x, p1.y);
                      gsk_path_builder_line_to (builder, p2.x, p2.y);
                      gsk_path_builder_line_to (builder, op1->r[0].x, op1->r[0].y);
                    }
                  else
                    {
                      gsk_path_builder_line_to (builder, op1->r[0].x, op1->r[0].y);
                    }
                  break;

                case GSK_LINE_JOIN_BEVEL:
                  gsk_path_builder_line_to (builder, op1->r[0].x, op1->r[0].y);
                  break;

                case GSK_LINE_JOIN_ROUND:
                  gsk_path_builder_svg_arc_to (builder,
                                               stroke->line_width / 2, stroke->line_width / 2,
                                               0, 0, 0,
                                               op1->r[0].x, op1->r[0].y);
                  break;

                default:
                  g_assert_not_reached ();
                }
            }
        }
    }

  if (draw_caps)
    {
      /* Deal with the cap at the end */
      if (ops)
        {
          add_cap (builder, &op->re[1], &op->le[1], stroke);
        }
      else
        {
          graphene_point_t s, e;

          /* an isolated point, we have it in start */
          e = s = *start;
          s.y += stroke->line_width / 2;
          e.y -= stroke->line_width / 2;

          gsk_path_builder_move_to (builder, s.x, s.y);
          add_cap (builder, &s, &e, stroke);
        }
    }
  else if (ops)
    {
      gsk_path_builder_close (builder);

      /* Start the left contour */
      op = last->data;
      if (op->angle[0] <= 179)
        gsk_path_builder_move_to (builder, op->le[1].x, op->le[1].y);
      else
        gsk_path_builder_move_to (builder, op->l[1].x, op->l[1].y);
    }

  /* Walk the ops backwards for the left edge */
  for (l = last; l; l = l->prev)
    {
      op = l->data;

      switch (op->op)
        {
        case GSK_PATH_MOVE:
          g_assert_not_reached ();
          break;

        case GSK_PATH_CLOSE:
          if (graphene_point_near (&op->pts[0], &op->pts[1], 0.0001))
            break;
          G_GNUC_FALLTHROUGH;

        case GSK_PATH_LINE:
          if (op->angle[0] <= 179)
            gsk_path_builder_line_to (builder, op->le[0].x, op->le[0].y);
          else
            gsk_path_builder_line_to (builder, op->l[0].x, op->l[0].y);
          break;

        case GSK_PATH_CURVE:
          if (op->angle[0] <= 179)
            gsk_path_builder_curve_to (builder, op->l[2].x, op->l[2].y,
                                                op->l[1].x, op->l[1].y,
                                                op->le[0].x, op->le[0].y);
          else
            gsk_path_builder_curve_to (builder, op->l[2].x, op->l[2].y,
                                                op->l[1].x, op->l[1].y,
                                                op->l[0].x, op->l[0].y);
          break;

        case GSK_PATH_CONIC:
          if (op->angle[0] <= 179)
            gsk_path_builder_conic_to (builder, op->l[1].x, op->l[1].y,
                                                op->le[0].x, op->le[0].y,
                                                op->w);
          else
            gsk_path_builder_conic_to (builder, op->l[1].x, op->l[1].y,
                                                op->l[0].x, op->l[0].y,
                                                op->w);
          break;

        default:
          g_assert_not_reached ();
        }

      if (l->prev || ((PathOpData *)last->data)->op == GSK_PATH_CLOSE)
        {
          op1 = l->prev ? l->prev->data : last->data;

          if (op->angle[0] >= 181)
            {
              /* Deal with joins */
              switch (stroke->line_join)
                {
                case GSK_LINE_JOIN_MITER:
                case GSK_LINE_JOIN_MITER_CLIP:
                  if (op->angle[0] != 0 && fabs (1 / sin (DEG_TO_RAD (op->angle[0]) / 2) <= stroke->miter_limit))
                    {
                      gsk_path_builder_line_to (builder, op->le[0].x, op->le[0].y);
                      gsk_path_builder_line_to (builder, op1->l[op1->n_pts - 1].x, op1->l[op1->n_pts - 1].y);
                    }
                  else if (stroke->line_join == GSK_LINE_JOIN_MITER_CLIP)
                    {
                      graphene_point_t p, q, p1, p2;
                      graphene_vec2_t t, n;

                      direction_vector (&op->pts[0], &op->le[0], &t);
                      scale_point (&op->pts[0], &t, stroke->line_width * stroke->miter_limit / 2, &p);

                      normal_vector (&op->pts[0], &op->le[0], &n);
                      scale_point (&p, &n, 1, &q);

                      line_intersection (&p, &q, &op->l[0], &op->le[0], &p1);
                      line_intersection (&p, &q, &op1->l[op1->n_pts - 1], &op->le[0], &p2);

                      gsk_path_builder_line_to (builder, p1.x, p1.y);
                      gsk_path_builder_line_to (builder, p2.x, p2.y);
                      gsk_path_builder_line_to (builder, op1->l[op1->n_pts - 1].x, op1->l[op1->n_pts - 1].y);
                    }
                  else
                    {
                      gsk_path_builder_line_to (builder, op1->l[op1->n_pts - 1].x, op1->l[op1->n_pts - 1].y);
                    }
                  break;

                case GSK_LINE_JOIN_BEVEL:
                  gsk_path_builder_line_to (builder, op1->l[op1->n_pts - 1].x, op1->l[op1->n_pts - 1].y);
                  break;

                case GSK_LINE_JOIN_ROUND:
                  gsk_path_builder_svg_arc_to (builder,
                                               stroke->line_width / 2, stroke->line_width / 2,
                                               0, 0, 0,
                                               op1->l[op1->n_pts - 1].x, op1->l[op1->n_pts - 1].y);
                  break;

                default:
                  g_assert_not_reached ();
                }
            }
        }
    }

  if (draw_caps)
    {
      /* Deal with the cap at the beginning */

      if (ops)
        {
          op = ops->data;
          add_cap (builder, &op->le[0], &op->re[0], stroke);
        }
      else
        {
          graphene_point_t s, e;

          /* an isolated point, we have it in start */
          e = s = *start;
          s.y -= stroke->line_width / 2;
          e.y += stroke->line_width / 2;
          add_cap (builder, &s, &e, stroke);
        }
    }

  gsk_path_builder_close (builder);

#ifdef STROKE_DEBUG
  emit_debug (builder, ops);
#endif
}

static gboolean
add_op_to_list (GskPathOperation        op,
                const graphene_point_t *pts,
                gsize                   n_pts,
                float                   weight,
                gpointer                user_data)
{
  AddOpData *data = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (data->has_start)
        {
          if (data->print) g_print ("==\n");
          data->ops = g_list_reverse (data->ops);
          stroke_ops (data->ops, &data->start, data->stroke, data->builder);
          g_list_free_full (data->ops, g_free);
          data->ops = NULL;
          data->has_start = FALSE;
        }
      if (data->print) g_print ("M %f %f\n", pts[0].x, pts[0].y);
      data->start = pts[0];
      data->has_start = TRUE;
      break;

    case GSK_PATH_CLOSE:
      if (data->print) g_print ("Z\n");
      data->ops = g_list_prepend (data->ops, path_op_data_new (op, pts, weight, n_pts));
      data->ops = g_list_reverse (data->ops);
      if (data->print) g_print ("==\n");
      stroke_ops (data->ops, &data->start, data->stroke, data->builder);
      g_list_free_full (data->ops, g_free);
      data->ops = NULL;
      data->has_start = FALSE;
      break;

    case GSK_PATH_LINE:
      if (data->print) g_print ("L %f %f\n", pts[1].x, pts[1].y);
      data->ops = g_list_prepend (data->ops, path_op_data_new (op, pts, weight, n_pts));
      break;

    case GSK_PATH_CURVE:
      if (data->print) g_print ("C %f %f, %f %f, %f %f\n",
                                pts[1].x, pts[1].y,
                                pts[2].x, pts[2].y,
                                pts[3].x, pts[3].y);
      subdivide_and_add (pts, data, 2);
      break;

    case GSK_PATH_CONIC:
      if (data->print) g_print ("O %f %f, %f %f, %f\n",
                                pts[1].x, pts[1].y,
                                pts[3].x, pts[3].y,
                                pts[2].x);
      subdivide_and_add_conic (pts, weight, data);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void
gsk_contour_default_add_stroke (const GskContour *contour,
                                GskPathBuilder   *builder,
                                GskStroke        *stroke)
{
  AddOpData data;

  data.builder = builder;
  data.stroke = stroke;
  data.ops = NULL;
  data.has_start = FALSE;
  data.print = FALSE;

  if (stroke->dash_length <= 0)
    gsk_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, add_op_to_list, &data);
  else
    {
      data.print = TRUE;
      g_print ("====\n");
      gsk_contour_dash (contour, stroke, GSK_PATH_TOLERANCE_DEFAULT, add_op_to_list, &data);
    }

  if (data.has_start)
    {
      data.ops = g_list_reverse (data.ops);
      if (data.print) g_print ("==\n");
      stroke_ops (data.ops, &data.start, stroke, builder);
      g_list_free_full (data.ops, g_free);
    }
}
