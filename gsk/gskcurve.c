/*
 * Copyright © 2020 Benjamin Otte
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
 */

#include "config.h"

#include "gskcurveprivate.h"

#define MIN_PROGRESS (1/1024.f)

typedef struct _GskCurveClass GskCurveClass;

struct _GskCurveClass
{
  void                          (* init)                (GskCurve               *curve,
                                                         gskpathop               op);
  void                          (* init_foreach)        (GskCurve               *curve,
                                                         GskPathOperation        op,
                                                         const graphene_point_t *pts,
                                                         gsize                   n_pts,
                                                         float                   weight);
  void                          (* get_point)           (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_point_t       *pos);
  void                          (* get_tangent)         (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_vec2_t        *tangent);
  void                          (* split)               (const GskCurve         *curve,
                                                         float                   progress,
                                                         GskCurve               *result1,
                                                         GskCurve               *result2);
  void                          (* segment)             (const GskCurve         *curve,
                                                         float                   start,
                                                         float                   end,
                                                         GskCurve               *segment);
  gboolean                      (* decompose)           (const GskCurve         *curve,
                                                         float                   tolerance,
                                                         GskCurveAddLineFunc     add_line_func,
                                                         gpointer                user_data);
  gboolean                      (* decompose_curve)     (const GskCurve         *curve,
                                                         float                   tolerance,
                                                         GskCurveAddCurveFunc    add_curve_func,
                                                         gpointer                user_data);
  gskpathop                     (* pathop)              (const GskCurve         *curve);
  const graphene_point_t *      (* get_start_point)     (const GskCurve         *curve);
  const graphene_point_t *      (* get_end_point)       (const GskCurve         *curve);
  void                          (* get_start_tangent)   (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_end_tangent)     (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_bounds)          (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
  void                          (* get_tight_bounds)    (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
  void                          (* offset)              (const GskCurve         *curve,
                                                         float                   distance,
                                                         GskCurve               *offset_curve);
  void                          (* reverse)             (const GskCurve         *curve,
                                                         GskCurve               *reverse);
  float                         (* get_curvature)       (const GskCurve         *curve,
                                                         float                   t);
};

/* {{{ Line implementation */

static void
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static void
get_normal (const graphene_point_t *p0,
            const graphene_point_t *p1,
            graphene_vec2_t        *n)
{
  graphene_vec2_init (n, p0->y - p1->y, p1->x - p0->x);
  graphene_vec2_normalize (n, n);
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

/* Set p to the intersection of the lines through a, b and c, d.
 * Return the number of intersections found (0 or 1)
 */
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

  if (fabs (det) < 0.001)
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
gsk_line_curve_init_from_points (GskLineCurve           *self,
                                 GskPathOperation        op,
                                 const graphene_point_t *start,
                                 const graphene_point_t *end)
{
  self->op = op;
  self->points[0] = *start;
  self->points[1] = *end;
}

static void
gsk_line_curve_init (GskCurve  *curve,
                     gskpathop  op)
{
  GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = gsk_pathop_points (op);

  gsk_line_curve_init_from_points (self, gsk_pathop_op (op), &pts[0], &pts[1]);
}

static void
gsk_line_curve_init_foreach (GskCurve               *curve,
                             GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts,
                             float                   weight)
{
  GskLineCurve *self = &curve->line;

  g_assert (n_pts == 2);

  gsk_line_curve_init_from_points (self, op, &pts[0], &pts[1]);
}

static void
gsk_line_curve_get_point (const GskCurve   *curve,
                          float             t,
                          graphene_point_t *pos)
{
  const GskLineCurve *self = &curve->line;

  graphene_point_interpolate (&self->points[0], &self->points[1], t, pos);
}

static void
gsk_line_curve_get_tangent (const GskCurve  *curve,
                            float            t,
                            graphene_vec2_t *tangent)
{
  const GskLineCurve *self = &curve->line;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_line_curve_split (const GskCurve   *curve,
                      float             progress,
                      GskCurve         *start,
                      GskCurve         *end)
{
  const GskLineCurve *self = &curve->line;
  graphene_point_t point;

  graphene_point_interpolate (&self->points[0], &self->points[1], progress, &point);

  if (start)
    gsk_line_curve_init_from_points (&start->line, GSK_PATH_LINE, &self->points[0], &point);
  if (end)
    gsk_line_curve_init_from_points (&end->line, GSK_PATH_LINE, &point, &self->points[1]);
}

static void
gsk_line_curve_segment (const GskCurve *curve,
                        float           start,
                        float           end,
                        GskCurve       *segment)
{
  const GskLineCurve *self = &curve->line;
  graphene_point_t start_point, end_point;

  graphene_point_interpolate (&self->points[0], &self->points[1], start, &start_point);
  graphene_point_interpolate (&self->points[0], &self->points[1], end, &end_point);

  gsk_line_curve_init_from_points (&segment->line, GSK_PATH_LINE, &start_point, &end_point);
}

static gboolean
gsk_line_curve_decompose (const GskCurve      *curve,
                          float                tolerance,
                          GskCurveAddLineFunc  add_line_func,
                          gpointer             user_data)
{
  const GskLineCurve *self = &curve->line;

  return add_line_func (&self->points[0], &self->points[1], 0.0f, 1.0f, user_data);
}

static gboolean
gsk_line_curve_decompose_curve (const GskCurve       *curve,
                                float                 tolerance,
                                GskCurveAddCurveFunc  add_curve_func,
                                gpointer              user_data)
{
  const GskLineCurve *self = &curve->line;
  graphene_point_t p[4];

  p[0] = self->points[0];
  p[3] = self->points[1];
  graphene_point_interpolate (&p[0], &p[3], 1/3.0, &p[1]);
  graphene_point_interpolate (&p[0], &p[3], 2/3.0, &p[2]);

  return add_curve_func (p, user_data);
}

static gskpathop
gsk_line_curve_pathop (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_line_curve_get_start_point (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return &self->points[0];
}

static const graphene_point_t *
gsk_line_curve_get_end_point (const GskCurve *curve)
{
  const GskLineCurve *self = &curve->line;

  return &self->points[1];
}

static void
gsk_line_curve_get_start_end_tangent (const GskCurve  *curve,
                                      graphene_vec2_t *tangent)
{
  const GskLineCurve *self = &curve->line;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_line_curve_get_bounds (const GskCurve  *curve,
                           GskBoundingBox  *bounds)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[1]);
}

static void
gsk_line_curve_offset (const GskCurve *curve,
                       float           distance,
                       GskCurve       *offset_curve)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;
  graphene_vec2_t n;
  graphene_point_t p[4];

  get_normal (&pts[0], &pts[1], &n);
  scale_point (&pts[0], &n, distance, &p[0]);
  scale_point (&pts[1], &n, distance, &p[1]);

  gsk_curve_init (offset_curve, gsk_pathop_encode (GSK_PATH_LINE, p));
}

static void
gsk_line_curve_reverse (const GskCurve *curve,
                        GskCurve       *reverse)
{
  const GskLineCurve *self = &curve->line;

  reverse->op = GSK_PATH_LINE;
  reverse->line.points[0] = self->points[1];
  reverse->line.points[1] = self->points[0];
}

static float
gsk_line_curve_get_curvature (const GskCurve *curve,
                              float           t)
{
  return 0;
}

static const GskCurveClass GSK_LINE_CURVE_CLASS = {
  gsk_line_curve_init,
  gsk_line_curve_init_foreach,
  gsk_line_curve_get_point,
  gsk_line_curve_get_tangent,
  gsk_line_curve_split,
  gsk_line_curve_segment,
  gsk_line_curve_decompose,
  gsk_line_curve_decompose_curve,
  gsk_line_curve_pathop,
  gsk_line_curve_get_start_point,
  gsk_line_curve_get_end_point,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_bounds,
  gsk_line_curve_get_bounds,
  gsk_line_curve_offset,
  gsk_line_curve_reverse,
  gsk_line_curve_get_curvature
};

/* }}} */
/* {{{ Curve implementation */

static void
gsk_curve_curve_init_from_points (GskCurveCurve          *self,
                                  const graphene_point_t  pts[4])
{
  self->op = GSK_PATH_CURVE;
  self->has_coefficients = FALSE;
  memcpy (self->points, pts, sizeof (graphene_point_t) * 4);
}

static void
gsk_curve_curve_init (GskCurve  *curve,
                      gskpathop  op)
{
  GskCurveCurve *self = &curve->curve;

  gsk_curve_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_curve_curve_init_foreach (GskCurve               *curve,
                              GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts,
                              float                   weight)
{
  GskCurveCurve *self = &curve->curve;

  g_assert (n_pts == 4);

  gsk_curve_curve_init_from_points (self, pts);
}

static void
gsk_curve_curve_ensure_coefficients (const GskCurveCurve *curve)
{
  GskCurveCurve *self = (GskCurveCurve *) curve;
  const graphene_point_t *pts = &self->points[0];

  if (self->has_coefficients)
    return;

  self->coeffs[0] = GRAPHENE_POINT_INIT (pts[3].x - 3.0f * pts[2].x + 3.0f * pts[1].x - pts[0].x,
                                         pts[3].y - 3.0f * pts[2].y + 3.0f * pts[1].y - pts[0].y);
  self->coeffs[1] = GRAPHENE_POINT_INIT (3.0f * pts[2].x - 6.0f * pts[1].x + 3.0f * pts[0].x,
                                         3.0f * pts[2].y - 6.0f * pts[1].y + 3.0f * pts[0].y);
  self->coeffs[2] = GRAPHENE_POINT_INIT (3.0f * pts[1].x - 3.0f * pts[0].x,
                                         3.0f * pts[1].y - 3.0f * pts[0].y);
  self->coeffs[3] = pts[0];

  self->has_coefficients = TRUE;
}

static void
gsk_curve_curve_get_point (const GskCurve   *curve,
                           float             t,
                           graphene_point_t *pos)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *c = self->coeffs;

  gsk_curve_curve_ensure_coefficients (self);

  *pos = GRAPHENE_POINT_INIT (((c[0].x * t + c[1].x) * t +c[2].x) * t + c[3].x,
                              ((c[0].y * t + c[1].y) * t +c[2].y) * t + c[3].y);
}

static void
gsk_curve_curve_get_tangent (const GskCurve   *curve,
                             float             t,
                             graphene_vec2_t  *tangent)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *c = self->coeffs;

  gsk_curve_curve_ensure_coefficients (self);

  graphene_vec2_init (tangent,
                      (3.0f * c[0].x * t + 2.0f * c[1].x) * t + c[2].x,
                      (3.0f * c[0].y * t + 2.0f * c[1].y) * t + c[2].y);
  graphene_vec2_normalize (tangent, tangent);
}

static void
gsk_curve_curve_split (const GskCurve   *curve,
                       float             progress,
                       GskCurve         *start,
                       GskCurve         *end)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;
  graphene_point_t ab, bc, cd;
  graphene_point_t abbc, bccd;
  graphene_point_t final;

  graphene_point_interpolate (&pts[0], &pts[1], progress, &ab);
  graphene_point_interpolate (&pts[1], &pts[2], progress, &bc);
  graphene_point_interpolate (&pts[2], &pts[3], progress, &cd);
  graphene_point_interpolate (&ab, &bc, progress, &abbc);
  graphene_point_interpolate (&bc, &cd, progress, &bccd);
  graphene_point_interpolate (&abbc, &bccd, progress, &final);

  if (start)
    gsk_curve_curve_init_from_points (&start->curve, (graphene_point_t[4]) { pts[0], ab, abbc, final });
  if (end)
    gsk_curve_curve_init_from_points (&end->curve, (graphene_point_t[4]) { final, bccd, cd, pts[3] });
}

static void
gsk_curve_curve_segment (const GskCurve *curve,
                         float           start,
                         float           end,
                         GskCurve       *segment)
{
  GskCurve tmp;

  gsk_curve_curve_split (curve, start, NULL, &tmp);
  gsk_curve_curve_split (&tmp, (end - start) / (1.0f - start), segment, NULL);
}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_curve_curve_too_curvy (const GskCurveCurve *self,
                           float                tolerance)
{
  const graphene_point_t *pts = self->points;
  graphene_point_t p;

  graphene_point_interpolate (&pts[0], &pts[3], 1.0f / 3, &p);
  if (ABS (p.x - pts[1].x) + ABS (p.y - pts[1].y)  > tolerance)
    return TRUE;

  graphene_point_interpolate (&pts[0], &pts[3], 2.0f / 3, &p);
  if (ABS (p.x - pts[2].x) + ABS (p.y - pts[2].y)  > tolerance)
    return TRUE;

  return FALSE;
}

static gboolean
gsk_curce_curve_decompose_step (const GskCurve      *curve,
                                float                start_progress,
                                float                end_progress,
                                float                tolerance,
                                GskCurveAddLineFunc  add_line_func,
                                gpointer             user_data)
{
  const GskCurveCurve *self = &curve->curve;
  GskCurve left, right;
  float mid_progress;

  if (!gsk_curve_curve_too_curvy (self, tolerance) || end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (&self->points[0], &self->points[3], start_progress, end_progress, user_data);

  gsk_curve_curve_split ((const GskCurve *) self, 0.5, &left, &right);
  mid_progress = (start_progress + end_progress) / 2;

  return gsk_curce_curve_decompose_step (&left, start_progress, mid_progress, tolerance, add_line_func, user_data)
      && gsk_curce_curve_decompose_step (&right, mid_progress, end_progress, tolerance, add_line_func, user_data);
}

static gboolean
gsk_curve_curve_decompose (const GskCurve      *curve,
                           float                tolerance,
                           GskCurveAddLineFunc  add_line_func,
                           gpointer             user_data)
{
  return gsk_curce_curve_decompose_step (curve, 0.0, 1.0, tolerance, add_line_func, user_data);
}

static gboolean
gsk_curve_curve_decompose_curve (const GskCurve       *curve,
                                 float                 tolerance,
                                 GskCurveAddCurveFunc  add_curve_func,
                                 gpointer              user_data)
{
  const GskCurveCurve *self = &curve->curve;

  return add_curve_func (self->points, user_data);
}

static gskpathop
gsk_curve_curve_pathop (const GskCurve *curve)
{
  const GskCurveCurve *self = &curve->curve;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_curve_curve_get_start_point (const GskCurve *curve)
{
  const GskCurveCurve *self = &curve->curve;

  return &self->points[0];
}

static const graphene_point_t *
gsk_curve_curve_get_end_point (const GskCurve *curve)
{
  const GskCurveCurve *self = &curve->curve;

  return &self->points[3];
}

static void
gsk_curve_curve_get_start_tangent (const GskCurve  *curve,
                                   graphene_vec2_t *tangent)
{
  const GskCurveCurve *self = &curve->curve;

  if (graphene_point_near (&self->points[0], &self->points[1], 0.0001))
    {
      if (graphene_point_near (&self->points[0], &self->points[2], 0.0001))
        get_tangent (&self->points[0], &self->points[3], tangent);
      else
        get_tangent (&self->points[0], &self->points[2], tangent);
    }
  else
    get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_curve_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskCurveCurve *self = &curve->curve;

  if (graphene_point_near (&self->points[2], &self->points[3], 0.0001))
    {
      if (graphene_point_near (&self->points[1], &self->points[3], 0.0001))
        get_tangent (&self->points[0], &self->points[3], tangent);
      else
        get_tangent (&self->points[1], &self->points[3], tangent);
    }
  else
    get_tangent (&self->points[2], &self->points[3], tangent);
}

static void
gsk_curve_curve_get_bounds (const GskCurve  *curve,
                            GskBoundingBox  *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);
  gsk_bounding_box_expand (bounds, &pts[1]);
  gsk_bounding_box_expand (bounds, &pts[2]);
}

static inline gboolean
acceptable (float t)
{
  return 0 <= t && t <= 1;
}

/* Solve P' = 0 where P is
 * P = (1-t)^3*pa + 3*t*(1-t)^2*pb + 3*t^2*(1-t)*pc + t^3*pd
 */
static int
get_cubic_extrema (float pa, float pb, float pc, float pd, float t[2])
{
  float a, b, c;
  float d, tt;
  int n = 0;

  a = 3 * (pd - 3*pc + 3*pb - pa);
  b = 6 * (pc - 2*pb + pa);
  c = 3 * (pb - pa);

  if (fabs (a) > 0.0001)
    {
      if (b*b > 4*a*c)
        {
          d = sqrt (b*b - 4*a*c);
          tt = (-b + d)/(2*a);
          if (acceptable (tt))
            t[n++] = tt;
          tt = (-b - d)/(2*a);
          if (acceptable (tt))
            t[n++] = tt;
        }
      else
        {
          tt = -b / (2*a);
          if (acceptable (tt))
            t[n++] = tt;
        }
    }
  else if (fabs (b) > 0.0001)
    {
      tt = -c / b;
      if (acceptable (tt))
        t[n++] = tt;
    }

  return n;
}

static void
gsk_curve_curve_get_tight_bounds (const GskCurve  *curve,
                                  GskBoundingBox  *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;
  float t[4];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);

  n = 0;
  n += get_cubic_extrema (pts[0].x, pts[1].x, pts[2].x, pts[3].x, &t[n]);
  n += get_cubic_extrema (pts[0].y, pts[1].y, pts[2].y, pts[3].y, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_curve_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static void
gsk_curve_curve_offset (const GskCurve *curve,
                        float           distance,
                        GskCurve       *offset)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;
  graphene_vec2_t n;
  graphene_point_t p[4];
  graphene_point_t m1, m2, m3, m4;
  int coinc;

  coinc = (graphene_point_near (&pts[0], &pts[1], 0.001) << 0) |
          (graphene_point_near (&pts[1], &pts[2], 0.001) << 1) |
          (graphene_point_near (&pts[2], &pts[3], 0.001) << 2);

  if (coinc == 7)
    {
      /* just give up */
      p[0] = pts[0];
      p[1] = pts[1];
      p[2] = pts[2];
      p[3] = pts[3];
    }
  else if (coinc == 3 || coinc == 5 || coinc == 6)
    {
      /* a straight line */
      get_normal (&pts[0], &pts[3], &n);
      scale_point (&pts[0], &n, distance, &p[0]);
      scale_point (&pts[3], &n, distance, &p[3]);

      if (coinc & (1 << 0))
        p[1] = p[0];
      else
        p[1] = p[3];
      if (coinc & (1 << 2))
        p[2] = p[3];
      else
        p[2] = p[0];
    }
  else if (coinc == 1 || coinc == 2 || coinc == 4)
    {
      graphene_point_t p1;

      if (coinc == 1)
        p1 = pts[2];
      else
        p1 = pts[1];

      get_normal (&pts[0], &p1, &n);
      scale_point (&pts[0], &n, distance, &p[0]);
      scale_point (&p1, &n, distance, &m1);

      get_normal (&p1, &pts[3], &n);
      scale_point (&p1, &n, distance, &m2);
      scale_point (&pts[3], &n, distance, &p[3]);

      if (!line_intersection (&p[0], &m1, &m2, &p[3], &m3))
        m3 = m1;

      if (coinc == 1)
        {
          p[1] = p[0];
          p[2] = m3;
        }
      else if (coinc == 2)
        {
          p[1] = m3;
          p[2] = m3;
        }
      else
        {
          p[1] = m3;
          p[2] = p[3];
        }
    }
  else if (coinc == 0)
    {
      get_normal (&pts[0], &pts[1], &n);
      scale_point (&pts[0], &n, distance, &p[0]);
      scale_point (&pts[1], &n, distance, &m1);

      get_normal (&pts[1], &pts[2], &n);
      scale_point (&pts[1], &n, distance, &m2);
      scale_point (&pts[2], &n, distance, &m3);

      get_normal (&pts[2], &pts[3], &n);
      scale_point (&pts[2], &n, distance, &m4);
      scale_point (&pts[3], &n, distance, &p[3]);

      if (!line_intersection (&p[0], &m1, &m2, &m3, &p[1]))
        p[1] = m1;

      if (!line_intersection (&m2, &m3, &m4, &p[3], &p[2]))
        p[2] = m4;
    }

  gsk_curve_curve_init_from_points (&offset->curve, p);
}

static void
gsk_curve_curve_reverse (const GskCurve *curve,
                         GskCurve       *reverse)
{
  const GskCurveCurve *self = &curve->curve;

  reverse->op = GSK_PATH_CURVE;
  reverse->curve.points[0] = self->points[3];
  reverse->curve.points[1] = self->points[2];
  reverse->curve.points[2] = self->points[1];
  reverse->curve.points[3] = self->points[0];
  reverse->curve.has_coefficients = FALSE;
}

static void
get_derivatives (const GskCurve  *curve,
                float            t,
                graphene_vec2_t *d,
                graphene_vec2_t *dd)
{
  const graphene_point_t *p = curve->curve.points;
  graphene_point_t dp[3];
  graphene_point_t ddp[2];
  float t1, t2, t3;

  dp[0].x = 3 * (p[1].x - p[0].x);
  dp[0].y = 3 * (p[1].y - p[0].y);
  dp[1].x = 3 * (p[2].x - p[1].x);
  dp[1].y = 3 * (p[2].y - p[1].y);
  dp[2].x = 3 * (p[3].x - p[2].x);
  dp[2].y = 3 * (p[3].y - p[2].y);

  if (d)
    {
      t1 = (1 - t) * (1 - t);
      t2 = t * (1 - t);
      t3 = t * t;

      graphene_vec2_init (d,
                          dp[0].x * t1 + 2 * dp[1].x * t2 + dp[2].x * t3,
                          dp[0].y * t1 + 2 * dp[1].y * t2 + dp[2].y * t3);
    }

  if (dd)
    {
      ddp[0].x = 2 * (dp[1].x - dp[0].x);
      ddp[0].y = 2 * (dp[1].y - dp[0].y);
      ddp[1].x = 2 * (dp[2].x - dp[1].x);
      ddp[1].y = 2 * (dp[2].y - dp[1].y);

      graphene_vec2_init (dd,
                          ddp[0].x * (1 - t) + ddp[1].x * t,
                          ddp[0].y * (1 - t) + ddp[1].y * t);
    }
}

static float
cross (const graphene_vec2_t *t1,
       const graphene_vec2_t *t2)
{
  return graphene_vec2_get_x (t1)*graphene_vec2_get_y (t2)
        - graphene_vec2_get_y (t1)*graphene_vec2_get_x (t2);
}

static float
pow3 (float w)
{
  return w*w*w;
}

static float
gsk_curve_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  graphene_vec2_t d, dd;
  float num, denom;

  get_derivatives (curve, t, &d, &dd);

  num = cross (&d, &dd);
  if (num == 0)
    return 0;

  denom = pow3 (graphene_vec2_length (&d));
  if (denom == 0)
    return 0;

  return num / denom;
}

static const GskCurveClass GSK_CURVE_CURVE_CLASS = {
  gsk_curve_curve_init,
  gsk_curve_curve_init_foreach,
  gsk_curve_curve_get_point,
  gsk_curve_curve_get_tangent,
  gsk_curve_curve_split,
  gsk_curve_curve_segment,
  gsk_curve_curve_decompose,
  gsk_curve_curve_decompose_curve,
  gsk_curve_curve_pathop,
  gsk_curve_curve_get_start_point,
  gsk_curve_curve_get_end_point,
  gsk_curve_curve_get_start_tangent,
  gsk_curve_curve_get_end_tangent,
  gsk_curve_curve_get_bounds,
  gsk_curve_curve_get_tight_bounds,
  gsk_curve_curve_offset,
  gsk_curve_curve_reverse,
  gsk_curve_curve_get_curvature,
};

/* }}} */
/* {{{ Conic implementation */

static void
gsk_conic_curve_init_from_points (GskConicCurve          *self,
                                  const graphene_point_t  pts[4])
{
  self->op = GSK_PATH_CONIC;
  self->has_coefficients = FALSE;

  memcpy (self->points, pts, sizeof (graphene_point_t) * 4);
}

static void
gsk_conic_curve_init (GskCurve  *curve,
                      gskpathop  op)
{
  GskConicCurve *self = &curve->conic;

  gsk_conic_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_conic_curve_init_foreach (GskCurve               *curve,
                              GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts,
                              float                   weight)
{
  GskConicCurve *self = &curve->conic;

  g_assert (n_pts == 3);

  gsk_conic_curve_init_from_points (self,
                                    (graphene_point_t[4]) {
                                      pts[0],
                                      pts[1],
                                      GRAPHENE_POINT_INIT (weight, 0),
                                      pts[2]
                                    });
}

static inline float
gsk_conic_curve_get_weight (const GskConicCurve *self)
{
  return self->points[2].x;
}

static void
gsk_conic_curve_ensure_coefficents (const GskConicCurve *curve)
{
  GskConicCurve *self = (GskConicCurve *) curve;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  graphene_point_t pw = GRAPHENE_POINT_INIT (w * pts[1].x, w * pts[1].y);

  if (self->has_coefficients)
    return;

  self->num[2] = pts[0];
  self->num[1] = GRAPHENE_POINT_INIT (2 * (pw.x - pts[0].x),
                                      2 * (pw.y - pts[0].y));
  self->num[0] = GRAPHENE_POINT_INIT (pts[3].x - 2 * pw.x + pts[0].x,
                                      pts[3].y - 2 * pw.y + pts[0].y);

  self->denom[2] = GRAPHENE_POINT_INIT (1, 1);
  self->denom[1] = GRAPHENE_POINT_INIT (2 * (w - 1), 2 * (w - 1));
  self->denom[0] = GRAPHENE_POINT_INIT (-self->denom[1].x, -self->denom[1].y);

  self->has_coefficients = TRUE;
}

static inline void
gsk_curve_eval_quad (const graphene_point_t quad[3],
                     float                  progress,
                     graphene_point_t      *result)
{
  *result = GRAPHENE_POINT_INIT ((quad[0].x * progress + quad[1].x) * progress + quad[2].x,
                                 (quad[0].y * progress + quad[1].y) * progress + quad[2].y);
}

static inline void
gsk_conic_curve_eval_point (const GskConicCurve *self,
                            float                progress,
                            graphene_point_t    *point)
{
  graphene_point_t num, denom;

  gsk_curve_eval_quad (self->num, progress, &num);
  gsk_curve_eval_quad (self->denom, progress, &denom);

  *point = GRAPHENE_POINT_INIT (num.x / denom.x, num.y / denom.y);
}

static void
gsk_conic_curve_get_point (const GskCurve   *curve,
                           float             t,
                           graphene_point_t *pos)
{
  const GskConicCurve *self = &curve->conic;

  gsk_conic_curve_ensure_coefficents (self);

  gsk_conic_curve_eval_point (self, t, pos);
}

static void
gsk_conic_curve_get_tangent (const GskCurve   *curve,
                             float             t,
                             graphene_vec2_t  *tangent)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t tmp;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;

  /* The tangent will be 0 in these corner cases, just
   * treat it like a line here. */
  if ((t <= 0.f && graphene_point_equal (&pts[0], &pts[1])) ||
      (t >= 1.f && graphene_point_equal (&pts[1], &pts[3])))
    {
      graphene_vec2_init (tangent, pts[3].x - pts[0].x, pts[3].y - pts[0].y);
      return;
    }

  gsk_curve_eval_quad ((graphene_point_t[3]) {
                         GRAPHENE_POINT_INIT ((w - 1) * (pts[3].x - pts[0].x),
                                              (w - 1) * (pts[3].y - pts[0].y)),
                         GRAPHENE_POINT_INIT (pts[3].x - pts[0].x - 2 * w * (pts[1].x - pts[0].x),
                                              pts[3].y - pts[0].y - 2 * w * (pts[1].y - pts[0].y)),
                         GRAPHENE_POINT_INIT (w * (pts[1].x - pts[0].x),
                                              w * (pts[1].y - pts[0].y))
                       },
                       t,
                       &tmp);
  graphene_vec2_init (tangent, tmp.x, tmp.y);
  graphene_vec2_normalize (tangent, tangent);
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

static void
gsk_conic_curve_split (const GskCurve   *curve,
                       float             progress,
                       GskCurve         *start,
                       GskCurve         *end)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point3d_t p[3];
  graphene_point3d_t l[3], r[3];
  graphene_point_t left[4], right[4];
  float w;

  /* do de Casteljau in homogeneous coordinates... */
  w = self->points[2].x;
  p[0] = GRAPHENE_POINT3D_INIT (self->points[0].x, self->points[0].y, 1);
  p[1] = GRAPHENE_POINT3D_INIT (self->points[1].x * w, self->points[1].y * w, w);
  p[2] = GRAPHENE_POINT3D_INIT (self->points[3].x, self->points[3].y, 1);

  split_bezier3d (p, 3, progress, l, r);

  /* then project the control points down */
  left[0] = GRAPHENE_POINT_INIT (l[0].x / l[0].z, l[0].y / l[0].z);
  left[1] = GRAPHENE_POINT_INIT (l[1].x / l[1].z, l[1].y / l[1].z);
  left[3] = GRAPHENE_POINT_INIT (l[2].x / l[2].z, l[2].y / l[2].z);

  right[0] = GRAPHENE_POINT_INIT (r[0].x / r[0].z, r[0].y / r[0].z);
  right[1] = GRAPHENE_POINT_INIT (r[1].x / r[1].z, r[1].y / r[1].z);
  right[3] = GRAPHENE_POINT_INIT (r[2].x / r[2].z, r[2].y / r[2].z);

  /* normalize the outer weights to be 1 by using
   * the fact that weights w_i and c*w_i are equivalent
   * for any nonzero constant c
   */
  for (int i = 0; i < 3; i++)
    {
      l[i].z /= l[0].z;
      r[i].z /= r[2].z;
    }

  /* normalize the inner weight to be 1 by using
   * the fact that w_0*w_2/w_1^2 is a constant for
   * all equivalent weights.
   */
  left[2] = GRAPHENE_POINT_INIT (l[1].z / sqrt (l[2].z), 0);
  right[2] = GRAPHENE_POINT_INIT (r[1].z / sqrt (r[0].z), 0);

  if (start)
    gsk_curve_init (start, gsk_pathop_encode (GSK_PATH_CONIC, left));
  if (end)
    gsk_curve_init (end, gsk_pathop_encode (GSK_PATH_CONIC, right));
}

static void
gsk_conic_curve_segment (const GskCurve *curve,
                         float           start,
                         float           end,
                         GskCurve       *segment)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t start_num, start_denom;
  graphene_point_t mid_num, mid_denom;
  graphene_point_t end_num, end_denom;
  graphene_point_t ctrl_num, ctrl_denom;
  float mid;

