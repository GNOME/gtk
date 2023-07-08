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
#include "gskboundingboxprivate.h"

/* GskCurve collects all the functionality we need for Bézier segments */

#define MIN_PROGRESS (1/1024.f)

typedef struct _GskCurveClass GskCurveClass;

struct _GskCurveClass
{
  void                          (* init)                (GskCurve               *curve,
                                                         gskpathop               op);
  void                          (* init_foreach)        (GskCurve               *curve,
                                                         GskPathOperation        op,
                                                         const graphene_point_t *pts,
                                                         gsize                   n_pts);
  void                          (* print)               (const GskCurve         *curve,
                                                         GString                *string);
  gskpathop                     (* pathop)              (const GskCurve         *curve);
  const graphene_point_t *      (* get_start_point)     (const GskCurve         *curve);
  const graphene_point_t *      (* get_end_point)       (const GskCurve         *curve);
  void                          (* get_start_tangent)   (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_end_tangent)     (const GskCurve         *curve,
                                                         graphene_vec2_t        *tangent);
  void                          (* get_point)           (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_point_t       *pos);
  void                          (* get_tangent)         (const GskCurve         *curve,
                                                         float                   t,
                                                         graphene_vec2_t        *tangent);
  void                          (* reverse)             (const GskCurve         *curve,
                                                         GskCurve               *reverse);
  float                         (* get_curvature)       (const GskCurve         *curve,
                                                         float                   t);
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
                                                         GskPathForeachFlags     flags,
                                                         float                   tolerance,
                                                         GskCurveAddCurveFunc    add_curve_func,
                                                         gpointer                user_data);
  void                          (* get_bounds)          (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
  void                          (* get_tight_bounds)    (const GskCurve         *curve,
                                                         GskBoundingBox         *bounds);
};

/* {{{ Utilities */

static void
get_tangent (const graphene_point_t *p0,
             const graphene_point_t *p1,
             graphene_vec2_t        *t)
{
  graphene_vec2_init (t, p1->x - p0->x, p1->y - p0->y);
  graphene_vec2_normalize (t, t);
}

/* Replace a line by an equivalent quad,
 * and a quad by an equivalent cubic.
 */
static void
gsk_curve_elevate (const GskCurve *curve,
                   GskCurve       *elevated)
{
  if (curve->op == GSK_PATH_LINE)
    {
      graphene_point_t p[3];

      p[0] = curve->line.points[0];
      graphene_point_interpolate (&curve->line.points[0],
                                  &curve->line.points[1],
                                  0.5,
                                  &p[1]);
      p[2] = curve->line.points[1];
      gsk_curve_init (elevated, gsk_pathop_encode (GSK_PATH_QUAD, p));
    }
  else if (curve->op == GSK_PATH_QUAD)
    {
      graphene_point_t p[4];

      p[0] = curve->quad.points[0];
      graphene_point_interpolate (&curve->quad.points[0],
                                  &curve->quad.points[1],
                                  2/3.,
                                  &p[1]);
      graphene_point_interpolate (&curve->quad.points[2],
                                  &curve->quad.points[1],
                                  2/3.,
                                  &p[2]);
      p[3] = curve->quad.points[2];
      gsk_curve_init (elevated, gsk_pathop_encode (GSK_PATH_CUBIC, p));
    }
  else
    g_assert_not_reached ();
}

/* }}} */
/* {{{ Line  */

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
                             gsize                   n_pts)
{
  GskLineCurve *self = &curve->line;

  g_assert (n_pts == 2);

  gsk_line_curve_init_from_points (self, op, &pts[0], &pts[1]);
}

