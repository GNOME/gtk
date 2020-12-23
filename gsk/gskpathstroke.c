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

#define STROKE_DEBUG

#define RAD_TO_DEG(r) ((r)*180.0/M_PI)
#define DEG_TO_RAD(d) ((d)*M_PI/180.0)

/* {{{ graphene utilities */

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

/* Return the angle between t1 and t2 in radians, such that
 * 0 means straight continuation
 * < 0 means right turn
 * > 0 means left turn
 */
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

/* Set p to the intersection of the lines a + t * ab * and
 * c + s * cd. Return 1 if the lines intersect, 0 otherwise.
 */
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

  if (fabs (det) <= 0.001)
    return 0;

  p->x = (b2 * c1 - b1 * c2) / det;
  p->y = (a1 * c2 - a2 * c1) / det;

  return 1;
}

static float
line_point_dist (const graphene_point_t *a,
                 const graphene_vec2_t  *n,
                 const graphene_point_t *q)
{
  graphene_vec2_t t;

  graphene_vec2_init (&t, q->x - a->x, q->y - a->y);
  return graphene_vec2_dot (n, &t);
}

/* }}} */
 /* {{{ GskPathBuilder utilities */

static void
path_builder_move_to_point (GskPathBuilder         *builder,
                            const graphene_point_t *point)
{
  gsk_path_builder_move_to (builder, point->x, point->y);
}

static void
path_builder_line_to_point (GskPathBuilder         *builder,
                            const graphene_point_t *point)
{
  gsk_path_builder_line_to (builder, point->x, point->y);
}

/* Assumes that the current point of the builder is
 * the start point of the curve
 */
static void
path_builder_add_curve (GskPathBuilder *builder,
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
  GskCurve c;
  GskCurve *curve;
  GList **ops = user_data;

  if (op == GSK_PATH_MOVE)
    return TRUE;

  gsk_curve_init_foreach (&c, op, pts, n_pts, weight);
  curve = g_new0 (GskCurve, 1);
  gsk_curve_reverse (&c, curve);

  *ops = g_list_prepend (*ops, curve);

  return TRUE;
}

static void
path_builder_add_reverse_path (GskPathBuilder *builder,
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
      path_builder_add_curve (builder, curve);
    }
  g_list_free_full (ops, g_free);
}

/* }}} */
/* {{{ GskCurve utilities */

static gboolean
curve_is_ignorable (const GskCurve *curve)
{
  if (curve->op == GSK_PATH_CURVE)
    {
      const graphene_point_t *pts = curve->curve.points;

      if (graphene_point_near (&pts[0], &pts[1], 0.001) &&
          graphene_point_near (&pts[1], &pts[2], 0.001) &&
          graphene_point_near (&pts[2], &pts[3], 0.001))
        return TRUE;
    }
  else if (curve->op == GSK_PATH_CONIC)
    {
      const graphene_point_t *pts = curve->conic.points;

      if (graphene_point_near (&pts[0], &pts[1], 0.001) &&
          graphene_point_near (&pts[1], &pts[3], 0.001))
        return TRUE;
    }
  else if (curve->op == GSK_PATH_LINE)
    {
      const graphene_point_t *pts = curve->line.points;

      if (graphene_point_near (&pts[0], &pts[1], 0.001))
        return TRUE;
    }

  return FALSE;
}

static gboolean
cubic_is_simple (const GskCurve *curve)
{
  const graphene_point_t *pts = curve->curve.points;
  float a1, a2, s;
  graphene_vec2_t t1, t2, t3;

  if (!graphene_point_near (&pts[0], &pts[1], 0.001) &&
      !graphene_point_near (&pts[1], &pts[2], 0.001) &&
      !graphene_point_near (&pts[2], &pts[3], 0.001))
    {
      get_tangent (&pts[0], &pts[1], &t1);
      get_tangent (&pts[1], &pts[2], &t2);
      get_tangent (&pts[2], &pts[3], &t3);
      a1 = angle_between (&t1, &t2);
      a2 = angle_between (&t2, &t3);

      if ((a1 < 0 && a2 > 0) || (a1 > 0 && a2 < 0))
        return FALSE;
    }

  gsk_curve_get_start_tangent (curve, &t1);
  gsk_curve_get_end_tangent (curve, &t2);
  s = graphene_vec2_dot (&t1, &t2);

  if (fabs (acos (s)) >= M_PI / 3.f)
    return FALSE;

  return TRUE;
}