  if (start <= 0.0f)
    return gsk_conic_curve_split (curve, end, segment, NULL);
  else if (end >= 1.0f)
    return gsk_conic_curve_split (curve, start, NULL, segment);

  gsk_conic_curve_ensure_coefficents (self);

  gsk_curve_eval_quad (self->num, start, &start_num);
  gsk_curve_eval_quad (self->denom, start, &start_denom);
  mid = (start + end) / 2;
  gsk_curve_eval_quad (self->num, mid, &mid_num);
  gsk_curve_eval_quad (self->denom, mid, &mid_denom);
  gsk_curve_eval_quad (self->num, end, &end_num);
  gsk_curve_eval_quad (self->denom, end, &end_denom);
  ctrl_num = GRAPHENE_POINT_INIT (2 * mid_num.x - (start_num.x + end_num.x) / 2,
                                  2 * mid_num.y - (start_num.y + end_num.y) / 2);
  ctrl_denom = GRAPHENE_POINT_INIT (2 * mid_denom.x - (start_denom.x + end_denom.x) / 2,
                                    2 * mid_denom.y - (start_denom.y + end_denom.y) / 2);

  gsk_conic_curve_init_from_points (&segment->conic,
                                    (graphene_point_t[4]) {
                                      GRAPHENE_POINT_INIT (start_num.x / start_denom.x,
                                                           start_num.y / start_denom.y),
                                      GRAPHENE_POINT_INIT (ctrl_num.x / ctrl_denom.x,
                                                           ctrl_num.y / ctrl_denom.y),
                                      GRAPHENE_POINT_INIT (ctrl_denom.x / sqrtf (start_denom.x * end_denom.x),
                                                           0),
                                      GRAPHENE_POINT_INIT (end_num.x / end_denom.x,
                                                           end_num.y / end_denom.y)
                                    });

}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_conic_curve_too_curvy (const graphene_point_t *start,
                           const graphene_point_t *mid,
                           const graphene_point_t *end,
                           float                   tolerance)
{
  return fabs ((start->x + end->x) * 0.5 - mid->x) > tolerance
      || fabs ((start->y + end->y) * 0.5 - mid->y) > tolerance;
}

