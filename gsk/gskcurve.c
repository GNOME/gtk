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
  void                          (* eval)                (const GskCurve         *curve,
                                                         float                   progress,
                                                         graphene_point_t       *pos,
                                                         graphene_vec2_t        *tangent);
  void                          (* split)               (const GskCurve         *curve,
                                                         float                   progress,
                                                         GskCurve               *result1,
                                                         GskCurve               *result2);
  gboolean                      (* decompose)           (const GskCurve         *curve,
                                                         float                   tolerance,
                                                         GskCurveAddLineFunc     add_line_func,
                                                         gpointer                user_data);
  gskpathop                     (* pathop)              (const GskCurve         *curve);
  const graphene_point_t *      (* get_start_point)     (const GskCurve         *curve);
  const graphene_point_t *      (* get_end_point)       (const GskCurve         *curve);
  void                          (* get_start_tangent)   (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_end_tangent)     (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_bounds)          (const GskCurve         *curve,
                                                         graphene_rect_t        *bounds);
  void                          (* get_tight_bounds)    (const GskCurve         *curve,
                                                         graphene_rect_t        *bounds);
};

/** LINE **/

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
gsk_line_curve_eval (const GskCurve   *curve,
                     float             progress,
                     graphene_point_t *pos,
                     graphene_vec2_t  *tangent)
{
  const GskLineCurve *self = &curve->line;

  if (pos)
    graphene_point_interpolate (&self->points[0], &self->points[1], progress, pos);

  if (tangent)
    {
      graphene_vec2_init (tangent, self->points[1].x - self->points[0].x, self->points[1].y - self->points[0].y);
      graphene_vec2_normalize (tangent, tangent);
    }
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

static gboolean
gsk_line_curve_decompose (const GskCurve      *curve,
                          float                tolerance,
                          GskCurveAddLineFunc  add_line_func,
                          gpointer             user_data)
{
  const GskLineCurve *self = &curve->line;

  return add_line_func (&self->points[0], &self->points[1], 0.0f, 1.0f, user_data);
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
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

static void
gsk_line_curve_get_tangent (const GskCurve  *curve,
                            graphene_vec2_t *tangent)
{
  const GskLineCurve *self = &curve->line;

  return get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_line_curve_get_bounds (const GskCurve  *curve,
                           graphene_rect_t *bounds)
{
  const GskLineCurve *self = &curve->line;

  graphene_rect_init (bounds, self->points[0].x, self->points[0].y, 0, 0);
  graphene_rect_expand (bounds, &self->points[1], bounds);
}

static const GskCurveClass GSK_LINE_CURVE_CLASS = {
  gsk_line_curve_init,
  gsk_line_curve_eval,
  gsk_line_curve_split,
  gsk_line_curve_decompose,
  gsk_line_curve_pathop,
  gsk_line_curve_get_start_point,
  gsk_line_curve_get_end_point,
  gsk_line_curve_get_tangent,
  gsk_line_curve_get_tangent,
  gsk_line_curve_get_bounds,
  gsk_line_curve_get_bounds
};

/** CURVE **/

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
gsk_curve_curve_eval (const GskCurve   *curve,
                      float             progress,
                      graphene_point_t *pos,
                      graphene_vec2_t  *tangent)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *c = self->coeffs;

  gsk_curve_curve_ensure_coefficients (self);

  if (pos)
    *pos = GRAPHENE_POINT_INIT (((c[0].x * progress + c[1].x) * progress +c[2].x) * progress + c[3].x,
                                ((c[0].y * progress + c[1].y) * progress +c[2].y) * progress + c[3].y);
  if (tangent)
    {
      graphene_vec2_init (tangent,
                          (3.0f * c[0].x * progress + 2.0f * c[1].x) * progress + c[2].x,
                          (3.0f * c[0].y * progress + 2.0f * c[1].y) * progress + c[2].y);
      graphene_vec2_normalize (tangent, tangent);
    }
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

  return get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_curve_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskCurveCurve *self = &curve->curve;

  return get_tangent (&self->points[2], &self->points[3], tangent);
}

static void
gsk_curve_curve_get_bounds (const GskCurve  *curve,
                            graphene_rect_t *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;

  graphene_rect_init (bounds, pts[0].x, pts[0].y, 0, 0);
  graphene_rect_expand (bounds, &pts[1], bounds);
  graphene_rect_expand (bounds, &pts[2], bounds);
  graphene_rect_expand (bounds, &pts[3], bounds);
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
                                  graphene_rect_t *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;
  float t[4];
  int n;

  graphene_rect_init (bounds, pts[0].x, pts[0].y, 0, 0);
  graphene_rect_expand (bounds, &pts[3], bounds);

  n = 0;
  n += get_cubic_extrema (pts[0].x, pts[1].x, pts[2].x, pts[3].x, &t[n]);
  n += get_cubic_extrema (pts[0].y, pts[1].y, pts[2].y, pts[3].y, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_curve_curve_eval (curve, t[i], &p, NULL);
      graphene_rect_expand (bounds, &p, bounds);
    }
}

static const GskCurveClass GSK_CURVE_CURVE_CLASS = {
  gsk_curve_curve_init,
  gsk_curve_curve_eval,
  gsk_curve_curve_split,
  gsk_curve_curve_decompose,
  gsk_curve_curve_pathop,
  gsk_curve_curve_get_start_point,
  gsk_curve_curve_get_end_point,
  gsk_curve_curve_get_start_tangent,
  gsk_curve_curve_get_end_tangent,
  gsk_curve_curve_get_bounds,
  gsk_curve_curve_get_tight_bounds
};

/** CONIC **/

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
gsk_conic_curve_eval (const GskCurve   *curve,
                      float             progress,
                      graphene_point_t *pos,
                      graphene_vec2_t  *tangent)
{
  const GskConicCurve *self = &curve->conic;

  gsk_conic_curve_ensure_coefficents (self);

  if (pos)
    gsk_conic_curve_eval_point (self, progress, pos);

  if (tangent)
    {
      graphene_point_t tmp;
      float w = gsk_conic_curve_get_weight (self);
      const graphene_point_t *pts = self->points;

      /* The tangent will be 0 in these corner cases, just
       * treat it like a line here. */
      if ((progress <= 0.f && graphene_point_equal (&pts[0], &pts[1])) ||
          (progress >= 1.f && graphene_point_equal (&pts[1], &pts[3])))
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
                           progress,
                           &tmp);
      graphene_vec2_init (tangent, tmp.x, tmp.y);
      graphene_vec2_normalize (tangent, tangent);
    }
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

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_conic_curve_too_curvy (const graphene_point_t *start,
                           const graphene_point_t *mid,
                           const graphene_point_t *end,
                           float                  tolerance)
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

  if (end_progress - start_progress < MIN_PROGRESS ||
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

  gsk_conic_curve_ensure_coefficents (self);

  return gsk_conic_curve_decompose_subdivide (self,
                                              tolerance,
                                              &self->points[0],
                                              0.0f,
                                              &self->points[3],
                                              1.0f,
                                              add_line_func,
                                              user_data);
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
  const GskCurveCurve *self = &curve->curve;

  return get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_conic_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskCurveCurve *self = &curve->curve;

  return get_tangent (&self->points[1], &self->points[3], tangent);
}

static void
gsk_conic_curve_get_bounds (const GskCurve  *curve,
                            graphene_rect_t *bounds)
{
  const GskCurveCurve *self = &curve->curve;
  const graphene_point_t *pts = self->points;

  graphene_rect_init (bounds, pts[0].x, pts[0].y, 0, 0);
  graphene_rect_expand (bounds, &pts[1], bounds);
  graphene_rect_expand (bounds, &pts[3], bounds);
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
gsk_conic_curve_get_tight_bounds (const GskCurve  *curve,
                                  graphene_rect_t *bounds)
{
  const GskConicCurve *self = &curve->conic;
  float w = gsk_conic_curve_get_weight (self);
  const graphene_point_t *pts = self->points;
  float t[8];
  int n;

  graphene_rect_init (bounds, pts[0].x, pts[0].y, 0, 0);
  graphene_rect_expand (bounds, &pts[3], bounds);

  n = 0;
  n += get_conic_extrema (pts[0].x, pts[1].x, pts[3].x, w, &t[n]);
  n += get_conic_extrema (pts[0].y, pts[1].y, pts[3].y, w, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_conic_curve_eval (curve, t[i], &p, NULL);
      graphene_rect_expand (bounds, &p, bounds);
    }
}

static const GskCurveClass GSK_CONIC_CURVE_CLASS = {
  gsk_conic_curve_init,
  gsk_conic_curve_eval,
  gsk_conic_curve_split,
  gsk_conic_curve_decompose,
  gsk_conic_curve_pathop,
  gsk_conic_curve_get_start_point,
  gsk_conic_curve_get_end_point,
  gsk_conic_curve_get_start_tangent,
  gsk_conic_curve_get_end_tangent,
  gsk_conic_curve_get_bounds,
  gsk_conic_curve_get_tight_bounds
};

/** API **/

static const GskCurveClass *
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
gsk_curve_eval (const GskCurve   *curve,
                float             progress,
                graphene_point_t *pos,
                graphene_vec2_t  *tangent)
{
  get_class (curve->op)->eval (curve, progress, pos, tangent);
}

void
gsk_curve_split (const GskCurve *curve,
                 float           progress,
                 GskCurve       *start,
                 GskCurve       *end)
{
  get_class (curve->op)->split (curve, progress, start, end);
}

gboolean
gsk_curve_decompose (const GskCurve      *curve,
                     float                tolerance,
                     GskCurveAddLineFunc  add_line_func,
                     gpointer             user_data)
{
  return get_class (curve->op)->decompose (curve, tolerance, add_line_func, user_data);
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
                      graphene_rect_t *bounds)
{
  get_class (curve->op)->get_bounds (curve, bounds);
}

void
gsk_curve_get_tight_bounds (const GskCurve  *curve,
                            graphene_rect_t *bounds)
{
  get_class (curve->op)->get_tight_bounds (curve, bounds);
}

/** Intersections **/

static int
line_intersect (const GskCurve   *curve1,
                const GskCurve   *curve2,
                float            *t1,
                float            *t2,
                graphene_point_t *p)
{
  const graphene_point_t *pts1 = curve1->line.points;
  const graphene_point_t *pts2 = curve2->line.points;
  float a1 = pts1[0].x - pts1[1].x;
  float b1 = pts1[0].y - pts1[1].y;
  float a2 = pts2[0].x - pts2[1].x;
  float b2 = pts2[0].y - pts2[1].y;
  float det = a1 * b2 - b1 * a2;

  if (det != 0)
    {
      float tt =   ((pts1[0].x - pts2[0].x) * b2 - (pts1[0].y - pts2[0].y) * a2) / det;
      float ss = - ((pts1[0].y - pts2[0].y) * a1 - (pts1[0].x - pts2[0].x) * b1) / det;

      if (acceptable (tt) && acceptable (ss))
        {
          p->x = pts1[0].x + tt * (pts1[1].x - pts1[0].x);
          p->y = pts1[0].y + tt * (pts1[1].y - pts1[0].y);

          *t1 = tt;
          *t2 = ss;

          return 1;
        }
    }

  return 0;
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

  get_tangent (a, b, &n1);
  angle = -atan2 (graphene_vec2_get_y (&n1), graphene_vec2_get_x (&n1));

  for (int i = 0; i < n; i++)
    {
      q[i].x = (p[i].x - a->x) * cos (angle) - (p[i].y - a->y) * sin (angle);
      q[i].y = (p[i].x - a->x) * sin (angle) + (p[i].y - a->y) * cos (angle);
    }
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
  int m, i;

  /* Rotate things to place curve1 on the x axis,
   * then solve curve2 for y == 0.
   */
  align_points (curve2->curve.points, a, b, pts, 4);

  m = get_cubic_roots (pts[0].y, pts[1].y, pts[2].y, pts[3].y, t);

  m = MIN (m, n);
  for (i = 0; i < m; i++)
    {
      t2[i] = t[i];
      gsk_curve_eval (curve2, t[i], &p[i], NULL);
      find_point_on_line (a, b, &p[i], &t1[i]);
    }

  return m;
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
                         int              *pos)
{
  GskCurve p11, p12, p21, p22;
  graphene_rect_t b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  gsk_curve_get_tight_bounds (curve1, &b1);
  gsk_curve_get_tight_bounds (curve2, &b2);

  if (!graphene_rect_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.size.width < 0.1 && b1.size.height < 0.1 &&
      b2.size.width < 0.1 && b2.size.height < 0.1)
    {
      graphene_point_t c;
      t1[*pos] = t1l + d1;
      t2[*pos] = t2l + d2;
      gsk_curve_eval (curve1, 0.5, &c, NULL);

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

  curve_intersect_recurse (&p11, &p21, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos);
  curve_intersect_recurse (&p11, &p22, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos);
  curve_intersect_recurse (&p12, &p21, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos);
  curve_intersect_recurse (&p12, &p22, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos);
}

static int
curve_intersect (const GskCurve   *curve1,
                 const GskCurve   *curve2,
                 float            *t1,
                 float            *t2,
                 graphene_point_t *p,
                 int               n)
{
  int pos = 0;

  curve_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos);

  return pos;
}

static void
get_bounds (const GskCurve  *curve,
            float            tl,
            float            tr,
            graphene_rect_t *bounds)
{
  if (curve->op == GSK_PATH_CONIC)
    {
      const graphene_point_t *pts = curve->conic.points;
      graphene_point3d_t c[3], l[3], r[3], rest[3];
      graphene_point3d_t *cc;
      graphene_point_t p[4];
      GskCurve curve1;
      float w;

      w = pts[2].x;
      c[0] = GRAPHENE_POINT3D_INIT (pts[0].x, pts[0].y, 1);
      c[1] = GRAPHENE_POINT3D_INIT (pts[1].x * w, pts[1].y * w, w);
      c[2] = GRAPHENE_POINT3D_INIT (pts[3].x, pts[3].y, 1);

      cc = c;

      if (tl > 0)
        {
          split_bezier3d (cc, 3, tl, l, r);
          cc = r;
        }

      if (tr < 1)
        {
          split_bezier3d (cc, 3, (tr - tl) / (1 - tl), l, rest);
          cc = l;
        }

      p[0] = GRAPHENE_POINT_INIT (cc[0].x / cc[0].z, cc[0].y / cc[0].z);
      p[1] = GRAPHENE_POINT_INIT (cc[1].x / cc[1].z, cc[1].y / cc[1].z);
      p[3] = GRAPHENE_POINT_INIT (cc[2].x / cc[2].z, cc[2].y / cc[2].z);

      for (int i = 0; i < 3; i++)
        cc[i].z /= cc[0].z;

      p[2] = GRAPHENE_POINT_INIT (cc[1].z / sqrt (cc[2].z), 0);

      gsk_curve_init (&curve1, gsk_pathop_encode (GSK_PATH_CONIC, p));
      gsk_curve_get_tight_bounds (&curve1, bounds);
    }
  else
    {
      GskCurve c1, c2, c3;
      const GskCurve *c;

      c = curve;
      if (tl > 0)
        {
          gsk_curve_split (c, tl, &c1, &c2);
          c = &c2;
        }

      if (tr < 1)
        {
          gsk_curve_split (c, (tr - tl) / (1 - tl), &c3, &c1);
          c = &c3;
        }
      gsk_curve_get_tight_bounds (c, bounds);
    }
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
                           int              *pos)
{
  graphene_rect_t b1, b2;
  float d1, d2;

  if (*pos == n)
    return;

  get_bounds (curve1, t1l, t1r, &b1);
  get_bounds (curve2, t2l, t2r, &b2);

  if (!graphene_rect_intersection (&b1, &b2, NULL))
    return;

  d1 = (t1r - t1l) / 2;
  d2 = (t2r - t2l) / 2;

  if (b1.size.width < 0.1 && b1.size.height < 0.1 &&
      b2.size.width < 0.1 && b2.size.height < 0.1)
    {
      graphene_point_t c;
      t1[*pos] = t1l + d1;
      t2[*pos] = t2l + d2;
      gsk_curve_eval (curve1, t1[*pos], &c, NULL);

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
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l,      t2l + d2, t1, t2, p, n, pos);
  general_intersect_recurse (curve1, curve2, t1l,      t1l + d1, t2l + d2, t2r,      t1, t2, p, n, pos);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l,      t2l + d2, t1, t2, p, n, pos);
  general_intersect_recurse (curve1, curve2, t1l + d1, t1r,      t2l + d2, t2r,      t1, t2, p, n, pos);
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

  general_intersect_recurse (curve1, curve2, 0, 1, 0, 1, t1, t2, p, n, &pos);

  return pos;
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
  const GskCurveClass *c1 = get_class (curve1->op);
  const GskCurveClass *c2 = get_class (curve2->op);

  /* We special-case line-line and line-curve intersections,
   * since we can solve them directly.
   * Everything else is done via bisection.
   */
  if (c1 == &GSK_LINE_CURVE_CLASS && c2 == &GSK_LINE_CURVE_CLASS)
    return line_intersect (curve1, curve2, t1, t2, p);
  else if (c1 == &GSK_LINE_CURVE_CLASS && c2 == &GSK_CURVE_CURVE_CLASS)
    return line_curve_intersect (curve1, curve2, t1, t2, p, n);
  else if (c1 == &GSK_CURVE_CURVE_CLASS && c2 == &GSK_LINE_CURVE_CLASS)
    return line_curve_intersect (curve2, curve1, t2, t1, p, n);
  else if (c1 == &GSK_CURVE_CURVE_CLASS && c2 == &GSK_CURVE_CURVE_CLASS)
    return curve_intersect (curve1, curve2, t1, t2, p, n);
  else
    return general_intersect (curve1, curve2, t1, t2, p, n);

}
