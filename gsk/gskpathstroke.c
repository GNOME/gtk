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

#include "gskpathprivate.h"

#include "gskpathbuilder.h"

#include "gskstrokeprivate.h"
#include "gskcurveprivate.h"
#include "gskpathdashprivate.h"
#include "gskpathopprivate.h"

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

typedef struct
{
  GskPathBuilder *builder;
  GskPathBuilder *left;
  GskPathBuilder *right;
  GskStroke *stroke;
  gboolean has_current_point;
  GskCurve c;
  GskCurve l;
  GskCurve r;

  graphene_point_t l0;
  graphene_point_t r0;
} StrokeData;

static void
gsk_path_builder_move_to_point (GskPathBuilder         *builder,
                                const graphene_point_t *point)
{
  gsk_path_builder_move_to (builder, point->x, point->y);
}

static void
gsk_path_builder_line_to_point (GskPathBuilder         *builder,
                                const graphene_point_t *point)
{
  gsk_path_builder_line_to (builder, point->x, point->y);
}

static void
gsk_path_builder_add_curve (GskPathBuilder *builder,
                            const GskCurve *curve)
{
  const graphene_point_t *p;

  switch (curve->op)
    {
    case GSK_PATH_LINE:
      p = curve->line.points;
      gsk_path_builder_line_to (builder, p[1].x, p[1].y);
      break;

    case GSK_PATH_CURVE:
      p = curve->curve.points;
      gsk_path_builder_curve_to (builder, p[1].x, p[1].y,
                                          p[2].x, p[2].y,
                                          p[3].x, p[3].y);
      break;

    case GSK_PATH_CONIC:
      p = curve->conic.points;
      gsk_path_builder_conic_to (builder, p[1].x, p[1].y,
                                          p[3].x, p[3].y,
                                          p[2].x);
      break;

    case GSK_PATH_MOVE:
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }
}

static gboolean
add_op (GskPathOperation        op,
        const graphene_point_t *pts,
        gsize                   n_pts,
        float                   weight,
        gpointer                user_data)
{
  GskCurve *curve;
  GList **ops = user_data;

  curve = g_new (GskCurve, 1);
  switch (op)
    {
    case GSK_PATH_MOVE:
      return TRUE;
    case GSK_PATH_LINE:
      gsk_curve_init (curve, gsk_pathop_encode (op, (const graphene_point_t[2]) { pts[1], pts[0] }));
      break;
    case GSK_PATH_CURVE:
      gsk_curve_init (curve, gsk_pathop_encode (op, (const graphene_point_t[4]) { pts[3], pts[2], pts[1], pts[0] }));
      break;
    case GSK_PATH_CONIC:
      gsk_curve_init (curve, gsk_pathop_encode (op, (const graphene_point_t[4]) { pts[3], pts[1], pts[2], pts[0] }));
      break;
    case GSK_PATH_CLOSE:
    default:
      g_assert_not_reached ();
    }

  *ops = g_list_prepend (*ops, curve);

  return TRUE;
}

static void
gsk_path_builder_add_reverse_path (GskPathBuilder *builder,
                                   GskPath        *path)
{
  GList *l, *ops;

  ops = NULL;
  gsk_path_foreach (path,
                    GSK_PATH_FOREACH_ALLOW_CURVE | GSK_PATH_FOREACH_ALLOW_CONIC,
                    add_op,
                    &ops);
  for (l = ops; l; l = l->next)
    {
      GskCurve *curve = l->data;
      gsk_path_builder_add_curve (builder, curve);
    }
  g_list_free_full (ops, g_free);
}

static float
angle_between (const graphene_vec2_t *t1,
               const graphene_vec2_t *t2)
{
  float angle = atan2 (graphene_vec2_get_y (t2), graphene_vec2_get_x (t2))
                - atan2 (graphene_vec2_get_y (t1), graphene_vec2_get_x (t1));

  if (angle > M_PI)
    angle -= 2 * M_PI;
  if (angle < - M_PI)
    angle += 2 * M_PI;

  return angle;
}

static int
line_intersect (const graphene_point_t *a,
                const graphene_vec2_t  *ab,
                const graphene_point_t *c,
                const graphene_vec2_t  *cd,
                graphene_point_t       *p)
{
  float a1 = graphene_vec2_get_y (ab);
  float b1 = - graphene_vec2_get_x (ab);
  float c1 = a1 * a->x + b1 * a->y;

  float a2 = graphene_vec2_get_y (cd);
  float b2 = - graphene_vec2_get_x (cd);
  float c2 = a2 * c->x + b2 * c->y;

  float det = a1 * b2 - a2 * b1;

  if (det == 0)
    return 0;

  p->x = (b2 * c1 - b1 * c2) / det;
  p->y = (a1 * c2 - a2 * c1) / det;

  return 1;
}

#define RAD_TO_DEG(r) ((r)*180.0/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.0)