static gboolean
gsk_conic_curve_decompose_subdivide (const GskConicCurve    *self,
                                     float                   tolerance,
                                     const graphene_point_t *start,
                                     float                   start_progress,
                                     const graphene_point_t *end,
                                     float                   end_progress,
                                     GskCurveAddLineFunc     add_line_func,
                                     gpointer                user_data)
{
  graphene_point_t mid;
  float mid_progress;

  mid_progress = (start_progress + end_progress) / 2;
  gsk_conic_curve_eval_point (self, mid_progress, &mid);

  if (end_progress - start_progress <= MIN_PROGRESS ||
      !gsk_conic_curve_too_curvy (start, &mid, end, tolerance))
    {
      return add_line_func (start, end, start_progress, end_progress, user_data);
    }

  return gsk_conic_curve_decompose_subdivide (self, tolerance,
                                              start, start_progress, &mid, mid_progress,
                                              add_line_func, user_data)
      && gsk_conic_curve_decompose_subdivide (self, tolerance,
                                              &mid, mid_progress, end, end_progress,
                                              add_line_func, user_data);
}

static gboolean
gsk_conic_curve_decompose (const GskCurve      *curve,
                           float                tolerance,
                           GskCurveAddLineFunc  add_line_func,
                           gpointer             user_data)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t mid;

  gsk_conic_curve_ensure_coefficents (self);

  gsk_conic_curve_eval_point (self, 0.5, &mid);

  return gsk_conic_curve_decompose_subdivide (self,
                                              tolerance,
                                              &self->points[0],
                                              0.0f,
                                              &mid,
                                              0.5f,
                                              add_line_func,
                                              user_data)
      && gsk_conic_curve_decompose_subdivide (self,
                                              tolerance,
                                              &mid,
                                              0.5f,
                                              &self->points[3],
                                              1.0f,
                                              add_line_func,
                                              user_data);
}