static gboolean
conic_is_simple (const GskCurve *curve)
{
  const graphene_point_t *pts = curve->conic.points;
  graphene_vec2_t n1, n2;
  float s;

  get_normal (&pts[0], &pts[1], &n1);
  get_normal (&pts[1], &pts[3], &n2);

  s = graphene_vec2_dot (&n1, &n2);

  if (fabs (acos (s)) >= M_PI / 3.f)
    return FALSE;

  return TRUE;
}

static gboolean
conic_is_degenerate (const GskCurve *curve)
{
  if (curve->op == GSK_PATH_CONIC)
    {
      const graphene_point_t *pts = curve->curve.points;
      float a;
      graphene_vec2_t t1, t2;

      get_tangent (&pts[0], &pts[1], &t1);
      get_tangent (&pts[1], &pts[3], &t2);
      a = angle_between (&t1, &t2);

      if (a < 0)
        a += M_PI;

      if (fabs (a) < DEG_TO_RAD (3))
        return TRUE;
    }

  return FALSE;
}

/* }}} */
/* {{{ Stroke helpers */

static void
add_line_join (GskPathBuilder         *builder,
               GskLineJoin             line_join,
               float                   line_width,
               float                   miter_limit,
               const graphene_point_t *c,
               const GskCurve         *aa,
               const GskCurve         *bb,
               float                   angle)
{
  const graphene_point_t *a;
  const graphene_point_t *b;
  graphene_vec2_t ta;
  graphene_vec2_t tb;

  a = gsk_curve_get_end_point (aa);
  gsk_curve_get_end_tangent (aa, &ta);
  b = gsk_curve_get_start_point (bb);
  gsk_curve_get_start_tangent (bb, &tb);

  switch (line_join)
    {
    case GSK_LINE_JOIN_ARCS:
    case GSK_LINE_JOIN_MITER:
    case GSK_LINE_JOIN_MITER_CLIP:
      {
        graphene_point_t p;

        if (line_intersect (a, &ta, b, &tb, &p))
          {
            graphene_vec2_t tam;

            graphene_vec2_negate (&ta, &tam);

            /* Check that 1 / sin (psi / 2) <= miter_limit,
             * where psi is the angle between ta and tb
             */
            if (2 <= miter_limit * miter_limit * (1 - graphene_vec2_dot (&tam, &tb)))
              {
                path_builder_line_to_point (builder, &p);
                path_builder_line_to_point (builder, b);
              }
            else if (line_join == GSK_LINE_JOIN_MITER_CLIP)
              {
                graphene_point_t q, a1, b1;
                graphene_vec2_t n;

                q.x = (c->x + p.x) / 2;
                q.y = (c->y + p.y) / 2;
                get_normal (c, &p, &n);

                line_intersect (a, &ta, &q, &n, &a1);
                line_intersect (b, &tb, &q, &n, &b1);

                if ((line_point_dist (&q, &n, c) < 0) !=
                    (line_point_dist (&q, &n, a) < 0))
                  {
                    path_builder_line_to_point (builder, &a1);
                    path_builder_line_to_point (builder, &b1);
                    path_builder_line_to_point (builder, b);
                  }
                else
                  {
                    path_builder_line_to_point (builder, b);
                  }
              }
            else
              {
                path_builder_line_to_point (builder, b);
              }
          }
        else
          {
            path_builder_line_to_point (builder, b);
          }
      }
      break;

    case GSK_LINE_JOIN_ROUND:
      gsk_path_builder_svg_arc_to (builder,
                                   line_width / 2, line_width / 2,
                                   0, 0, angle > 0 ? 1 : 0,
                                   b->x, b->y);
      break;

    case GSK_LINE_JOIN_BEVEL:
      path_builder_line_to_point (builder, b);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
add_line_cap (GskPathBuilder         *builder,
              GskLineCap              line_cap,
              float                   line_width,
              const graphene_point_t *s,
              const graphene_point_t *e)
{
    switch (line_cap)
    {
    case GSK_LINE_CAP_BUTT:
      path_builder_line_to_point (builder, e);
      break;

    case GSK_LINE_CAP_ROUND:
      gsk_path_builder_svg_arc_to (builder,
                                   line_width / 2, line_width / 2,
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
        path_builder_line_to_point (builder, e);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

/* }}} */
/* {{{ Stroke implementation */

/* The general theory of operation for the stroker:
 *
 * We walk the segments of the path, offsetting each segment
 * to the left and right, and collect the offset segments in
 * a left and a right contour.
 *
 * When the segment is too curvy, or has cusps or inflections,
 * we subdivide it before we add the pieces.
 *
 * Whenever we add a segment, we need to decide if the join
 * is a smooth connection, a right turn, or a left turn. For
 * sharp turns, we add a line join on the one side, and intersect
 * the offset curves on the other.
 *
 * Since the intersection shortens both segments, we have to
 * delay adding the previous segments to the outlines until
 * we've handled the join at their end. We also need to hold
 * off on adding the initial segment to the outlines until
 * we've seen the end of the current contour of the path, to
 * handle the join at before the initial segment for closed
 * contours.
 *
 * If the contour turns out to not be closed when we reach
 * the end, we collect the pending segments, reverse the
 * left contour, and connect the right and left contour
 * with end caps, closing the resulting outline.
 *
 * If the path isn't done after we've finished handling the
 * outlines of the current contour, we start over with
 * collecting offset segments of the next contour.
 *
 * We rely on the ability to offset, subdivide, intersect
 * and reverse curves.
 */
typedef struct
{
  GskPathBuilder *builder;  // builder that collects the stroke
  GskStroke *stroke;  // stroke parameters

  GskPathBuilder *left;  // accumulates the left contour
  GskPathBuilder *right; // accumulates the right contour

  gboolean has_current_point;  // r0, l0 have been set from a move
  gboolean has_current_curve;  // c, l, r are set from a curve
  gboolean is_first_curve; // if c, l, r are the first segments we've seen

  GskCurve c;  // previous segment of the path
  GskCurve l;  // candidate for left contour of c
  GskCurve r;  // candidate for right contour of c

  GskCurve c0; // first segment of the path
  GskCurve l0; // first segment of left contour
  GskCurve r0; // first segment of right contour

#ifdef STROKE_DEBUG
  GskPathBuilder *debug;
#endif
} StrokeData;

static void
append_right (StrokeData     *stroke_data,
              const GskCurve *curve)
{
  if (stroke_data->is_first_curve)
    {
      stroke_data->r0 = *curve;
      path_builder_move_to_point (stroke_data->right, gsk_curve_get_end_point (curve));
    }
  else
    path_builder_add_curve (stroke_data->right, curve);
}

static void
append_left (StrokeData     *stroke_data,
             const GskCurve *curve)
{
  if (stroke_data->is_first_curve)
    {
      stroke_data->l0 = *curve;
      path_builder_move_to_point (stroke_data->left, gsk_curve_get_end_point (curve));
    }
  else
    path_builder_add_curve (stroke_data->left, curve);
}

/* Add the previous segments, stroke_data->l and ->r, and the join between
 * stroke_data->c and curve and update stroke_data->l, ->r and ->c to point
 * to the given curves.
 *
 * If stroke_data->c is the first segment of the contour, we don't add it
 * yet, but save it in stroke_data->c0, ->r0 and ->l0 for later when we
 * know if the contour is closed or not.
 */
static void
add_segments (StrokeData     *stroke_data,
              const GskCurve *curve,
              GskCurve       *r,
              GskCurve       *l,
              gboolean        force_round_join)
{
  float angle;
  float t1, t2;
  graphene_vec2_t tangent1, tangent2;
  graphene_point_t p;
  GskLineJoin line_join;

  if (!stroke_data->has_current_curve)
    {
      stroke_data->c0 = *curve;
      stroke_data->r0 = *r;
      stroke_data->l0 = *l;
      path_builder_move_to_point (stroke_data->right, gsk_curve_get_start_point (r));
      path_builder_move_to_point (stroke_data->left, gsk_curve_get_start_point (l));

      stroke_data->c = *curve;
      stroke_data->r = *r;
      stroke_data->l = *l;

      stroke_data->has_current_curve = TRUE;
      stroke_data->is_first_curve = TRUE;
      return;
    }

  gsk_curve_get_end_tangent (&stroke_data->c, &tangent1);
  gsk_curve_get_start_tangent (curve, &tangent2);
  angle = angle_between (&tangent1, &tangent2);

  if (force_round_join || fabs (angle) < DEG_TO_RAD (5))
    line_join = GSK_LINE_JOIN_ROUND;
  else
    line_join = stroke_data->stroke->line_join;

  if (fabs (angle) < DEG_TO_RAD (1))
    {
      /* Close enough to a straight line */
      append_right (stroke_data, &stroke_data->r);
      append_left (stroke_data, &stroke_data->l);
    }
  else
    {
      if (fabs (M_PI - fabs (angle)) < DEG_TO_RAD (1))
        {
          /* For 180 turns, we look at the whole curves to
           * decide if they are left or right turns
           */
          get_tangent (gsk_curve_get_start_point (&stroke_data->c),
                       gsk_curve_get_end_point (&stroke_data->c),
                       &tangent1);
          get_tangent (gsk_curve_get_start_point (curve),
                       gsk_curve_get_end_point (curve),
                       &tangent2);
          angle = angle_between (&tangent1, &tangent2);
        }

      if (angle > 0)
        {
          /* Right turn */
          if (gsk_curve_intersect (&stroke_data->r, r, &t1, &t2, &p, 1) > 0)
            {
              GskCurve c1, c2;

              gsk_curve_split (&stroke_data->r, t1, &c1, &c2);
              stroke_data->r = c1;
              gsk_curve_split (r, t2, &c1, &c2);
              *r = c2;

              append_right (stroke_data, &stroke_data->r);
            }
          else
            {
              append_right (stroke_data, &stroke_data->r);
              path_builder_line_to_point (stroke_data->right, gsk_curve_get_start_point (r));
            }

          append_left (stroke_data, &stroke_data->l);

          add_line_join (stroke_data->left,
                         line_join,
                         stroke_data->stroke->line_width,
                         stroke_data->stroke->miter_limit,
                         gsk_curve_get_start_point (curve),
                         &stroke_data->l,
                         l,
                         angle);
        }
      else
        {
          /* Left turn */
          append_right (stroke_data, &stroke_data->r);

          add_line_join (stroke_data->right,
                         line_join,
                         stroke_data->stroke->line_width,
                         stroke_data->stroke->miter_limit,
                         gsk_curve_get_start_point (curve),
                         &stroke_data->r,
                         r,
                         angle);

          if (gsk_curve_intersect (&stroke_data->l, l, &t1, &t2, &p, 1) > 0)
            {
              GskCurve c1, c2;

              gsk_curve_split (&stroke_data->l, t1, &c1, &c2);
              stroke_data->l = c1;
              gsk_curve_split (l, t2, &c1, &c2);
              *l = c2;

              append_left (stroke_data, &stroke_data->l);
            }
          else
            {
              append_left (stroke_data, &stroke_data->l);
              path_builder_line_to_point (stroke_data->left, gsk_curve_get_start_point (l));
            }
        }
    }

  stroke_data->c = *curve;
  stroke_data->r = *r;
  stroke_data->l = *l;
  stroke_data->is_first_curve = FALSE;
}

#ifdef STROKE_DEBUG
static void
add_debug (StrokeData     *stroke_data,
           const GskCurve *curve)
{
  const graphene_point_t *p;

  if (g_getenv ("STROKE_DEBUG"))
    {
      switch (curve->op)
        {
        case GSK_PATH_LINE:
        case GSK_PATH_CLOSE:
          p = curve->line.points;
          gsk_path_builder_add_circle (stroke_data->debug, &p[0], 3);
          gsk_path_builder_add_circle (stroke_data->debug, &p[1], 3);
          break;
        case GSK_PATH_CURVE:
          p = curve->curve.points;
          path_builder_move_to_point (stroke_data->debug, &p[0]);
          path_builder_line_to_point (stroke_data->debug, &p[1]);
          path_builder_line_to_point (stroke_data->debug, &p[2]);
          path_builder_line_to_point (stroke_data->debug, &p[3]);
          gsk_path_builder_add_circle (stroke_data->debug, &p[0], 3);
          gsk_path_builder_add_circle (stroke_data->debug, &p[1], 2);
          gsk_path_builder_add_circle (stroke_data->debug, &p[2], 2);
          gsk_path_builder_add_circle (stroke_data->debug, &p[3], 3);
          break;
        case GSK_PATH_CONIC:
          p = curve->conic.points;
          gsk_path_builder_add_circle (stroke_data->debug, &p[0], 3);
          gsk_path_builder_add_circle (stroke_data->debug, &p[3], 3);
          break;
        case GSK_PATH_MOVE:
        default:
          g_assert_not_reached ();
        }
    }
}
#endif

/* Add a curve to the in-progress stroke. We look at the angle between
 * the previous curve and this one to determine on which side we need
 * to intersect the curves, and on which to add a join.
 */
static void
add_curve (StrokeData     *stroke_data,
           const GskCurve *curve,
           gboolean        force_round_join)
{
  GskCurve l, r;

  if (curve_is_ignorable (curve))
    return;

#ifdef STROKE_DEBUG
  add_debug (stroke_data, curve);
#endif

  gsk_curve_offset (curve, - stroke_data->stroke->line_width / 2, &l);
  gsk_curve_offset (curve, stroke_data->stroke->line_width / 2, &r);

  add_segments (stroke_data, curve, &r, &l, force_round_join);
}

static int
cmpfloat (const void *p1, const void *p2)
{
  const float *f1 = p1;
  const float *f2 = p2;
  return *f1 < *f2 ? -1 : (*f1 > *f2 ? 1 : 0);
}

#define MAX_SUBDIVISION 3

static void
subdivide_and_add_curve (StrokeData     *stroke_data,
                         const GskCurve *curve,
                         int             level,
                         gboolean        force_round_join)
{
  GskCurve c1, c2;
  float t[5];
  int n;

  if (level == 0)
    add_curve (stroke_data, curve, force_round_join);
  else if (level < MAX_SUBDIVISION && cubic_is_simple (curve))
    add_curve (stroke_data, curve, force_round_join);
  else if (level == MAX_SUBDIVISION && (n = gsk_curve_get_cusps (curve, t)) > 0)
    {
      t[n++] = 0;
      t[n++] = 1;
      qsort (t, n, sizeof (float), cmpfloat);
      for (int i = 0; i + 1 < n; i++)
        {
          gsk_curve_segment (curve, t[i], t[i + 1], &c1);
          subdivide_and_add_curve (stroke_data, &c1, level - 1, i == 0 ? force_round_join : TRUE);
        }
    }
  else
    {
      n = 0;
      t[n++] = 0;
      t[n++] = 1;

      if (level == MAX_SUBDIVISION)
        {
          n += gsk_curve_get_curvature_points (curve, &t[n]);
          qsort (t, n, sizeof (float), cmpfloat);
        }

      if (n == 2)
        {
          gsk_curve_split (curve, 0.5, &c1, &c2);
          subdivide_and_add_curve (stroke_data, &c1, level - 1, force_round_join);
          subdivide_and_add_curve (stroke_data, &c2, level - 1, TRUE);
        }
      else
        {
          for (int i = 0; i + 1 < n; i++)
            {
              gsk_curve_segment (curve, t[i], t[i+1], &c1);
              subdivide_and_add_curve (stroke_data, &c1, level - 1, i == 0 ? force_round_join : TRUE);
            }
        }
    }
}

static void
add_degenerate_conic (StrokeData     *stroke_data,
                      const GskCurve *curve)
{
  GskCurve c;
  graphene_point_t p[2];

  p[0] = *gsk_curve_get_start_point (curve);
  gsk_curve_get_point (curve, 0.5, &p[1]);
  gsk_curve_init_foreach (&c, GSK_PATH_LINE, p, 2, 0);
  add_curve (stroke_data, &c, FALSE);

  p[0] = p[1];
  p[1] = *gsk_curve_get_end_point (curve);
  gsk_curve_init_foreach (&c, GSK_PATH_LINE, p, 2, 0);
  add_curve (stroke_data, &c, TRUE);
}

static void
subdivide_and_add_conic (StrokeData     *stroke_data,
                         const GskCurve *curve,
                         int             level,
                         gboolean        force_round_join)
{
  if (level == MAX_SUBDIVISION && conic_is_degenerate (curve))
    add_degenerate_conic (stroke_data, curve);
  else if (level == 0 || (level < MAX_SUBDIVISION && conic_is_simple (curve)))
    add_curve (stroke_data, curve, force_round_join);
  else
    {
      GskCurve c1, c2;

      gsk_curve_split (curve, 0.5, &c1, &c2);
      subdivide_and_add_conic (stroke_data, &c1, level - 1, force_round_join);
      subdivide_and_add_conic (stroke_data, &c2, level - 1, TRUE);
    }
}

/* Create a single closed contour and add it to
 * stroke_data->builder, by connecting the right and the
 * reversed left contour with caps.
 *
 * After this call, stroke_data->left and ->right are NULL.
 */
static void
cap_and_connect_contours (StrokeData *stroke_data)
{
  GskPath *path;
  const graphene_point_t *r0, *l0, *r1, *l1;

  r1 = r0 = gsk_curve_get_start_point (&stroke_data->r0);
  l1 = l0 = gsk_curve_get_start_point (&stroke_data->l0);

  if (stroke_data->has_current_curve)
    {
      path_builder_add_curve (stroke_data->right, &stroke_data->r);
      path_builder_add_curve (stroke_data->left, &stroke_data->l);

      r1 = gsk_curve_get_end_point (&stroke_data->r);
      l1 = gsk_curve_get_end_point (&stroke_data->l);
    }
  else
    path_builder_move_to_point (stroke_data->right, r1);

  add_line_cap (stroke_data->right,
                stroke_data->stroke->line_cap,
                stroke_data->stroke->line_width,
                r1, l1);

  if (stroke_data->has_current_curve)
    {
      GskCurve c;

      path = gsk_path_builder_free_to_path (stroke_data->left);
      path_builder_add_reverse_path (stroke_data->right, path);
      gsk_path_unref (path);

      if (!stroke_data->is_first_curve)
        {
          /* Add the first segment that wasn't added initially */
          gsk_curve_reverse (&stroke_data->l0, &c);
          path_builder_add_curve (stroke_data->right, &c);
        }
    }

  add_line_cap (stroke_data->right,
                stroke_data->stroke->line_cap,
                stroke_data->stroke->line_width,
                l0, r0);

  if (stroke_data->has_current_curve)
    {
      if (!stroke_data->is_first_curve)
        {
          /* Add the first segment that wasn't added initially */
          path_builder_add_curve (stroke_data->right, &stroke_data->r0);
        }
    }

  gsk_path_builder_close (stroke_data->right);

  path = gsk_path_builder_free_to_path (stroke_data->right);
  gsk_path_builder_add_path (stroke_data->builder, path);
  gsk_path_unref (path);

  stroke_data->left = NULL;
  stroke_data->right = NULL;
}

/* Close the left and the right contours and add them to
 * stroke_data->builder.
 *
 * After this call, stroke_data->left and ->right are NULL.
 */
static void
close_contours (StrokeData *stroke_data)
{
  GskPath *path;
  GskPathBuilder *builder;

  if (stroke_data->has_current_curve)
    {
      /* add final join and first segment */
      add_segments (stroke_data, &stroke_data->c0, &stroke_data->r0, &stroke_data->l0, FALSE);
      path_builder_add_curve (stroke_data->right, &stroke_data->r);
      path_builder_add_curve (stroke_data->left, &stroke_data->l);
    }

  gsk_path_builder_close (stroke_data->right);

  path = gsk_path_builder_free_to_path (stroke_data->right);
  gsk_path_builder_add_path (stroke_data->builder, path);
  gsk_path_unref (path);

  builder = gsk_path_builder_new ();
  path_builder_move_to_point (builder, gsk_path_builder_get_current_point (stroke_data->left));
  path = gsk_path_builder_free_to_path (stroke_data->left);
  path_builder_add_reverse_path (builder, path);
  gsk_path_unref (path);
  gsk_path_builder_close (builder);

  path = gsk_path_builder_free_to_path (builder);
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
        cap_and_connect_contours (stroke_data);

      gsk_curve_init_foreach (&curve,
                              GSK_PATH_LINE,
                              (const graphene_point_t[]) { pts[0], GRAPHENE_POINT_INIT (pts[0].x + 1, pts[0].y) },
                              2, 0.f);
      gsk_curve_offset (&curve, stroke_data->stroke->line_width / 2, &stroke_data->r0);
      gsk_curve_offset (&curve, - stroke_data->stroke->line_width / 2, &stroke_data->l0);

      stroke_data->right = gsk_path_builder_new ();
      stroke_data->left = gsk_path_builder_new ();

      stroke_data->has_current_point = TRUE;
      stroke_data->has_current_curve = FALSE;
      break;

    case GSK_PATH_CLOSE:
      if (stroke_data->has_current_point)
        {
          if (!graphene_point_near (&pts[0], &pts[1], 0.001))
            {
              gsk_curve_init_foreach (&curve, GSK_PATH_LINE, pts, n_pts, weight);
              add_curve (stroke_data, &curve, FALSE);
            }
          close_contours (stroke_data);
        }

      stroke_data->has_current_point = FALSE;
      stroke_data->has_current_curve = FALSE;
      break;

    case GSK_PATH_LINE:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      add_curve (stroke_data, &curve, FALSE);
      break;

    case GSK_PATH_CURVE:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      subdivide_and_add_curve (stroke_data, &curve, MAX_SUBDIVISION, FALSE);
      break;

    case GSK_PATH_CONIC:
      gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
      subdivide_and_add_conic (stroke_data, &curve, MAX_SUBDIVISION, FALSE);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

/*
 * gsk_contour_default_add_stroke:
 * @contour: the GskContour to stroke
 * @builder: a GskPathBuilder to add the results to
 * @stroke: stroke parameters
 *
 * Strokes @contour according to the parameters given in @stroke,
 * and adds the resulting curves to @builder. Note that stroking
 * a contour will in general produce multiple contours - either
 * because @contour is closed and has a left and right outline,
 * or because @stroke requires dashes.
 */
void
gsk_contour_default_add_stroke (const GskContour *contour,
                                GskPathBuilder   *builder,
                                GskStroke        *stroke)
{
  StrokeData stroke_data;

  memset (&stroke_data, 0, sizeof (StrokeData));
  stroke_data.builder = builder;
  stroke_data.stroke = stroke;

#ifdef STROKE_DEBUG
  stroke_data.debug = gsk_path_builder_new ();
#endif

  if (stroke->dash_length <= 0)
    gsk_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, stroke_op, &stroke_data);
  else
    gsk_contour_dash (contour, stroke, GSK_PATH_TOLERANCE_DEFAULT, stroke_op, &stroke_data);

  if (stroke_data.has_current_point)
    cap_and_connect_contours (&stroke_data);

#ifdef STROKE_DEBUG
  GskPath *path = gsk_path_builder_free_to_path (stroke_data.debug);
  gsk_path_builder_add_path (builder, path);
  gsk_path_unref (path);
#endif
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