static void
gsk_line_curve_print (const GskCurve *curve,
                      GString        *string)
{
  g_string_append_printf (string,
                          "M %g %g L %g %g",
                          curve->line.points[0].x, curve->line.points[0].y,
                          curve->line.points[1].x, curve->line.points[1].y);
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

static float
gsk_line_curve_get_curvature (const GskCurve *curve,
                              float           t)
{
  return 0;
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

  return add_line_func (&self->points[0], &self->points[1], 0.0f, 1.0f, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
}

static gboolean
gsk_line_curve_decompose_curve (const GskCurve       *curve,
                                GskPathForeachFlags   flags,
                                float                 tolerance,
                                GskCurveAddCurveFunc  add_curve_func,
                                gpointer              user_data)
{
  const GskLineCurve *self = &curve->line;

  return add_curve_func (GSK_PATH_LINE, self->points, 2, user_data);
}

static void
gsk_line_curve_get_bounds (const GskCurve  *curve,
                           GskBoundingBox  *bounds)
{
  const GskLineCurve *self = &curve->line;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[1]);
}

static const GskCurveClass GSK_LINE_CURVE_CLASS = {
  gsk_line_curve_init,
  gsk_line_curve_init_foreach,
  gsk_line_curve_print,
  gsk_line_curve_pathop,
  gsk_line_curve_get_start_point,
  gsk_line_curve_get_end_point,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_start_end_tangent,
  gsk_line_curve_get_point,
  gsk_line_curve_get_tangent,
  gsk_line_curve_reverse,
  gsk_line_curve_get_curvature,
  gsk_line_curve_split,
  gsk_line_curve_segment,
  gsk_line_curve_decompose,
  gsk_line_curve_decompose_curve,
  gsk_line_curve_get_bounds,
  gsk_line_curve_get_bounds,
};

/* }}} */
/* {{{ Quadratic */

static void
gsk_quad_curve_ensure_coefficients (const GskQuadCurve *curve)
{
  GskQuadCurve *self = (GskQuadCurve *) curve;
  const graphene_point_t *pts = self->points;

  if (self->has_coefficients)
    return;

  self->coeffs[2] = pts[0];
  self->coeffs[1] = GRAPHENE_POINT_INIT (2 * (pts[1].x - pts[0].x),
                                         2 * (pts[1].y - pts[0].y));
  self->coeffs[0] = GRAPHENE_POINT_INIT (pts[2].x - 2 * pts[1].x + pts[0].x,
                                         pts[2].y - 2 * pts[1].y + pts[0].y);

  self->has_coefficients = TRUE;
}

static void
gsk_quad_curve_init_from_points (GskQuadCurve           *self,
                                 const graphene_point_t  pts[3])
{
  self->op = GSK_PATH_QUAD;
  self->has_coefficients = FALSE;
  memcpy (self->points, pts, sizeof (graphene_point_t) * 3);
}

static void
gsk_quad_curve_init (GskCurve  *curve,
                     gskpathop  op)
{
  GskQuadCurve *self = &curve->quad;

  gsk_quad_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_quad_curve_init_foreach (GskCurve               *curve,
                             GskPathOperation        op,
                             const graphene_point_t *pts,
                             gsize                   n_pts)
{
  GskQuadCurve *self = &curve->quad;

  g_assert (n_pts == 3);

  gsk_quad_curve_init_from_points (self, pts);
}

static void
gsk_quad_curve_print (const GskCurve *curve,
                      GString        *string)
{
  g_string_append_printf (string,
                          "M %g %g Q %g %g %g %g",
                          curve->quad.points[0].x, curve->quad.points[0].y,
                          curve->quad.points[1].x, curve->cubic.points[1].y,
                          curve->quad.points[2].x, curve->cubic.points[2].y);
}

static gskpathop
gsk_quad_curve_pathop (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_quad_curve_get_start_point (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return &self->points[0];
}

static const graphene_point_t *
gsk_quad_curve_get_end_point (const GskCurve *curve)
{
  const GskQuadCurve *self = &curve->quad;

  return &self->points[2];
}

static void
gsk_quad_curve_get_start_tangent (const GskCurve  *curve,
                                  graphene_vec2_t *tangent)
{
  const GskQuadCurve *self = &curve->quad;

  get_tangent (&self->points[0], &self->points[1], tangent);
}

static void
gsk_quad_curve_get_end_tangent (const GskCurve  *curve,
                                graphene_vec2_t *tangent)
{
  const GskQuadCurve *self = &curve->quad;

  get_tangent (&self->points[1], &self->points[2], tangent);
}

static void
gsk_quad_curve_get_point (const GskCurve   *curve,
                          float             t,
                          graphene_point_t *pos)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *c = self->coeffs;

  gsk_quad_curve_ensure_coefficients (self);

  *pos = GRAPHENE_POINT_INIT ((c[0].x * t + c[1].x) * t + c[2].x,
                              (c[0].y * t + c[1].y) * t + c[2].y);
}

static void
gsk_quad_curve_get_tangent (const GskCurve   *curve,
                            float             t,
                            graphene_vec2_t  *tangent)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *c = self->coeffs;

  gsk_quad_curve_ensure_coefficients (self);

  graphene_vec2_init (tangent,
                      2.0f * c[0].x * t + c[1].x,
                      2.0f * c[0].y * t + c[1].y);
  graphene_vec2_normalize (tangent, tangent);
}


static float gsk_cubic_curve_get_curvature (const GskCurve *curve,
                                            float           t);

static float
gsk_quad_curve_get_curvature (const GskCurve *curve,
                              float           t)
{
  return gsk_cubic_curve_get_curvature (curve, t);
}

static void
gsk_quad_curve_reverse (const GskCurve *curve,
                        GskCurve       *reverse)
{
  const GskCubicCurve *self = &curve->cubic;

  reverse->op = GSK_PATH_QUAD;
  reverse->cubic.points[0] = self->points[2];
  reverse->cubic.points[1] = self->points[1];
  reverse->cubic.points[2] = self->points[0];
  reverse->cubic.has_coefficients = FALSE;
}

static void
gsk_quad_curve_split (const GskCurve   *curve,
                      float             progress,
                      GskCurve         *start,
                      GskCurve         *end)
{
  GskQuadCurve *self = (GskQuadCurve *) &curve->quad;
  const graphene_point_t *pts = self->points;
  graphene_point_t ab, bc;
  graphene_point_t final;

  graphene_point_interpolate (&pts[0], &pts[1], progress, &ab);
  graphene_point_interpolate (&pts[1], &pts[2], progress, &bc);
  graphene_point_interpolate (&ab, &bc, progress, &final);

  if (start)
    gsk_quad_curve_init_from_points (&start->quad, (graphene_point_t[3]) { pts[0], ab, final });
  if (end)
    gsk_quad_curve_init_from_points (&end->quad, (graphene_point_t[3]) { final, bc, pts[2] });
}

static void
gsk_quad_curve_segment (const GskCurve *curve,
                        float           start,
                        float           end,
                        GskCurve       *segment)
{
  GskCurve tmp;

  gsk_quad_curve_split (curve, start, NULL, &tmp);
  gsk_quad_curve_split (&tmp, (end - start) / (1.0f - start), segment, NULL);
}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_quad_curve_too_curvy (const GskQuadCurve *self,
                               float          tolerance)
{
  const graphene_point_t *pts = self->points;
  float dx, dy;

  dx = fabs (pts[1].x / 2 - (pts[0].x + pts[2].x) / 4);
  dy = fabs (pts[1].y / 2 - (pts[0].y + pts[2].y) / 4);

  return MAX (dx, dy) > tolerance;
}

static gboolean
gsk_quad_curve_decompose_step (const GskCurve      *curve,
                               float                start_progress,
                               float                end_progress,
                               float                tolerance,
                               GskCurveAddLineFunc  add_line_func,
                               gpointer             user_data)
{
  const GskQuadCurve *self = &curve->quad;
  GskCurve left, right;
  float mid_progress;

  if (!gsk_quad_curve_too_curvy (self, tolerance))
    return add_line_func (&self->points[0], &self->points[2], start_progress, end_progress, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
  if (end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (&self->points[0], &self->points[2], start_progress, end_progress, GSK_CURVE_LINE_REASON_SHORT, user_data);

  gsk_quad_curve_split ((const GskCurve *) self, 0.5, &left, &right);
  mid_progress = (start_progress + end_progress) / 2;

  return gsk_quad_curve_decompose_step (&left, start_progress, mid_progress, tolerance, add_line_func, user_data)
      && gsk_quad_curve_decompose_step (&right, mid_progress, end_progress, tolerance, add_line_func, user_data);
}

static gboolean
gsk_quad_curve_decompose (const GskCurve      *curve,
                          float                tolerance,
                          GskCurveAddLineFunc  add_line_func,
                          gpointer             user_data)
{
  return gsk_quad_curve_decompose_step (curve, 0.0, 1.0, tolerance, add_line_func, user_data);
}

typedef struct
{
  GskCurveAddCurveFunc add_curve;
  gpointer user_data;
} AddLineData;

static gboolean
gsk_curve_add_line_cb (const graphene_point_t *from,
                       const graphene_point_t *to,
                       float                   from_progress,
                       float                   to_progress,
                       GskCurveLineReason      reason,
                       gpointer                user_data)
{
  AddLineData *data = user_data;
  graphene_point_t p[2] = { *from, *to };

  return data->add_curve (GSK_PATH_LINE, p, 2, data->user_data);
}

static gboolean
gsk_quad_curve_decompose_curve (const GskCurve       *curve,
                                GskPathForeachFlags   flags,
                                float                 tolerance,
                                GskCurveAddCurveFunc  add_curve_func,
                                gpointer              user_data)
{
  const GskQuadCurve *self = &curve->quad;

  if (flags & GSK_PATH_FOREACH_ALLOW_QUAD)
    return add_curve_func (GSK_PATH_QUAD, self->points, 3, user_data);
  else if (flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
    {
      GskCurve c;

      gsk_curve_elevate (curve, &c);
      return add_curve_func (GSK_PATH_CUBIC, c.cubic.points, 4, user_data);
    }
  else
    {
      return gsk_quad_curve_decompose (curve,
                                       tolerance,
                                       gsk_curve_add_line_cb,
                                       &(AddLineData) { add_curve_func, user_data });
    }
}

static void
gsk_quad_curve_get_bounds (const GskCurve  *curve,
                           GskBoundingBox  *bounds)
{
  const GskQuadCurve *self = &curve->quad;
  const graphene_point_t *pts = self->points;

  gsk_bounding_box_init (bounds, &pts[0], &pts[2]);
  gsk_bounding_box_expand (bounds, &pts[1]);
}

/* Solve P' = 0 where P is
 * P = (1-t)^2*pa + 2*t*(1-t)*pb + t^2*pc
 */
static int
get_quadratic_extrema (float pa, float pb, float pc, float t[1])
{
  float d = pa - 2 * pb + pc;

  if (fabs (d) > 0.0001)
    {
      t[0] = (pa - pb) / d;
      return 1;
    }

  return 0;
}

static void
gsk_quad_curve_get_tight_bounds (const GskCurve  *curve,
                                 GskBoundingBox  *bounds)
{
  const GskQuadCurve *self = &curve->quad;
  const graphene_point_t *pts = self->points;
  float t[4];
  int n;

  gsk_bounding_box_init (bounds, &pts[0], &pts[2]);

  n = 0;
  n += get_quadratic_extrema (pts[0].x, pts[1].x, pts[2].x, &t[n]);
  n += get_quadratic_extrema (pts[0].y, pts[1].y, pts[2].y, &t[n]);

  for (int i = 0; i < n; i++)
    {
      graphene_point_t p;

      gsk_quad_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static const GskCurveClass GSK_QUAD_CURVE_CLASS = {
  gsk_quad_curve_init,
  gsk_quad_curve_init_foreach,
  gsk_quad_curve_print,
  gsk_quad_curve_pathop,
  gsk_quad_curve_get_start_point,
  gsk_quad_curve_get_end_point,
  gsk_quad_curve_get_start_tangent,
  gsk_quad_curve_get_end_tangent,
  gsk_quad_curve_get_point,
  gsk_quad_curve_get_tangent,
  gsk_quad_curve_reverse,
  gsk_quad_curve_get_curvature,
  gsk_quad_curve_split,
  gsk_quad_curve_segment,
  gsk_quad_curve_decompose,
  gsk_quad_curve_decompose_curve,
  gsk_quad_curve_get_bounds,
  gsk_quad_curve_get_tight_bounds,
};

/* }}} */
/* {{{ Cubic */

static void
gsk_cubic_curve_ensure_coefficients (const GskCubicCurve *curve)
{
  GskCubicCurve *self = (GskCubicCurve *) curve;
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
gsk_cubic_curve_init_from_points (GskCubicCurve          *self,
                                  const graphene_point_t  pts[4])
{
  self->op = GSK_PATH_CUBIC;
  self->has_coefficients = FALSE;
  memcpy (self->points, pts, sizeof (graphene_point_t) * 4);
}

static void
gsk_cubic_curve_init (GskCurve  *curve,
                      gskpathop  op)
{
  GskCubicCurve *self = &curve->cubic;

  gsk_cubic_curve_init_from_points (self, gsk_pathop_points (op));
}

static void
gsk_cubic_curve_init_foreach (GskCurve               *curve,
                              GskPathOperation        op,
                              const graphene_point_t *pts,
                              gsize                   n_pts)
{
  GskCubicCurve *self = &curve->cubic;

  g_assert (n_pts == 4);

  gsk_cubic_curve_init_from_points (self, pts);
}

static void
gsk_cubic_curve_print (const GskCurve *curve,
                       GString        *string)
{
  g_string_append_printf (string,
                          "M %f %f C %f %f %f %f %f %f",
                          curve->cubic.points[0].x, curve->cubic.points[0].y,
                          curve->cubic.points[1].x, curve->cubic.points[1].y,
                          curve->cubic.points[2].x, curve->cubic.points[2].y,
                          curve->cubic.points[3].x, curve->cubic.points[3].y);
}

static gskpathop
gsk_cubic_curve_pathop (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return gsk_pathop_encode (self->op, self->points);
}

static const graphene_point_t *
gsk_cubic_curve_get_start_point (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return &self->points[0];
}

static const graphene_point_t *
gsk_cubic_curve_get_end_point (const GskCurve *curve)
{
  const GskCubicCurve *self = &curve->cubic;

  return &self->points[3];
}

static void
gsk_cubic_curve_get_start_tangent (const GskCurve  *curve,
                                   graphene_vec2_t *tangent)
{
  const GskCubicCurve *self = &curve->cubic;

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
gsk_cubic_curve_get_end_tangent (const GskCurve  *curve,
                                 graphene_vec2_t *tangent)
{
  const GskCubicCurve *self = &curve->cubic;

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
gsk_cubic_curve_get_point (const GskCurve   *curve,
                           float             t,
                           graphene_point_t *pos)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *c = self->coeffs;

  gsk_cubic_curve_ensure_coefficients (self);

  *pos = GRAPHENE_POINT_INIT (((c[0].x * t + c[1].x) * t +c[2].x) * t + c[3].x,
                              ((c[0].y * t + c[1].y) * t +c[2].y) * t + c[3].y);
}

static void
gsk_cubic_curve_get_tangent (const GskCurve   *curve,
                             float             t,
                             graphene_vec2_t  *tangent)
{
  const GskCubicCurve *self = &curve->cubic;
  const graphene_point_t *c = self->coeffs;

  gsk_cubic_curve_ensure_coefficients (self);

  graphene_vec2_init (tangent,
                      (3.0f * c[0].x * t + 2.0f * c[1].x) * t + c[2].x,
                      (3.0f * c[0].y * t + 2.0f * c[1].y) * t + c[2].y);
  graphene_vec2_normalize (tangent, tangent);
}

static void
gsk_cubic_curve_reverse (const GskCurve *curve,
                         GskCurve       *reverse)
{
  const GskCubicCurve *self = &curve->cubic;

  reverse->op = GSK_PATH_CUBIC;
  reverse->cubic.points[0] = self->points[3];
  reverse->cubic.points[1] = self->points[2];
  reverse->cubic.points[2] = self->points[1];
  reverse->cubic.points[3] = self->points[0];
  reverse->cubic.has_coefficients = FALSE;
}

static void
gsk_curve_get_derivative (const GskCurve *curve,
                          GskCurve       *deriv)
{
  switch (curve->op)
    {
    case GSK_PATH_LINE:
      {
        const GskLineCurve *self = &curve->line;
        graphene_point_t p;

        p.x = self->points[1].x - self->points[0].x;
        p.y = self->points[1].y - self->points[0].y;

        gsk_line_curve_init_from_points (&deriv->line, GSK_PATH_LINE, &p, &p);
      }
      break;

    case GSK_PATH_QUAD:
      {
        const GskQuadCurve *self = &curve->quad;
        graphene_point_t p[2];

        p[0].x = 2.f * (self->points[1].x - self->points[0].x);
        p[0].y = 2.f * (self->points[1].y - self->points[0].y);
        p[1].x = 2.f * (self->points[2].x - self->points[1].x);
        p[1].y = 2.f * (self->points[2].y - self->points[1].y);

        gsk_line_curve_init_from_points (&deriv->line, GSK_PATH_LINE, &p[0], &p[1]);
      }
      break;

    case GSK_PATH_CUBIC:
      {
        const GskCubicCurve *self = &curve->cubic;
        graphene_point_t p[3];

        p[0].x = 3.f * (self->points[1].x - self->points[0].x);
        p[0].y = 3.f * (self->points[1].y - self->points[0].y);
        p[1].x = 3.f * (self->points[2].x - self->points[1].x);
        p[1].y = 3.f * (self->points[2].y - self->points[1].y);
        p[2].x = 3.f * (self->points[3].x - self->points[2].x);
        p[2].y = 3.f * (self->points[3].y - self->points[2].y);

        gsk_quad_curve_init_from_points (&deriv->quad, p);
      }
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

static inline float
cross (const graphene_vec2_t *v1,
       const graphene_vec2_t *v2)
{
  return graphene_vec2_get_x (v1) * graphene_vec2_get_y (v2)
         - graphene_vec2_get_y (v1) * graphene_vec2_get_x (v2);
}

static inline float
pow3 (float w)
{
  return w * w * w;
}

static float
gsk_cubic_curve_get_curvature (const GskCurve *curve,
                               float           t)
{
  GskCurve c1, c2;
  graphene_point_t p, pp;
  graphene_vec2_t d, dd;
  float num, denom;

  gsk_curve_get_derivative (curve, &c1);
  gsk_curve_get_derivative (&c1, &c2);

  gsk_curve_get_point (&c1, t, &p);
  gsk_curve_get_point (&c2, t, &pp);

  graphene_vec2_init (&d, p.x, p.y);
  graphene_vec2_init (&dd, pp.x, pp.y);

  num = cross (&d, &dd);
  if (num == 0)
    return 0;

  denom = pow3 (graphene_vec2_length (&d));
  if (denom == 0)
    return 0;

  return num / denom;
}

static void
gsk_cubic_curve_split (const GskCurve   *curve,
                       float             progress,
                       GskCurve         *start,
                       GskCurve         *end)
{
  const GskCubicCurve *self = &curve->cubic;
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
    gsk_cubic_curve_init_from_points (&start->cubic, (graphene_point_t[4]) { pts[0], ab, abbc, final });
  if (end)
    gsk_cubic_curve_init_from_points (&end->cubic, (graphene_point_t[4]) { final, bccd, cd, pts[3] });
}

static void
gsk_cubic_curve_segment (const GskCurve *curve,
                         float           start,
                         float           end,
                         GskCurve       *segment)
{
  GskCurve tmp;

  gsk_cubic_curve_split (curve, start, NULL, &tmp);
  gsk_cubic_curve_split (&tmp, (end - start) / (1.0f - start), segment, NULL);
}

/* taken from Skia, including the very descriptive name */
static gboolean
gsk_cubic_curve_too_curvy (const GskCubicCurve *self,
                           float                tolerance)
{
  const graphene_point_t *pts = self->points;
  graphene_point_t p;

  graphene_point_interpolate (&pts[0], &pts[3], 1.0f / 3, &p);
  if (MAX (ABS (p.x - pts[1].x), ABS (p.y - pts[1].y)) > tolerance)
    return TRUE;

  graphene_point_interpolate (&pts[0], &pts[3], 2.0f / 3, &p);
  if (MAX (ABS (p.x - pts[2].x), ABS (p.y - pts[2].y)) > tolerance)
    return TRUE;

  return FALSE;
}

static gboolean
gsk_cubic_curve_decompose_step (const GskCurve      *curve,
                                float                start_progress,
                                float                end_progress,
                                float                tolerance,
                                GskCurveAddLineFunc  add_line_func,
                                gpointer             user_data)
{
  const GskCubicCurve *self = &curve->cubic;
  GskCurve left, right;
  float mid_progress;

  if (!gsk_cubic_curve_too_curvy (self, tolerance))
    return add_line_func (&self->points[0], &self->points[3], start_progress, end_progress, GSK_CURVE_LINE_REASON_STRAIGHT, user_data);
  if (end_progress - start_progress <= MIN_PROGRESS)
    return add_line_func (&self->points[0], &self->points[3], start_progress, end_progress, GSK_CURVE_LINE_REASON_SHORT, user_data);

  gsk_cubic_curve_split ((const GskCurve *) self, 0.5, &left, &right);
  mid_progress = (start_progress + end_progress) / 2;

  return gsk_cubic_curve_decompose_step (&left, start_progress, mid_progress, tolerance, add_line_func, user_data)
      && gsk_cubic_curve_decompose_step (&right, mid_progress, end_progress, tolerance, add_line_func, user_data);
}

static gboolean
gsk_cubic_curve_decompose (const GskCurve      *curve,
                           float                tolerance,
                           GskCurveAddLineFunc  add_line_func,
                           gpointer             user_data)
{
  return gsk_cubic_curve_decompose_step (curve, 0.0, 1.0, tolerance, add_line_func, user_data);
}

static gboolean
gsk_cubic_curve_decompose_curve (const GskCurve       *curve,
                                 GskPathForeachFlags   flags,
                                 float                 tolerance,
                                 GskCurveAddCurveFunc  add_curve_func,
                                 gpointer              user_data)
{
  const GskCubicCurve *self = &curve->cubic;

  if (flags & GSK_PATH_FOREACH_ALLOW_CUBIC)
    return add_curve_func (GSK_PATH_CUBIC, self->points, 4, user_data);

  /* FIXME: Quadratic (or conic?) approximation */
  return gsk_cubic_curve_decompose (curve,
                                    tolerance,
                                    gsk_curve_add_line_cb,
                                    &(AddLineData) { add_curve_func, user_data });
}

static void
gsk_cubic_curve_get_bounds (const GskCurve  *curve,
                            GskBoundingBox  *bounds)
{
  const GskCubicCurve *self = &curve->cubic;
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
gsk_cubic_curve_get_tight_bounds (const GskCurve  *curve,
                                  GskBoundingBox  *bounds)
{
  const GskCubicCurve *self = &curve->cubic;
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

      gsk_cubic_curve_get_point (curve, t[i], &p);
      gsk_bounding_box_expand (bounds, &p);
    }
}

static const GskCurveClass GSK_CUBIC_CURVE_CLASS = {
  gsk_cubic_curve_init,
  gsk_cubic_curve_init_foreach,
  gsk_cubic_curve_print,
  gsk_cubic_curve_pathop,
  gsk_cubic_curve_get_start_point,
  gsk_cubic_curve_get_end_point,
  gsk_cubic_curve_get_start_tangent,
  gsk_cubic_curve_get_end_tangent,
  gsk_cubic_curve_get_point,
  gsk_cubic_curve_get_tangent,
  gsk_cubic_curve_reverse,
  gsk_cubic_curve_get_curvature,
  gsk_cubic_curve_split,
  gsk_cubic_curve_segment,
  gsk_cubic_curve_decompose,
  gsk_cubic_curve_decompose_curve,
  gsk_cubic_curve_get_bounds,
  gsk_cubic_curve_get_tight_bounds,
};

/* }}} */
/* {{{ API */

static const GskCurveClass *
get_class (GskPathOperation op)
{
  const GskCurveClass *klasses[] = {
    [GSK_PATH_CLOSE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_LINE] = &GSK_LINE_CURVE_CLASS,
    [GSK_PATH_QUAD] = &GSK_QUAD_CURVE_CLASS,
    [GSK_PATH_CUBIC] = &GSK_CUBIC_CURVE_CLASS,
  };

  g_assert (op < G_N_ELEMENTS (klasses) && klasses[op] != NULL);

  return klasses[op];
}

void
gsk_curve_init (GskCurve  *curve,
                gskpathop  op)
{
  memset (curve, 0, sizeof (GskCurve));
  get_class (gsk_pathop_op (op))->init (curve, op);
}

void
gsk_curve_init_foreach (GskCurve               *curve,
                        GskPathOperation        op,
                        const graphene_point_t *pts,
                        gsize                   n_pts)
{
  memset (curve, 0, sizeof (GskCurve));
  get_class (op)->init_foreach (curve, op, pts, n_pts);
}

void
gsk_curve_print (const GskCurve *curve,
                 GString        *string)
{
  get_class (curve->op)->print (curve, string);
}

char *
gsk_curve_to_string (const GskCurve *curve)
{
  GString *s = g_string_new ("");
  gsk_curve_print (curve, s);
  return g_string_free (s, FALSE);
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
      graphene_vec2_t tangent;
      float r;

      r = 1/k;
      gsk_curve_get_point (curve, t, &p);
      gsk_curve_get_tangent (curve, t, &tangent);
      center->x = p.x - r * graphene_vec2_get_y (&tangent);
      center->y = p.y + r * graphene_vec2_get_x (&tangent);
    }

  return k;
}

void
gsk_curve_reverse (const GskCurve *curve,
                   GskCurve       *reverse)
{
  get_class (curve->op)->reverse (curve, reverse);
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
                           GskPathForeachFlags   flags,
                           float                 tolerance,
                           GskCurveAddCurveFunc  add_curve_func,
                           gpointer              user_data)
{
  return get_class (curve->op)->decompose_curve (curve, flags, tolerance, add_curve_func, user_data);
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

static inline int
line_get_crossing (const graphene_point_t *p,
                   const graphene_point_t *p1,
                   const graphene_point_t *p2)
{
  if (p1->y <= p->y)
    {
      if (p2->y > p->y)
        {
          if ((p2->x - p1->x) * (p->y - p1->y) - (p->x - p1->x) * (p2->y - p1->y) > 0)
            return 1;
        }
    }
  else if (p2->y <= p->y)
    {
      if ((p2->x - p1->x) * (p->y - p1->y) - (p->x - p1->x) * (p2->y - p1->y) < 0)
        return -1;
    }

  return 0;
}

int
gsk_curve_get_crossing (const GskCurve         *curve,
                        const graphene_point_t *point)
{
  GskBoundingBox bounds;
  GskCurve c1, c2;

  if (curve->op == GSK_PATH_LINE || curve->op == GSK_PATH_CLOSE)
    return line_get_crossing (point, gsk_curve_get_start_point (curve), gsk_curve_get_end_point (curve));

  gsk_curve_get_bounds (curve, &bounds);

  if (bounds.max.y < point->y || bounds.min.y > point->y || bounds.max.x < point->x)
    return 0;

  if (bounds.min.x > point->x)
    return line_get_crossing (point, gsk_curve_get_start_point (curve), gsk_curve_get_end_point (curve));

  if (graphene_point_distance (&bounds.min, &bounds.max, NULL, NULL) < 0.001)
    return line_get_crossing (point, gsk_curve_get_start_point (curve), gsk_curve_get_end_point (curve));

  gsk_curve_split (curve, 0.5, &c1, &c2);

  return gsk_curve_get_crossing (&c1, point) + gsk_curve_get_crossing (&c2, point);
}

static gboolean
project_point_onto_line (const GskCurve         *curve,
                         const graphene_point_t *point,
                         float                   threshold,
                         float                  *out_distance,
                         float                  *out_t)
{
  const graphene_point_t *a = gsk_curve_get_start_point (curve);
  const graphene_point_t *b = gsk_curve_get_end_point (curve);
  graphene_vec2_t n, ap;
  graphene_point_t p;

  if (graphene_point_equal (a, b))
    {
      *out_t = 0;
      *out_distance = graphene_point_distance (point, a, NULL, NULL);
    }
  else
    {
      graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
      graphene_vec2_init (&ap, point->x - a->x, point->y - a->y);

      *out_t = graphene_vec2_dot (&n, &ap) / graphene_vec2_dot (&n, &n);
      *out_t = CLAMP (*out_t, 0, 1);

      graphene_point_interpolate (a, b, *out_t, &p);
      *out_distance = graphene_point_distance (point, &p, NULL, NULL);
    }

  return *out_distance <= threshold;
}

static float
get_segment_bounding_sphere (const GskCurve  *curve,
                             float            t1,
                             float            t2,
                             graphene_point_t *center)
{
  GskCurve c;
  GskBoundingBox bounds;

  gsk_curve_segment (curve, t1, t2, &c);
  gsk_curve_get_tight_bounds (&c, &bounds);
  graphene_point_interpolate (&bounds.min, &bounds.max, 0.5, center);
  return graphene_point_distance (center, &bounds.min, NULL, NULL);
}

static gboolean
find_closest_point (const GskCurve         *curve,
                    const graphene_point_t *point,
                    float                   threshold,
                    float                   t1,
                    float                   t2,
                    float                  *out_distance,
                    float                  *out_t)
{
  graphene_point_t center;
  float radius;
  float t, d, nt;

  radius = get_segment_bounding_sphere (curve, t1, t2, &center);
  if (graphene_point_distance (&center, point, NULL, NULL) > threshold + radius)
    return FALSE;

  d = INFINITY;
  t = (t1 + t2) / 2;

  if (radius < 1)
    {
      graphene_point_t p;
      gsk_curve_get_point (curve, t, &p);
      d = graphene_point_distance (point, &p, NULL, NULL);
      nt = t;
    }
  else
    {
      float dd, tt;

      dd = INFINITY;
      nt = 0;

      if (find_closest_point (curve, point, threshold, t1, t, &dd, &tt))
        {
          d = dd;
          nt = tt;
        }

      if (find_closest_point (curve, point, MIN (dd, threshold), t, t2, &dd, &tt))
        {
          d = dd;
          nt = tt;
        }
    }

  if (d < threshold)
    {
      *out_distance = d;
      *out_t = nt;
      return TRUE;
    }
  else
    {
      *out_distance = INFINITY;
      *out_t = 0;
      return FALSE;
    }
}

gboolean
gsk_curve_get_closest_point (const GskCurve         *curve,
                             const graphene_point_t *point,
                             float                   threshold,
                             float                  *out_dist,
                             float                  *out_t)
{
  if (curve->op == GSK_PATH_LINE || curve->op == GSK_PATH_CLOSE)
    return project_point_onto_line (curve, point, threshold, out_dist, out_t);
  else
    return find_closest_point (curve, point, threshold, 0, 1, out_dist, out_t);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