/* See Floater, M: An analysis of cubic approximation schemes
 * for conic sections
 */
static void
cubic_approximation (const GskCurve *curve,
                     GskCurve       *cubic)
{
  const GskConicCurve *self = &curve->conic;
  graphene_point_t p[4];
  float w = self->points[2].x;
  float w2 = w*w;
  float lambda;

  lambda = 2 * (6*w2 + 1 - sqrt (3*w2 + 1)) / (12*w2 + 3);

  p[0] = self->points[0];
  p[3] = self->points[3];
  graphene_point_interpolate (&self->points[0], &self->points[1], lambda, &p[1]);
  graphene_point_interpolate (&self->points[3], &self->points[1], lambda, &p[2]);

  gsk_curve_init (cubic, gsk_pathop_encode (GSK_PATH_CURVE, p));
}

static gboolean
gsk_conic_is_close_to_cubic (const GskCurve *conic,
                             const GskCurve *cubic,
                             float           tolerance)
{
  float t[] = { 0.1, 0.5, 0.9 };
  graphene_point_t p0, p1;

  for (int i = 0; i < G_N_ELEMENTS (t); i++)
    {
      gsk_curve_get_point (conic, t[i], &p0);
      gsk_curve_get_point (cubic, t[i], &p1);
      if (graphene_point_distance (&p0, &p1, NULL, NULL) > tolerance)
        return FALSE;
    }

  return TRUE;
}