static void
add_line_join (GskPathBuilder         *builder,
               GskStroke              *stroke,
               const graphene_point_t *c,
               const graphene_point_t *a,
               const graphene_vec2_t  *ta,
               const graphene_point_t *b,
               const graphene_vec2_t  *tb,
               float                   angle)
{
  switch (stroke->line_join)
    {
    case GSK_LINE_JOIN_MITER:
    case GSK_LINE_JOIN_MITER_CLIP:
      {
        graphene_point_t p;

        if (line_intersect (a, ta, b, tb, &p))
          {
            if (fabs (1 / sin ((angle + M_PI) / 2)) <= stroke->miter_limit)
              {
                gsk_path_builder_line_to_point (builder, &p);
                gsk_path_builder_line_to_point (builder, b);
              }
            else if (stroke->line_join == GSK_LINE_JOIN_MITER_CLIP)
              {
                graphene_point_t q, a1, b1;
                graphene_vec2_t t, n;

                get_tangent (c, &p, &t);
                scale_point (c, &t, stroke->line_width * stroke->miter_limit / 2, &q);
                get_normal (c, &p, &n);

                line_intersect (a, ta, &q, &n, &a1);
                line_intersect (b, tb, &q, &n, &b1);

                gsk_path_builder_line_to_point (builder, &a1);
                gsk_path_builder_line_to_point (builder, &b1);
                gsk_path_builder_line_to_point (builder, b);
              }
            else
              {
                gsk_path_builder_line_to_point (builder, b);
              }
          }
      }
      break;

    case GSK_LINE_JOIN_ROUND:
      gsk_path_builder_svg_arc_to (builder,
                                   stroke->line_width / 2, stroke->line_width / 2,
                                   0, 0, angle > 0 ? 1 : 0,
                                   b->x, b->y);
      break;

    case GSK_LINE_JOIN_BEVEL:
      gsk_path_builder_line_to_point (builder, b);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
add_curve (StrokeData     *stroke_data,
           const GskCurve *curve)
{
  GskCurve l, r;

  gsk_curve_offset (curve, - stroke_data->stroke->line_width / 2, &l);
  gsk_curve_offset (curve, stroke_data->stroke->line_width / 2, &r);

  if (!stroke_data->has_current_point)
    {
      stroke_data->r0 = *gsk_curve_get_start_point (&r);
      stroke_data->l0 = *gsk_curve_get_start_point (&l);
      gsk_path_builder_move_to_point (stroke_data->right, &stroke_data->r0);
      gsk_path_builder_move_to_point (stroke_data->left, &stroke_data->l0);
      stroke_data->has_current_point = TRUE;
    }
  else
    {
      float angle;
      float t1, t2;
      graphene_vec2_t tangent1, tangent2;
      graphene_point_t p;

      gsk_curve_get_end_tangent (&stroke_data->c, &tangent1);
      gsk_curve_get_start_tangent (curve, &tangent2);
      angle = angle_between (&tangent1, &tangent2);

      if (fabs (angle) < DEG_TO_RAD (5))
        {
          /* Close enough to a straight line */
          gsk_path_builder_add_curve (stroke_data->right, &stroke_data->r);
          gsk_path_builder_line_to_point (stroke_data->right, gsk_curve_get_start_point (&r));

          gsk_path_builder_add_curve (stroke_data->left, &stroke_data->l);
          gsk_path_builder_line_to_point (stroke_data->left, gsk_curve_get_start_point (&l));
        }
      else if (angle > 0)
        {
          /* Right turn */
          if (gsk_curve_intersect (&stroke_data->r, &r, &t1, &t2, &p, 1) > 0)
            {
              GskCurve c1, c2;

              gsk_curve_split (&stroke_data->r, t1, &c1, &c2);
              stroke_data->r = c1;
              gsk_curve_split (&r, t2, &c1, &c2);
              r = c2;

              gsk_path_builder_add_curve (stroke_data->right, &stroke_data->r);
            }
          else
            {
              gsk_path_builder_add_curve (stroke_data->right, &stroke_data->r);
              gsk_path_builder_line_to_point (stroke_data->right,
                                              gsk_curve_get_start_point (&r));
            }

          gsk_path_builder_add_curve (stroke_data->left, &stroke_data->l);

          add_line_join (stroke_data->left,
                         stroke_data->stroke,
                         gsk_curve_get_start_point (curve),
                         gsk_curve_get_end_point (&stroke_data->l),
                         &tangent1,
                         gsk_curve_get_start_point (&l),
                         &tangent2,
                         angle);
        }
      else
        {
          /* Left turn */
          gsk_path_builder_add_curve (stroke_data->right, &stroke_data->r);

          add_line_join (stroke_data->right,
                         stroke_data->stroke,
                         gsk_curve_get_start_point (curve),
                         gsk_curve_get_end_point (&stroke_data->r),
                         &tangent1,
                         gsk_curve_get_start_point (&r),
                         &tangent2,
                         angle);

          if (gsk_curve_intersect (&stroke_data->l, &l, &t1, &t2, &p, 1) > 0)
            {
              GskCurve c1, c2;

              gsk_curve_split (&stroke_data->l, t1, &c1, &c2);
              stroke_data->l = c1;
              gsk_curve_split (&l, t2, &c1, &c2);
              l = c2;

              gsk_path_builder_add_curve (stroke_data->left, &stroke_data->l);
            }
          else
            {
              gsk_path_builder_add_curve (stroke_data->left, &stroke_data->l);
              gsk_path_builder_line_to_point (stroke_data->left,
                                               gsk_curve_get_start_point (&l));
            }
        }
    }

  stroke_data->c = *curve;
  stroke_data->r = r;
  stroke_data->l = l;
}

static gboolean
cubic_is_simple (const GskCurve *curve)
{
  const graphene_point_t *pts = curve->curve.points;
  float a1, a2, s;
  graphene_vec2_t t1, t2, t3;
  graphene_vec2_t n1, n2;

  get_tangent (&pts[0], &pts[1], &t1);
  get_tangent (&pts[1], &pts[2], &t2);
  get_tangent (&pts[2], &pts[3], &t3);
  a1 = angle_between (&t1, &t2);
  a2 = angle_between (&t2, &t3);

  if ((a1 < 0 && a2 > 0) || (a1 > 0 && a2 < 0))
    return FALSE;

  get_normal (&pts[0], &pts[1], &n1);
  get_normal (&pts[2], &pts[3], &n2);

  s = graphene_vec2_dot (&n1, &n2);

  if (fabs (acos (s)) >= M_PI / 3.f)
    return FALSE;

  return TRUE;
}

static void
subdivide_and_add_curve (StrokeData     *stroke_data,
                         const GskCurve *curve,
                         int             level)
{
  if (level == 0 || cubic_is_simple (curve))
    {
      add_curve (stroke_data, curve);
    }
  else
    {
      GskCurve c1, c2;

      gsk_curve_split (curve, 0.5, &c1, &c2);
      subdivide_and_add_curve (stroke_data, &c1, level - 1);
      subdivide_and_add_curve (stroke_data, &c2, level - 1);
    }
}

static void
subdivide_and_add_conic (StrokeData     *stroke_data,
                         const GskCurve *curve)
{
  GskCurve c1, c2;

  /* FIXME make this adaptive */
  gsk_curve_split (curve, 0.5, &c1, &c2);
  add_curve (stroke_data, &c1);
  add_curve (stroke_data, &c2);
}

static void
add_line_cap (GskPathBuilder         *builder,
              GskStroke              *stroke,
              const graphene_point_t *s,
              const graphene_point_t *e)
{
    switch (stroke->line_cap)
    {
    case GSK_LINE_CAP_BUTT:
      gsk_path_builder_line_to_point (builder, e);
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
        gsk_path_builder_line_to_point (builder, e);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
close_stroke (StrokeData *stroke_data)
{
  GskPath *path;

  gsk_path_builder_add_curve (stroke_data->right, &stroke_data->r);
  gsk_path_builder_add_curve (stroke_data->left, &stroke_data->l);

  add_line_cap (stroke_data->right,
                stroke_data->stroke,
                gsk_curve_get_end_point (&stroke_data->r),
                gsk_curve_get_end_point (&stroke_data->l));

  path = gsk_path_builder_free_to_path (stroke_data->left);
  gsk_path_builder_add_reverse_path (stroke_data->right, path);
  gsk_path_unref (path);

  add_line_cap (stroke_data->right,
                stroke_data->stroke,
                &stroke_data->l0,
                &stroke_data->r0);

  gsk_path_builder_close (stroke_data->right);

  path = gsk_path_builder_free_to_path (stroke_data->right);
  gsk_path_builder_add_path (stroke_data->builder, path);
  gsk_path_unref (path);

  stroke_data->left = NULL;
  stroke_data->right = NULL;
}

static gboolean
stroke_op (GskPathOperation        op,
           const graphene_point_t *pts,
           gsize                   n_pts,
           float                   weight,
           gpointer                user_data)
{
  StrokeData *stroke_data = user_data;
  GskCurve curve;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (stroke_data->has_current_point)
        {
          close_stroke (stroke_data);

          stroke_data->right = gsk_path_builder_new ();
          stroke_data->left = gsk_path_builder_new ();
        }
      stroke_data->has_current_point = FALSE;
      break;

    case GSK_PATH_CLOSE:
      gsk_path_builder_close (stroke_data->right);
      gsk_path_builder_close (stroke_data->left);
      stroke_data->has_current_point = FALSE;
      break;

    case GSK_PATH_LINE:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      add_curve (stroke_data, &curve);
      break;

    case GSK_PATH_CURVE:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      subdivide_and_add_curve (stroke_data, &curve, 2);
      break;

    case GSK_PATH_CONIC:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      subdivide_and_add_conic (stroke_data, &curve);
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
  StrokeData stroke_data;

  stroke_data.builder = builder;
  stroke_data.stroke = stroke;
  stroke_data.left = gsk_path_builder_new ();
  stroke_data.right = gsk_path_builder_new ();
  stroke_data.has_current_point = FALSE;

  if (stroke->dash_length <= 0)
    gsk_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, stroke_op, &stroke_data);
  else
    gsk_contour_dash (contour, stroke, GSK_PATH_TOLERANCE_DEFAULT, stroke_op, &stroke_data);

  if (stroke_data.has_current_point)
    close_stroke (&stroke_data);
}