static gboolean gsk_conic_curve_decompose_curve (const GskCurve       *curve,
                                                 float                 tolerance,
                                                 GskCurveAddCurveFunc  add_curve_func,
                                                 gpointer              user_data);

static gboolean
gsk_conic_curve_decompose_or_add (const GskCurve       *curve,
                                  const GskCurve       *cubic,
                                  float                 tolerance,
                                  GskCurveAddCurveFunc  add_curve_func,
                                  gpointer              user_data)
{
  if (gsk_conic_is_close_to_cubic (curve, cubic, tolerance))
    return add_curve_func (cubic->curve.points, user_data);
  else
    {
      GskCurve c1, c2;
      GskCurve cc1, cc2;

      gsk_conic_curve_split (curve, 0.5, &c1, &c2);

      cubic_approximation (&c1, &cc1);
      cubic_approximation (&c2, &cc2);

      return gsk_conic_curve_decompose_or_add (&c1, &cc1, tolerance, add_curve_func, user_data) &&
             gsk_conic_curve_decompose_or_add (&c2, &cc2, tolerance, add_curve_func, user_data);
    }
}

static gboolean
gsk_conic_curve_decompose_curve (const GskCurve       *curve,
                                 float                 tolerance,
                                 GskCurveAddCurveFunc  add_curve_func,
                                 gpointer              user_data)
{
  GskCurve c;

  cubic_approximation (curve, &c);

  return gsk_conic_curve_decompose_or_add (curve, &c, tolerance, add_curve_func, user_data);
}

static gskpathop
gsk_conic_curve_pathop (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_conic_curve_get_start_point (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return &self->points[0];
}

static const graphene_point_t *
gsk_conic_curve_get_end_point (const GskCurve *curve)
{
  const GskConicCurve *self = &curve->conic;

  return &self->points[3];
}

static void
gsk_conic_curve_get_start_tangent (const GskCurve  *curve,
                                   graphene_vec2_t *tangent)
{
  const GskConicCurve *self = &curve->conic;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_conic_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskConicCurve *self = &curve->conic;

  get_tangent (&self->points[1], &self->points[3], tangent);
}

static void
gsk_conic_curve_get_bounds (const GskCurve *curve,
                            GskBoundingBox *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);
  gsk_bounding_box_expand (bounds, &pts[1]);
}

/* Solve N = 0 where N is the numerator of (P/Q)', with
 * P = (1-t)^2*a + 2*t*(1-t)*w*b + t^2*c
 * Q = (1-t)^2 + 2*t*(1-t)*w + t^2
 */
static int
get_conic_extrema (float a, float b, float c, float w, float t[4])
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
gsk_conic_curve_get_tight_bounds (const GskCurve *curve,
                                  GskBoundingBox *bounds)
{
  const GskConicCurve *self = &curve->conic;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  float t[8];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[3]);

  n = 0;
  n += get_conic_extrema (pts[0].x, pts[1].x, pts[3].x, w, &t[n]);
  n += get_conic_extrema (pts[0].y, pts[1].y, pts[3].y, w, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_conic_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static void
gsk_conic_curve_offset (const GskCurve *curve,
                        float           distance,
                        GskCurve       *offset)
{
  const GskConicCurve *self = &curve->conic;
  const graphene_point_t *pts = self->points;
  graphene_vec2_t n;
  graphene_point_t p[4];
  graphene_point_t m1, m2;

  /* Simply scale control points, a la Tiller and Hanson */
  get_normal (&pts[0], &pts[1], &n);
  scale_point (&pts[0], &n, distance, &p[0]);
  scale_point (&pts[1], &n, distance, &m1);

  get_normal (&pts[1], &pts[3], &n);
  scale_point (&pts[1], &n, distance, &m2);
  scale_point (&pts[3], &n, distance, &p[3]);

  if (!line_intersection (&p[0], &m1, &m2, &p[3], &p[1]))
    p[1] = m1;

  p[2] = pts[2];

  gsk_conic_curve_init_from_points (&offset->conic, p);
}

static void
gsk_conic_curve_reverse (const GskCurve *curve,
                         GskCurve       *reverse)
{
  const GskConicCurve *self = &curve->conic;

  reverse->op = GSK_PATH_CONIC;
  reverse->conic.points[0] = self->points[3];
  reverse->conic.points[1] = self->points[1];
  reverse->conic.points[2] = self->points[2];
  reverse->conic.points[3] = self->points[0];
  reverse->conic.has_coefficients = FALSE;
}

static void
get_conic_derivatives (const GskCurve   *curve,
                       float             t,
                       graphene_vec2_t  *d,
                       graphene_vec2_t  *dd)
{
  /* See M. Floater, Derivatives of rational Bezier curves */
  graphene_point_t p[3], p1[2];
  float w, w1[2], w2;
  graphene_vec2_t t1, t2;

  w = curve->conic.points[2].x;

  p[0] = curve->conic.points[0];
  p[1] = curve->conic.points[1];
  p[2] = curve->conic.points[3];

  w1[0] = (1 - t) + t*w;
  w1[1] = (1 - t)*w + t;

  w2 = (1 - t)*w1[0] + t*w1[1];

  p1[0].x = ((1 - t)*p[0].x + t*w*p[1].x)/w1[0];
  p1[0].y = ((1 - t)*p[0].y + t*w*p[1].y)/w1[0];
  p1[1].x = ((1 - t)*w*p[1].x + t*p[2].x)/w1[1];
  p1[1].y = ((1 - t)*w*p[1].y + t*p[2].y)/w1[1];

  if (d)
    {
      get_tangent (&p1[0], &p1[1], &t1);
      graphene_vec2_scale (&t1, 2 * (w1[0]*w1[1])/(w2*w2), d);
    }

  if (dd)
    {
      get_tangent (&p[1], &p[2], &t1);
      graphene_vec2_scale (&t1, 2 * (1/pow3 (w2))*(4*w1[0]*w1[0] - w2 - 2*w1[0]*w2), &t1);

      get_tangent (&p[0], &p[1], &t2);
      graphene_vec2_scale (&t2, 2 * (1/pow3 (w2))*(4*w1[1]*w1[1] - w2 - 2*w1[1]*w2), &t2);
      graphene_vec2_subtract (&t1, &t2, dd);
    }
}

#if 0
static float
gsk_conic_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  /* See M. Floater, Derivatives of rational Bezier curves */
  graphene_point_t p[3], p1[2];
  float w, w1[2], w2;
  graphene_vec2_t t1, t2, t3;

  w = curve->conic.points[2].x;

  p[0] = curve->conic.points[0];
  p[1] = curve->conic.points[1];
  p[2] = curve->conic.points[3];

  w1[0] = (1 - t) + t*w;
  w1[1] = (1 - t)*w + t;

  w2 = (1 - t)*w1[0] + t*w1[1];

  p1[0].x = ((1 - t)*p[0].x + t*w*p[1].x)/w1[0];
  p1[0].y = ((1 - t)*p[0].y + t*w*p[1].y)/w1[0];
  p1[1].x = ((1 - t)*w*p[1].x + t*p[2].x)/w1[1];
  p1[1].y = ((1 - t)*w*p[1].y + t*p[2].y)/w1[1];

  get_tangent (&p[0], &p[1], &t1);
  get_tangent (&p[1], &p[2], &t2);
  get_tangent (&p1[0], &p1[1], &t3);

  return 0.5 * ((w*pow3 (w2))/(pow3 (w1[0])*pow3 (w1[1]))) * (cross (&t1, &t2) / pow3 (graphene_vec2_length (&t3)));
}
#endif

static float
gsk_conic_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  graphene_vec2_t d, dd;
  float num, denom;

  get_conic_derivatives (curve, t, &d, &dd);

  num = cross (&d, &dd);
  if (num == 0)
    return 0;

  denom = pow3 (graphene_vec2_length (&d));
  if (denom == 0)
    return 0;

  return num / denom;
}

static const GskCurveClass GSK_CONIC_CURVE_CLASS = {
  gsk_conic_curve_init,
  gsk_conic_curve_init_foreach,
  gsk_conic_curve_get_point,
  gsk_conic_curve_get_tangent,
  gsk_conic_curve_split,
  gsk_conic_curve_segment,
  gsk_conic_curve_decompose,
  gsk_conic_curve_decompose_curve,
  gsk_conic_curve_pathop,
  gsk_conic_curve_get_start_point,
  gsk_conic_curve_get_end_point,
  gsk_conic_curve_get_start_tangent,
  gsk_conic_curve_get_end_tangent,
  gsk_conic_curve_get_bounds,
  gsk_conic_curve_get_tight_bounds,
  gsk_conic_curve_offset,
  gsk_conic_curve_reverse,
  gsk_conic_curve_get_curvature
};

/* }}} */
/* {{{ API */

static inline const GskCurveClass *
get_class (GskPathOperation op)
{
  const GskCurveClass *klasses[] = {
    [GSK_PATH_CLOSE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_LINE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_CURVE] = &GSK_CURVE_CURVE_CLASS,
    [GSK_PATH_CONIC] = &GSK_CONIC_CURVE_CLASS,
  };

  g_assert (op < G_N_ELEMENTS (klasses) && klasses[op] != NULL);

  return klasses[op];
}

void
gsk_curve_init (GskCurve  *curve,
                gskpathop  op)
{
  get_class (gsk_pathop_op (op))->init (curve, op);
}

void
gsk_curve_init_foreach (GskCurve               *curve,
                        GskPathOperation        op,
                        const graphene_point_t *pts,
                        gsize                   n_pts,
                        float                   weight)
{
  get_class (op)->init_foreach (curve, op, pts, n_pts, weight);
}

void
gsk_curve_get_point (const GskCurve   *curve,
                     float             progress,
                     graphene_point_t *pos)
{
  get_class (curve->op)->get_point (curve, progress, pos);
}

void
gsk_curve_get_tangent (const GskCurve   *curve,
                       float             progress,
                       graphene_vec2_t  *tangent)
{
  get_class (curve->op)->get_tangent (curve, progress, tangent);
}

void
gsk_curve_split (const GskCurve *curve,
                 float           progress,
                 GskCurve       *start,
                 GskCurve       *end)
{
  get_class (curve->op)->split (curve, progress, start, end);
}

void
gsk_curve_segment (const GskCurve *curve,
                   float           start,
                   float           end,
                   GskCurve       *segment)
{
  if (start <= 0 && end >= 1)
    {
      *segment = *curve;
      return;
    }

  get_class (curve->op)->segment (curve, start, end, segment);
}

gboolean
gsk_curve_decompose (const GskCurve      *curve,
                     float                tolerance,
                     GskCurveAddLineFunc  add_line_func,
                     gpointer             user_data)
{
  return get_class (curve->op)->decompose (curve, tolerance, add_line_func, user_data);
}

gboolean
gsk_curve_decompose_curve (const GskCurve       *curve,
                           float                 tolerance,
                           GskCurveAddCurveFunc  add_curve_func,
                           gpointer              user_data)
{
  return get_class (curve->op)->decompose_curve (curve, tolerance, add_curve_func, user_data);
}

gskpathop
gsk_curve_pathop (const GskCurve *curve)
{
  return get_class (curve->op)->pathop (curve);
}

const graphene_point_t *
gsk_curve_get_start_point (const GskCurve *curve)
{
  return get_class (curve->op)->get_start_point (curve);
}

const graphene_point_t *
gsk_curve_get_end_point (const GskCurve *curve)
{
  return get_class (curve->op)->get_end_point (curve);
}

void
gsk_curve_get_start_tangent (const GskCurve  *curve,
                             graphene_vec2_t *tangent)
{
  get_class (curve->op)->get_start_tangent (curve, tangent);
}

void
gsk_curve_get_end_tangent (const GskCurve  *curve,
                           graphene_vec2_t *tangent)
{
  get_class (curve->op)->get_end_tangent (curve, tangent);
}

void
gsk_curve_get_bounds (const GskCurve  *curve,
                      GskBoundingBox  *bounds)
{
  get_class (curve->op)->get_bounds (curve, bounds);
}

void
gsk_curve_get_tight_bounds (const GskCurve  *curve,
                            GskBoundingBox  *bounds)
{
  get_class (curve->op)->get_tight_bounds (curve, bounds);
}

void
gsk_curve_offset (const GskCurve *curve,
                  float           distance,
                  GskCurve       *offset_curve)
{
  get_class (curve->op)->offset (curve, distance, offset_curve);
}

void
gsk_curve_reverse (const GskCurve *curve,
                   GskCurve       *reverse)
{
  get_class (curve->op)->reverse (curve, reverse);
}

void
gsk_curve_get_normal (const GskCurve  *curve,
                      float            t,
                      graphene_vec2_t *normal)
{
  graphene_vec2_t tangent;

  gsk_curve_get_tangent (curve, t, &tangent);
  graphene_vec2_init (normal,
                      - graphene_vec2_get_y (&tangent),
                      graphene_vec2_get_x (&tangent));
}

float
gsk_curve_get_curvature (const GskCurve   *curve,
                         float             t,
                         graphene_point_t *center)
{
  float k;

  k = get_class (curve->op)->get_curvature (curve, t);

  if (center != NULL && k != 0)
    {
      graphene_point_t p;
      graphene_vec2_t n;
      float r;

      r = 1/k;
      gsk_curve_get_point (curve, t, &p);
      gsk_curve_get_normal (curve, t, &n);
      center->x = p.x + r * graphene_vec2_get_x (&n);
      center->y = p.y + r * graphene_vec2_get_y (&n);
    }

  return k;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
