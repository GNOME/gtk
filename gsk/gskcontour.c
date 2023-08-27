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

#include "gskcontourprivate.h"

#include "gskcurveprivate.h"
#include "gskpathbuilder.h"
#include "gskpathprivate.h"
#include "gskpathpointprivate.h"
#include "gskstrokeprivate.h"

typedef struct _GskContourClass GskContourClass;

struct _GskContour
{
  const GskContourClass *klass;
};

struct _GskContourClass
{
  gsize struct_size;
  const char *type_name;

  void                  (* copy)                (const GskContour       *contour,
                                                 GskContour             *dest);
  gsize                 (* get_size)            (const GskContour       *contour);
  GskPathFlags          (* get_flags)           (const GskContour       *contour);
  void                  (* print)               (const GskContour       *contour,
                                                 GString                *string);
  gboolean              (* get_bounds)          (const GskContour       *contour,
                                                 GskBoundingBox         *bounds);
  gboolean              (* get_stroke_bounds)   (const GskContour       *contour,
                                                 const GskStroke        *stroke,
                                                 GskBoundingBox         *bounds);
  void                  (* get_start_end)       (const GskContour       *self,
                                                 graphene_point_t       *start,
                                                 graphene_point_t       *end);
  gboolean              (* foreach)             (const GskContour       *contour,
                                                 float                   tolerance,
                                                 GskPathForeachFunc      func,
                                                 gpointer                user_data);
  GskContour *          (* reverse)             (const GskContour       *contour);
  int                   (* get_winding)         (const GskContour       *contour,
                                                 const graphene_point_t *point);
  gsize                 (* get_n_ops)           (const GskContour       *contour);
  gboolean              (* get_closest_point)   (const GskContour       *contour,
                                                 const graphene_point_t *point,
                                                 float                   threshold,
                                                 GskRealPathPoint       *result,
                                                 float                  *out_dist);
  void                  (* get_position)        (const GskContour       *contour,
                                                 GskRealPathPoint       *point,
                                                 graphene_point_t       *position);
  void                  (* get_tangent)         (const GskContour       *contour,
                                                 GskRealPathPoint       *point,
                                                 GskPathDirection        direction,
                                                 graphene_vec2_t        *tangent);
  float                 (* get_curvature)       (const GskContour       *contour,
                                                 GskRealPathPoint       *point,
                                                 GskPathDirection        direction,
                                                 graphene_point_t       *center);
  void                  (* add_segment)         (const GskContour       *contour,
                                                 GskPathBuilder         *builder,
                                                 gboolean                emit_move_to,
                                                 GskRealPathPoint       *start,
                                                 GskRealPathPoint       *end);
  gpointer              (* init_measure)        (const GskContour       *contour,
                                                 float                   tolerance,
                                                 float                  *out_length);
  void                  (* free_measure)        (const GskContour       *contour,
                                                 gpointer                measure_data);
  void                  (* get_point)           (const GskContour       *contour,
                                                 gpointer                measure_data,
                                                 float                   distance,
                                                 GskRealPathPoint       *result);
  float                 (* get_distance)        (const GskContour       *contour,
                                                 GskRealPathPoint       *point,
                                                 gpointer                measure_data);
};

/* {{{ Utilities */

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))
#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))

static void
_g_string_append_double (GString    *string,
                         const char *prefix,
                         double      d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, d);
  g_string_append (string, prefix);
  g_string_append (string, buf);
}

static void
_g_string_append_point (GString                *string,
                        const char             *prefix,
                        const graphene_point_t *pt)
{
  _g_string_append_double (string, prefix, pt->x);
  _g_string_append_double (string, " ", pt->y);
}

static gboolean
add_segment (GskPathOperation        op,
             const graphene_point_t *pts,
             gsize                   n_pts,
             float                   weight,
             gpointer                user_data)
{
  GskPathBuilder *builder = user_data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      gsk_path_builder_move_to (builder, pts[0].x, pts[0].y);
      break;
    case GSK_PATH_LINE:
      gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
      break;
    case GSK_PATH_QUAD:
      gsk_path_builder_quad_to (builder,
                                pts[1].x, pts[1].y,
                                pts[2].x, pts[2].y);
      break;
    case GSK_PATH_CUBIC:
      gsk_path_builder_cubic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 pts[3].x, pts[3].y);
      break;
    case GSK_PATH_CONIC:
      gsk_path_builder_conic_to (builder,
                                 pts[1].x, pts[1].y,
                                 pts[2].x, pts[2].y,
                                 weight);
      break;
    case GSK_PATH_CLOSE:
      gsk_path_builder_close (builder);
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static GskPath *
convert_to_standard_contour (const GskContour *contour)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  gsk_contour_foreach (contour, 0.5, add_segment, builder);
  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Default implementations */

static gsize
gsk_contour_get_size_default (const GskContour *contour)
{
  return contour->klass->struct_size;
}

static gboolean
foreach_print (GskPathOperation        op,
               const graphene_point_t *pts,
               gsize                   n_pts,
               float                   weight,
               gpointer                data)
{
  GString *string = data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      _g_string_append_point (string, "M ", &pts[0]);
      break;

    case GSK_PATH_CLOSE:
      g_string_append (string, " Z");
      break;

    case GSK_PATH_LINE:
      _g_string_append_point (string, " L ", &pts[1]);
      break;

    case GSK_PATH_QUAD:
      _g_string_append_point (string, " Q ", &pts[1]);
      _g_string_append_point (string, ", ", &pts[2]);
      break;

    case GSK_PATH_CUBIC:
      _g_string_append_point (string, " C ", &pts[1]);
      _g_string_append_point (string, ", ", &pts[2]);
      _g_string_append_point (string, ", ", &pts[3]);
      break;

    case GSK_PATH_CONIC:
      _g_string_append_point (string, " O ", &pts[1]);
      _g_string_append_point (string, ", ", &pts[2]);
      _g_string_append_double (string, ", ", weight);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
gsk_contour_print_default (const GskContour *contour,
                           GString          *string)
{
  gsk_contour_foreach (contour, 0.5, foreach_print, string);
}

/* }}} */
/* {{{ Standard */

typedef struct _GskStandardContour GskStandardContour;
struct _GskStandardContour
{
  GskContour contour;

  GskPathFlags flags;

  GskBoundingBox bounds;

  gsize n_ops;
  gsize n_points;
  graphene_point_t *points;
  gskpathop ops[];
};

static gsize
gsk_standard_contour_compute_size (gsize n_ops,
                                   gsize n_points)
{
  gsize align = MAX (G_ALIGNOF (graphene_point_t),
                     MAX (G_ALIGNOF (gpointer),
                          G_ALIGNOF (GskStandardContour)));
  gsize s = sizeof (GskStandardContour)
          + sizeof (gskpathop) * n_ops
          + sizeof (graphene_point_t) * n_points;
  return s + (align - (s % align));
}

static void
gsk_standard_contour_init (GskContour             *contour,
                           GskPathFlags            flags,
                           const graphene_point_t *points,
                           gsize                   n_points,
                           const gskpathop        *ops,
                           gsize                   n_ops,
                           ptrdiff_t               offset);

static void
gsk_standard_contour_copy (const GskContour *contour,
                           GskContour       *dest)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  gsk_standard_contour_init (dest, self->flags, self->points, self->n_points, self->ops, self->n_ops, 0);
}

static gsize
gsk_standard_contour_get_size (const GskContour *contour)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  return gsk_standard_contour_compute_size (self->n_ops, self->n_points);
}

static gboolean
gsk_standard_contour_foreach (const GskContour   *contour,
                              float               tolerance,
                              GskPathForeachFunc  func,
                              gpointer            user_data)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  for (i = 0; i < self->n_ops; i ++)
    {
      if (!gsk_pathop_foreach (self->ops[i], func, user_data))
        return FALSE;
    }

  return TRUE;
}

static gboolean
add_reverse (GskPathOperation        op,
             const graphene_point_t *pts,
             gsize                   n_pts,
             float                   weight,
             gpointer                user_data)
{
  GskPathBuilder *builder = user_data;
  GskCurve c, r;

  if (op == GSK_PATH_MOVE)
    return TRUE;

  if (op == GSK_PATH_CLOSE)
    op = GSK_PATH_LINE;

  gsk_curve_init_foreach (&c, op, pts, n_pts, weight);
  gsk_curve_reverse (&c, &r);
  gsk_curve_builder_to (&r, builder);

  return TRUE;
}

static GskContour *
gsk_standard_contour_reverse (const GskContour *contour)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  GskPathBuilder *builder;
  GskPath *path;
  GskContour *res;

  builder = gsk_path_builder_new ();

  gsk_path_builder_move_to (builder, self->points[self->n_points - 1].x,
                                     self->points[self->n_points - 1].y);

  for (int i = self->n_ops - 1; i >= 0; i--)
    gsk_pathop_foreach (self->ops[i], add_reverse, builder);

  if (self->flags & GSK_PATH_CLOSED)
    gsk_path_builder_close (builder);

  path = gsk_path_builder_free_to_path (builder);

  g_assert (gsk_path_get_n_contours (path) == 1);

  res = gsk_contour_dup (gsk_path_get_contour (path, 0));

  gsk_path_unref (path);

  return res;
}

static GskPathFlags
gsk_standard_contour_get_flags (const GskContour *contour)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  return self->flags;
}

static gboolean
gsk_standard_contour_get_bounds (const GskContour *contour,
                                 GskBoundingBox   *bounds)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  if (self->n_points == 0)
    return FALSE;

  *bounds = self->bounds;

  return bounds->max.x > bounds->min.x && bounds->max.y > bounds->min.y;
}

static gboolean
gsk_standard_contour_get_stroke_bounds (const GskContour *contour,
                                        const GskStroke  *stroke,
                                        GskBoundingBox   *bounds)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  float extra;

  if (self->n_points == 0)
    return FALSE;

  extra = MAX (stroke->line_width, gsk_stroke_get_join_width (stroke));

  gsk_bounding_box_init (bounds, &GRAPHENE_POINT_INIT (self->bounds.min.x - extra,
                                                       self->bounds.min.y - extra),
                                 &GRAPHENE_POINT_INIT (self->bounds.max.x + extra,
                                                       self->bounds.max.y + extra));

  return TRUE;
}

static void
gsk_standard_contour_get_start_end (const GskContour *contour,
                                    graphene_point_t *start,
                                    graphene_point_t *end)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  if (start)
    *start = self->points[0];

  if (end)
    *end = self->points[self->n_points - 1];
}

static int
gsk_standard_contour_get_winding (const GskContour       *contour,
                                  const graphene_point_t *point)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  int winding = 0;

  if (!gsk_bounding_box_contains_point (&self->bounds, point))
    return 0;

  for (gsize i = 0; i < self->n_ops; i ++)
    {
      GskCurve c;

      if (gsk_pathop_op (self->ops[i]) == GSK_PATH_MOVE)
        continue;

      gsk_curve_init (&c, self->ops[i]);
      winding += gsk_curve_get_crossing (&c, point);
    }

  if ((self->flags & GSK_PATH_CLOSED) == 0)
    {
      GskCurve c;

      gsk_curve_init (&c, gsk_pathop_encode (GSK_PATH_CLOSE,
                                             (const graphene_point_t[]) { self->points[self->n_points - 1],
                                                                          self->points[0] }));

      winding += gsk_curve_get_crossing (&c, point);
    }

  return winding;
}

static gsize
gsk_standard_contour_get_n_ops (const GskContour *contour)
{
  GskStandardContour *self = (GskStandardContour *) contour;

  return self->n_ops;
}

static gboolean
gsk_standard_contour_get_closest_point (const GskContour       *contour,
                                        const graphene_point_t *point,
                                        float                   threshold,
                                        GskRealPathPoint       *result,
                                        float                  *out_dist)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  unsigned int best_idx = G_MAXUINT;
  float best_t = 0;

  g_assert (gsk_pathop_op (self->ops[0]) == GSK_PATH_MOVE);

  if (self->n_ops == 1)
    {
      float dist;

      dist = graphene_point_distance (point, &self->points[0], NULL, NULL);
      if (dist <= threshold)
        {
          *out_dist = dist;
          result->idx = 0;
          result->t = 0;
          return TRUE;
        }

      return FALSE;
    }

  for (gsize i = 0; i < self->n_ops; i ++)
    {
      GskCurve c;
      float distance, t;

      if (gsk_pathop_op (self->ops[i]) == GSK_PATH_MOVE)
        continue;

      gsk_curve_init (&c, self->ops[i]);
      if (gsk_curve_get_closest_point (&c, point, threshold, &distance, &t) &&
          distance < threshold)
        {
          best_idx = i;
          best_t = t;
          threshold = distance;
        }
    }

  if (best_idx != G_MAXUINT)
    {
      *out_dist = threshold;
      result->idx = best_idx;
      result->t = best_t;
      return TRUE;
    }

  return FALSE;
}

static void
gsk_standard_contour_get_position (const GskContour *contour,
                                   GskRealPathPoint *point,
                                   graphene_point_t *position)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve curve;

  if (G_UNLIKELY (point->idx == 0))
    {
      *position = self->points[0];
      return;
    }

  gsk_curve_init (&curve, self->ops[point->idx]);
  gsk_curve_get_point (&curve, point->t, position);
}

static void
gsk_standard_contour_get_tangent (const GskContour *contour,
                                  GskRealPathPoint *point,
                                  GskPathDirection  direction,
                                  graphene_vec2_t  *tangent)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve curve;
  gsize idx;
  float t;

  if (G_UNLIKELY (point->idx == 0))
    {
      graphene_vec2_init (tangent, 1, 0);
      return;
    }

  idx = point->idx;
  t = point->t;

  if (t == 0 && (direction == GSK_PATH_FROM_START ||
                 direction == GSK_PATH_TO_START))
    {
      /* Look at the previous segment */
      if (idx > 1)
        {
          idx--;
          t = 1;
        }
      else if (self->flags & GSK_PATH_CLOSED)
        {
          idx = self->n_ops - 1;
          t = 1;
        }
    }
  else if (t == 1 && (direction == GSK_PATH_TO_END ||
                      direction == GSK_PATH_FROM_END))
    {
      /* Look at the next segment */
      if (idx < self->n_ops - 1)
        {
          idx++;
          t = 0;
        }
      else if (self->flags & GSK_PATH_CLOSED)
        {
          idx = 1;
          t = 0;
        }
    }

  gsk_curve_init (&curve, self->ops[idx]);
  gsk_curve_get_tangent (&curve, t, tangent);

  if (direction == GSK_PATH_TO_START || direction == GSK_PATH_FROM_END)
    graphene_vec2_negate (tangent, tangent);
}

static float
gsk_standard_contour_get_curvature (const GskContour *contour,
                                    GskRealPathPoint *point,
                                    GskPathDirection  direction,
                                    graphene_point_t *center)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve curve;
  gsize idx;
  float t;

  if (G_UNLIKELY (point->idx == 0))
    return 0;

  idx = point->idx;
  t = point->t;

  if (t == 0 && idx > 1 &&
      (direction == GSK_PATH_FROM_START || direction == GSK_PATH_TO_START))
    {
      idx--;
      t = 1;
    }
  else if (t == 1 && idx + 1 < self->n_ops &&
           (direction == GSK_PATH_FROM_END || direction == GSK_PATH_TO_END))
    {
      idx++;
      t = 0;
    }

  gsk_curve_init (&curve, self->ops[idx]);
  return gsk_curve_get_curvature (&curve, t, center);
}

static void
add_curve (GskCurve       *curve,
           GskPathBuilder *builder,
           gboolean       *emit_move_to)
{
  if (*emit_move_to)
    {
      const graphene_point_t *s;

      s = gsk_curve_get_start_point (curve);
      gsk_path_builder_move_to (builder, s->x, s->y);
      *emit_move_to = FALSE;
    }
  gsk_curve_builder_to (curve, builder);
}

static void
gsk_standard_contour_add_segment (const GskContour *contour,
                                  GskPathBuilder   *builder,
                                  gboolean          emit_move_to,
                                  GskRealPathPoint *start,
                                  GskRealPathPoint *end)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve c, c1, c2;

  g_assert (start->idx < self->n_ops);

  gsk_curve_init (&c, self->ops[start->idx]);

  if (start->idx == end->idx)
    {
      gsk_curve_segment (&c, start->t, end->t, &c1);
      add_curve (&c1, builder, &emit_move_to);
      return;
    }
  if (start->t == 0)
    {
      add_curve (&c, builder, &emit_move_to);
    }
  else if (start->t < 1)
    {
      gsk_curve_split (&c, start->t, &c1, &c2);
      add_curve (&c2, builder, &emit_move_to);
    }

  for (gsize i = start->idx + 1; i < end->idx; i++)
    {
      gsk_curve_init (&c, self->ops[i]);
      add_curve (&c, builder, &emit_move_to);
    }

  gsk_curve_init (&c, self->ops[end->idx]);
  if (c.op == GSK_PATH_CLOSE)
    c.op = GSK_PATH_LINE;

  if (end->t == 1)
    {
      add_curve (&c, builder, &emit_move_to);
    }
  else if (end->t > 0)
    {
      gsk_curve_split (&c, end->t, &c1, &c2);
      add_curve (&c1, builder, &emit_move_to);
    }
}

typedef struct
{
  gsize idx;
  float length0;
  float length1;
  gsize n_samples;
  gsize first;
} CurveMeasure;

typedef struct
{
  float t;
  float length;
} CurvePoint;

typedef struct
{
  GArray *curves;
  GArray *points;
  float tolerance;
} GskStandardContourMeasure;

static void
add_measure (const GskCurve *curve,
             gsize           idx,
             float           length,
             float           tolerance,
             float           t1,
             float           l1,
             GArray         *array)
{
  GskCurve c;
  float ll, l0;
  float t0;
  CurvePoint *p;

  /* Check if we can add (t1, length + l1) without further
   * splitting. We check two things:
   * - Is the curve close to a straight line (length-wise) ?
   * - Does the roundtrip length<>t not deviate too much ?
   */

  if (curve->op == GSK_PATH_LINE ||
      curve->op == GSK_PATH_CLOSE)
    goto done;

  p = &g_array_index (array, CurvePoint, array->len - 1);

  t0 = (p->t + t1) / 2;
  if (t0 == p->t  || t0 == t1)
    goto done;

  gsk_curve_split (curve, t0, &c, NULL);
  l0 = gsk_curve_get_length (&c);
  ll = (p->length + length + l1) / 2;

  if (fabsf (length + l0 - ll) < tolerance)
    {
done:
      g_array_append_val (array, ((CurvePoint) { t1, length + l1 }));
    }
  else
    {
      add_measure (curve, idx, length, tolerance, t0, l0, array);
      add_measure (curve, idx, length, tolerance, t1, l1, array);
    }
}

static int
cmpfloat (const void *p1, const void *p2)
{
  const float *f1 = p1;
  const float *f2 = p2;
  return *f1 < *f2 ? -1 : (*f1 > *f2 ? 1 : 0);
}

static void
add_samples (const GskStandardContour  *self,
             GskStandardContourMeasure *measure,
             CurveMeasure              *curve_measure)
{
  gsize first;
  GskCurve curve;
  float l0, l1;
  float t[3];
  int n;

  g_assert (curve_measure->n_samples == 0);
  g_assert (0 < curve_measure->idx && curve_measure->idx < self->n_ops);

  first = measure->points->len;

  l0 = curve_measure->length0;
  l1 = curve_measure->length1;

  g_array_append_val (measure->points, ((CurvePoint) { 0, l0 } ));

  gsk_curve_init (&curve, self->ops[curve_measure->idx]);

  n = gsk_curve_get_curvature_points (&curve, t);
  qsort (t, n, sizeof (float), cmpfloat);

  for (int j = 0; j < n; j++)
    {
      float l = gsk_curve_get_length_to (&curve, t[j]);
      add_measure (&curve, curve_measure->idx, l0, measure->tolerance, t[j], l, measure->points);
    }

  add_measure (&curve, curve_measure->idx, l0, measure->tolerance, 1, l1 - l0, measure->points);

  curve_measure->first = first;
  curve_measure->n_samples = measure->points->len - first;
}

static void
ensure_samples (const GskStandardContour  *self,
                GskStandardContourMeasure *measure,
                CurveMeasure              *curve_measure)
{
  if (curve_measure->n_samples == 0)
    add_samples (self, measure, curve_measure);
}

static gpointer
gsk_standard_contour_init_measure (const GskContour *contour,
                                   float             tolerance,
                                   float            *out_length)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  GskStandardContourMeasure *measure;
  float length;

  measure = g_new (GskStandardContourMeasure, 1);

  measure->curves = g_array_new (FALSE, FALSE, sizeof (CurveMeasure));
  measure->points = g_array_new (FALSE, FALSE, sizeof (CurvePoint));
  measure->tolerance = tolerance;

  /* Add a placeholder for the move, so indexes match up */
  g_array_append_val (measure->curves, ((CurveMeasure) { 0, -1, -1, 0, 0 } ));

  length = 0;

  for (gsize i = 1; i < self->n_ops; i++)
    {
      GskCurve curve;
      float l;

      gsk_curve_init (&curve, self->ops[i]);
      l = gsk_curve_get_length (&curve);

      g_array_append_val (measure->curves, ((CurveMeasure) { i, length, length + l, 0, 0 } ));

      length += l;
    }

  *out_length = length;

  return measure;
}

static void
gsk_standard_contour_free_measure (const GskContour *contour,
                                   gpointer          data)
{
  GskStandardContourMeasure *measure = data;

  g_array_free (measure->curves, TRUE);
  g_array_free (measure->points, TRUE);
  g_free (measure);
}

static int
find_curve (gconstpointer a,
            gconstpointer b)
{
  const CurveMeasure *m = a;
  const float distance = *(const float *) b;

  if (distance < m->length0)
    return 1;
  else if (distance > m->length1)
    return -1;
  else
    return 0;
}

static void
gsk_standard_contour_get_point (const GskContour *contour,
                                gpointer          measure_data,
                                float             distance,
                                GskRealPathPoint *result)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  GskStandardContourMeasure *measure = measure_data;
  CurveMeasure *curve_measure;
  gboolean found G_GNUC_UNUSED;
  guint idx;
  gsize i0, i1;
  CurvePoint *p0, *p1;

  if (self->n_ops == 1)
    {
      result->idx = 0;
      result->t = 1;
      return;
    }

  found = g_array_binary_search (measure->curves, &distance, find_curve, &idx);
  g_assert (found);

  curve_measure = &g_array_index (measure->curves, CurveMeasure, idx);
  ensure_samples (self, measure, curve_measure);

  i0 = curve_measure->first;
  i1 = curve_measure->first + curve_measure->n_samples - 1;
  while (i0 + 1 < i1)
    {
      gsize i = (i0 + i1) / 2;
      CurvePoint *p = &g_array_index (measure->points, CurvePoint, i);

      if (p->length < distance)
        i0 = i;
      else if (p->length > distance)
        i1 = i;
      else
        {
          result->idx = curve_measure->idx;
          result->t = p->t;
          return;
        }
    }

  p0 = &g_array_index (measure->points, CurvePoint, i0);
  p1 = &g_array_index (measure->points, CurvePoint, i1);

  if (distance >= p1->length)
    {
      if (curve_measure->idx == self->n_ops - 1)
        {
          result->idx = curve_measure->idx;
          result->t = 1;
        }
      else
        {
          result->idx = curve_measure->idx + 1;
          result->t = 0;
        }
    }
  else
    {
      float fraction;

      result->idx = curve_measure->idx;

      fraction = (distance - p0->length) / (p1->length - p0->length);
      g_assert (fraction >= 0.f && fraction <= 1.f);
      result->t = p0->t * (1 - fraction) + p1->t * fraction;
      g_assert (result->t >= 0.f && result->t <= 1.f);
    }
}

static float
gsk_standard_contour_get_distance (const GskContour *contour,
                                   GskRealPathPoint *point,
                                   gpointer          measure_data)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  GskStandardContourMeasure *measure = measure_data;
  CurveMeasure *curve_measure;
  gsize i0, i1;
  CurvePoint *p0, *p1;
  float fraction;

  if (G_UNLIKELY (point->idx == 0))
    return 0;

  curve_measure = &g_array_index (measure->curves, CurveMeasure, point->idx);
  ensure_samples (self, measure, curve_measure);

  i0 = curve_measure->first;
  i1 = curve_measure->first + curve_measure->n_samples - 1;
  while (i0 + 1 < i1)
    {
      gsize i = (i0 + i1) / 2;
      CurvePoint *p = &g_array_index (measure->points, CurvePoint, i);

      if (p->t > point->t)
       i1 = i;
      else if (p->t < point->t)
       i0 = i;
      else
        return p->length;
    }

  p0 = &g_array_index (measure->points, CurvePoint, i0);
  p1 = &g_array_index (measure->points, CurvePoint, i1);

  g_assert (p0->t <= point->t && point->t <= p1->t);

  fraction = (point->t - p0->t) / (p1->t - p0->t);
  g_assert (fraction >= 0.f && fraction <= 1.f);

  return p0->length * (1 - fraction) + p1->length * fraction;
}

static const GskContourClass GSK_STANDARD_CONTOUR_CLASS =
{
  sizeof (GskStandardContour),
  "GskStandardContour",
  gsk_standard_contour_copy,
  gsk_standard_contour_get_size,
  gsk_standard_contour_get_flags,
  gsk_contour_print_default,
  gsk_standard_contour_get_bounds,
  gsk_standard_contour_get_stroke_bounds,
  gsk_standard_contour_get_start_end,
  gsk_standard_contour_foreach,
  gsk_standard_contour_reverse,
  gsk_standard_contour_get_winding,
  gsk_standard_contour_get_n_ops,
  gsk_standard_contour_get_closest_point,
  gsk_standard_contour_get_position,
  gsk_standard_contour_get_tangent,
  gsk_standard_contour_get_curvature,
  gsk_standard_contour_add_segment,
  gsk_standard_contour_init_measure,
  gsk_standard_contour_free_measure,
  gsk_standard_contour_get_point,
  gsk_standard_contour_get_distance,
};

/* You must ensure the contour has enough size allocated,
 * see gsk_standard_contour_compute_size()
 */
static void
gsk_standard_contour_init (GskContour             *contour,
                           GskPathFlags            flags,
                           const graphene_point_t *points,
                           gsize                   n_points,
                           const gskpathop        *ops,
                           gsize                   n_ops,
                           gssize                  offset)

{
  GskStandardContour *self = (GskStandardContour *) contour;

  self->contour.klass = &GSK_STANDARD_CONTOUR_CLASS;

  self->flags = flags;
  self->n_ops = n_ops;
  self->n_points = n_points;
  self->points = (graphene_point_t *) &self->ops[n_ops];
  memcpy (self->points, points, sizeof (graphene_point_t) * n_points);

  offset += self->points - points;
  for (gsize i = 0; i < n_ops; i++)
    self->ops[i] = gsk_pathop_encode (gsk_pathop_op (ops[i]),
                                      gsk_pathop_points (ops[i]) + offset);

  gsk_bounding_box_init (&self->bounds,  &self->points[0], &self->points[0]);
  for (gsize i = 1; i < self->n_points; i ++)
    gsk_bounding_box_expand (&self->bounds, &self->points[i]);
}

GskContour *
gsk_standard_contour_new (GskPathFlags            flags,
                          const graphene_point_t *points,
                          gsize                   n_points,
                          const gskpathop        *ops,
                          gsize                   n_ops,
                          gssize                  offset)
{
  GskContour *contour;

  contour = g_malloc0 (gsk_standard_contour_compute_size (n_ops, n_points));

  gsk_standard_contour_init (contour, flags, points, n_points, ops, n_ops, offset);

  return contour;
}

/* }}} */
/* {{{ Circle */

typedef struct _GskCircleContour GskCircleContour;
struct _GskCircleContour
{
  GskContour contour;

  graphene_point_t center;
  float radius;
  gboolean ccw;
};

static void
gsk_circle_contour_copy (const GskContour *contour,
                         GskContour       *dest)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  GskCircleContour *target = (GskCircleContour *) dest;

  *target = *self;
}

static GskPathFlags
gsk_circle_contour_get_flags (const GskContour *contour)
{
  return GSK_PATH_CLOSED;
}

static gboolean
gsk_circle_contour_get_bounds (const GskContour *contour,
                               GskBoundingBox   *bounds)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  gsk_bounding_box_init (bounds,
                         &GRAPHENE_POINT_INIT (self->center.x - self->radius,
                                               self->center.y - self->radius),
                         &GRAPHENE_POINT_INIT (self->center.x + self->radius,
                                               self->center.y + self->radius));

  return TRUE;
}

static gboolean
gsk_circle_contour_get_stroke_bounds (const GskContour *contour,
                                      const GskStroke  *stroke,
                                      GskBoundingBox   *bounds)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  gsk_bounding_box_init (bounds,
                         &GRAPHENE_POINT_INIT (self->center.x - self->radius - stroke->line_width/2,
                                               self->center.y - self->radius - stroke->line_width/2),
                         &GRAPHENE_POINT_INIT (self->center.x + self->radius + stroke->line_width/2,
                                               self->center.y + self->radius + stroke->line_width/2));

  return TRUE;
}

static void
gsk_circle_contour_get_start_end (const GskContour *contour,
                                  graphene_point_t *start,
                                  graphene_point_t *end)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (start)
    *start = GRAPHENE_POINT_INIT (self->center.x + self->radius, self->center.y);

  if (end)
    *end = GRAPHENE_POINT_INIT (self->center.x + self->radius, self->center.y);
}

static gboolean
gsk_circle_contour_foreach (const GskContour   *contour,
                            float               tolerance,
                            GskPathForeachFunc  func,
                            gpointer            user_data)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float rx, ry;

  rx = ry = self->radius;
  if (self->ccw)
    ry = - self->radius;

  if (!func (GSK_PATH_MOVE,
             (const graphene_point_t[1]) {
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y),
             },
             1, 0.f, user_data))
    return FALSE;

  if (!func (GSK_PATH_CONIC,
             (const graphene_point_t[3]) {
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y),
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y + ry),
               GRAPHENE_POINT_INIT (self->center.x, self->center.y + ry),
             },
             3, M_SQRT1_2, user_data))
    return FALSE;

  if (!func (GSK_PATH_CONIC,
             (const graphene_point_t[3]) {
               GRAPHENE_POINT_INIT (self->center.x, self->center.y + ry),
               GRAPHENE_POINT_INIT (self->center.x - rx, self->center.y + ry),
               GRAPHENE_POINT_INIT (self->center.x - rx, self->center.y),
             },
             3, M_SQRT1_2, user_data))
    return FALSE;

  if (!func (GSK_PATH_CONIC,
             (const graphene_point_t[3]) {
               GRAPHENE_POINT_INIT (self->center.x - rx, self->center.y),
               GRAPHENE_POINT_INIT (self->center.x - rx, self->center.y - ry),
               GRAPHENE_POINT_INIT (self->center.x, self->center.y - ry),
             },
             3, M_SQRT1_2, user_data))
    return FALSE;

  if (!func (GSK_PATH_CONIC,
             (const graphene_point_t[3]) {
               GRAPHENE_POINT_INIT (self->center.x, self->center.y - ry),
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y - ry),
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y),
             },
             3, M_SQRT1_2, user_data))
    return FALSE;

  if (!func (GSK_PATH_CLOSE,
             (const graphene_point_t[2]) {
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y),
               GRAPHENE_POINT_INIT (self->center.x + rx, self->center.y),
             },
             2, 0.f, user_data))
    return FALSE;

  return TRUE;
}

static GskContour *
gsk_circle_contour_reverse (const GskContour *contour)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  GskCircleContour *copy;

  copy = g_new0 (GskCircleContour, 1);
  gsk_circle_contour_copy (contour, (GskContour *)copy);
  copy->ccw = !self->ccw;

  return (GskContour *)copy;
}

static int
gsk_circle_contour_get_winding (const GskContour       *contour,
                                const graphene_point_t *point)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (graphene_point_distance (point, &self->center, NULL, NULL) <= self->radius)
    return self->ccw ? -1 : 1;

  return 0;
}

static gsize
gsk_circle_contour_get_n_ops (const GskContour *contour)
{
  /* Not related to how many curves foreach produces.
   * GskPath assumes that the start- and endpoints
   * of a contour are { x, 1, 0 } and { x, n_ops - 1, 1 }.
   *
   * The circle contour uses a single 'segment' in path
   * points, with a t that ranges from 0 to 1 to cover
   * the angles from 0 to 360 (or 360 to 0 in the ccw
   * case).
   */

  return 2;
}

static gboolean
gsk_circle_contour_get_closest_point (const GskContour       *contour,
                                      const graphene_point_t *point,
                                      float                   threshold,
                                      GskRealPathPoint       *result,
                                      float                  *out_dist)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float dist, angle, t;

  dist = fabsf (graphene_point_distance (&self->center, point, NULL, NULL) - self->radius);

  if (dist > threshold)
    return FALSE;

  angle = RAD_TO_DEG (atan2f (point->y - self->center.y, point->x - self->center.x));

  if (angle < 0)
    angle = 360 + angle;

  t = CLAMP (angle / 360, 0, 1);

  if (self->ccw)
    t = 1 - t;

  result->idx = 1;
  result->t = t;

  return TRUE;
}

#define GSK_CIRCLE_POINT_INIT(self, angle) \
  GRAPHENE_POINT_INIT ((self)->center.x + cosf (DEG_TO_RAD (angle)) * self->radius, \
                       (self)->center.y + sinf (DEG_TO_RAD (angle)) * self->radius)

static void
gsk_circle_contour_get_position (const GskContour *contour,
                                 GskRealPathPoint *point,
                                 graphene_point_t *position)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float t;

  t = point->t;

  if (self->ccw)
    t = 1 - t;

  if (t == 0 || t == 1)
    *position = GRAPHENE_POINT_INIT (self->center.x + self->radius, self->center.y);
  else
    *position = GSK_CIRCLE_POINT_INIT (self, t * 360);
}

static void
gsk_circle_contour_get_tangent (const GskContour *contour,
                                GskRealPathPoint *point,
                                GskPathDirection  direction,
                                graphene_vec2_t  *tangent)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  graphene_point_t p;

  gsk_circle_contour_get_position (contour, point, &p);

  graphene_vec2_init (tangent, p.y - self->center.y, - p.x + self->center.x);
  graphene_vec2_normalize (tangent, tangent);
}

static float
gsk_circle_contour_get_curvature (const GskContour *contour,
                                  GskRealPathPoint *point,
                                  GskPathDirection  direction,
                                  graphene_point_t *center)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (center)
    *center = self->center;

  return 1 / self->radius;
}

static void
gsk_circle_contour_add_segment (const GskContour *contour,
                                GskPathBuilder   *builder,
                                gboolean          emit_move_to,
                                GskRealPathPoint *start,
                                GskRealPathPoint *end)
{
  GskPath *path;
  graphene_point_t p;
  GskRealPathPoint start2, end2;
  const GskContour *std;
  float dist;

  /* This is a cheesy way of doing things: convert to a standard contour,
   * and translate the path points from circle to standard. We just have
   * to be careful to tell start- and endpoint apart.
   */

  path = convert_to_standard_contour (contour);
  std = gsk_path_get_contour (path, 0);

  start2.contour = 0;

  if (start->idx == 1 && start->t == 0)
    {
      start2.idx = 1;
      start2.t = 0;
    }
  else
    {
      gsk_circle_contour_get_position (contour, start, &p);
      gsk_standard_contour_get_closest_point (std, &p, INFINITY, &start2, &dist);
    }

  end2.contour = 0;

  if (end->idx == 1 && end->t == 1)
    {
      end2.idx = 5;
      end2.t = 1;
    }
  else
    {
      gsk_circle_contour_get_position (contour, end, &p);
      gsk_standard_contour_get_closest_point (std, &p, INFINITY, &end2, &dist);
    }

  gsk_standard_contour_add_segment (std, builder, emit_move_to, &start2, &end2);

  gsk_path_unref (path);
}

static gpointer
gsk_circle_contour_init_measure (const GskContour *contour,
                                 float             tolerance,
                                 float            *out_length)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  *out_length = 2 * M_PI * self->radius;

  return NULL;
}

static void
gsk_circle_contour_free_measure (const GskContour *contour,
                                 gpointer          data)
{
}

static void
gsk_circle_contour_get_point (const GskContour *contour,
                              gpointer          measure_data,
                              float             distance,
                              GskRealPathPoint *result)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float t;

  t = distance / (2 * M_PI * self->radius);

  if (self->ccw)
    t = 1 - t;

  result->idx = 1;
  result->t = t;
}

static float
gsk_circle_contour_get_distance (const GskContour *contour,
                                 GskRealPathPoint *point,
                                 gpointer          measure_data)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float t;

  t = point->t;

  if (self->ccw)
    t = 1 - t;

  return 2 * M_PI * self->radius * t;
}

static const GskContourClass GSK_CIRCLE_CONTOUR_CLASS =
{
  sizeof (GskCircleContour),
  "GskCircleContour",
  gsk_circle_contour_copy,
  gsk_contour_get_size_default,
  gsk_circle_contour_get_flags,
  gsk_contour_print_default,
  gsk_circle_contour_get_bounds,
  gsk_circle_contour_get_stroke_bounds,
  gsk_circle_contour_get_start_end,
  gsk_circle_contour_foreach,
  gsk_circle_contour_reverse,
  gsk_circle_contour_get_winding,
  gsk_circle_contour_get_n_ops,
  gsk_circle_contour_get_closest_point,
  gsk_circle_contour_get_position,
  gsk_circle_contour_get_tangent,
  gsk_circle_contour_get_curvature,
  gsk_circle_contour_add_segment,
  gsk_circle_contour_init_measure,
  gsk_circle_contour_free_measure,
  gsk_circle_contour_get_point,
  gsk_circle_contour_get_distance,
};

GskContour *
gsk_circle_contour_new (const graphene_point_t *center,
                        float                   radius)
{
  GskCircleContour *self;

  self = g_new0 (GskCircleContour, 1);

  self->contour.klass = &GSK_CIRCLE_CONTOUR_CLASS;

  self->contour.klass = &GSK_CIRCLE_CONTOUR_CLASS;
  self->center = *center;
  self->radius = radius;
  self->ccw = FALSE;

  return (GskContour *) self;
}

/* }}} */
/* {{{ Rounded Rectangle */

typedef struct _GskRoundedRectContour GskRoundedRectContour;
struct _GskRoundedRectContour
{
  GskContour contour;

  GskRoundedRect rect;
  gboolean ccw;
};

static void
gsk_rounded_rect_contour_copy (const GskContour *contour,
                               GskContour       *dest)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;
  GskRoundedRectContour *target = (GskRoundedRectContour *) dest;

  *target = *self;
}

static GskPathFlags
gsk_rounded_rect_contour_get_flags (const GskContour *contour)
{
  return GSK_PATH_CLOSED;
}

static gboolean
gsk_rounded_rect_contour_get_bounds (const GskContour *contour,
                                     GskBoundingBox   *bounds)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  gsk_bounding_box_init_from_rect (bounds, &self->rect.bounds);

  return TRUE;
}

static gboolean
gsk_rounded_rect_contour_get_stroke_bounds (const GskContour *contour,
                                            const GskStroke  *stroke,
                                            GskBoundingBox   *bounds)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;
  GskBoundingBox b;

  gsk_bounding_box_init_from_rect (&b, &self->rect.bounds);
  gsk_bounding_box_init (bounds,
                         &GRAPHENE_POINT_INIT (b.min.x - stroke->line_width / 2,
                                               b.min.y - stroke->line_width / 2),
                         &GRAPHENE_POINT_INIT (b.max.x + stroke->line_width / 2,
                                               b.max.y + stroke->line_width / 2));

  return TRUE;
}

static void
gsk_rounded_rect_contour_get_start_end (const GskContour *contour,
                                        graphene_point_t *start,
                                        graphene_point_t *end)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  if (start)
    *start = GRAPHENE_POINT_INIT (self->rect.bounds.origin.x + self->rect.corner[GSK_CORNER_TOP_LEFT].width,
                                  self->rect.bounds.origin.y);

  if (end)
    *end = GRAPHENE_POINT_INIT (self->rect.bounds.origin.x + self->rect.corner[GSK_CORNER_TOP_LEFT].width,
                                self->rect.bounds.origin.y);
}

static void
get_rounded_rect_points (const GskRoundedRect *rect,
                         graphene_point_t     *pts)
{
  pts[0] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->corner[GSK_CORNER_TOP_LEFT].width, rect->bounds.origin.y);
  pts[1] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width - rect->corner[GSK_CORNER_TOP_RIGHT].width, rect->bounds.origin.y);
  pts[2] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width, rect->bounds.origin.y);
  pts[3] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width, rect->bounds.origin.y + rect->corner[GSK_CORNER_TOP_RIGHT].height);
  pts[4] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width, rect->bounds.origin.y + rect->bounds.size.height - rect->corner[GSK_CORNER_BOTTOM_RIGHT].height);
  pts[5] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width, rect->bounds.origin.y + rect->bounds.size.height);
  pts[6] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width - rect->corner[GSK_CORNER_BOTTOM_RIGHT].width, rect->bounds.origin.y + rect->bounds.size.height);
  pts[7] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->corner[GSK_CORNER_BOTTOM_LEFT].width, rect->bounds.origin.y + rect->bounds.size.height);
  pts[8] = GRAPHENE_POINT_INIT (rect->bounds.origin.x, rect->bounds.origin.y + rect->bounds.size.height);
  pts[9] = GRAPHENE_POINT_INIT (rect->bounds.origin.x, rect->bounds.origin.y + rect->bounds.size.height - rect->corner[GSK_CORNER_BOTTOM_LEFT].height);
  pts[10] = GRAPHENE_POINT_INIT (rect->bounds.origin.x, rect->bounds.origin.y + rect->corner[GSK_CORNER_TOP_LEFT].height);
  pts[11] = GRAPHENE_POINT_INIT (rect->bounds.origin.x, rect->bounds.origin.y);
  pts[12] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->corner[GSK_CORNER_TOP_LEFT].width, rect->bounds.origin.y);
}

static gboolean
gsk_rounded_rect_contour_foreach (const GskContour   *contour,
                                  float               tolerance,
                                  GskPathForeachFunc  func,
                                  gpointer            user_data)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;
  graphene_point_t pts[13];

  get_rounded_rect_points (&self->rect, pts);
  if (self->ccw)
    {
      graphene_point_t p;
#define SWAP(a,b,c) a = b; b = c; c = a;
      SWAP (p, pts[1], pts[11]);
      SWAP (p, pts[2], pts[10]);
      SWAP (p, pts[3], pts[9]);
      SWAP (p, pts[4], pts[8]);
      SWAP (p, pts[5], pts[7]);
#undef SWAP

      return func (GSK_PATH_MOVE, &pts[0], 1, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[0], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[2], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[3], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[5], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[6], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[8], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[9], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[11], 2, 0.f, user_data) &&
             func (GSK_PATH_CLOSE, &pts[12], 2, 0.f, user_data);
    }
  else
    {
      return func (GSK_PATH_MOVE, &pts[0], 1, 0.f, user_data) &&
             func (GSK_PATH_LINE, &pts[0], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[1], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[3], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[4], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[6], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[7], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[9], 2, 0.f, user_data) &&
             func (GSK_PATH_CONIC, &pts[10], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_CLOSE, &pts[12], 2, 0.f, user_data);
    }
}

static GskContour *
gsk_rounded_rect_contour_reverse (const GskContour *contour)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;
  GskRoundedRectContour *copy;

  copy = g_new0 (GskRoundedRectContour, 1);
  gsk_rounded_rect_contour_copy (contour, (GskContour *)copy);
  copy->ccw = !self->ccw;

  return (GskContour *)copy;
}

static int
gsk_rounded_rect_contour_get_winding (const GskContour       *contour,
                                      const graphene_point_t *point)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  if (gsk_rounded_rect_contains_point (&self->rect, point))
    return self->ccw ? -1 : 1;

  return 0;
}

static gsize
gsk_rounded_rect_contour_get_n_ops (const GskContour *contour)
{
  return 9;
}

static gboolean
gsk_rounded_rect_contour_get_closest_point (const GskContour       *contour,
                                            const graphene_point_t *point,
                                            float                   threshold,
                                            GskRealPathPoint       *result,
                                            float                  *out_dist)
{
  GskPath *path;
  const GskContour *std;
  gboolean ret;

  path = convert_to_standard_contour (contour);
  std = gsk_path_get_contour (path, 0);
  ret = gsk_standard_contour_get_closest_point (std, point, threshold, result, out_dist);
  gsk_path_unref (path);

  return ret;
}

typedef struct
{
  GskCurve *curve;
  unsigned int idx;
  unsigned int count;
} InitCurveData;

static gboolean
init_curve_cb (GskPathOperation        op,
               const graphene_point_t *pts,
               gsize                   n_pts,
               float                   weight,
               gpointer                user_data)
{
  InitCurveData *data = user_data;

  if (data->idx == data->count)
    {
      gsk_curve_init_foreach (data->curve, op, pts, n_pts, weight);
      return FALSE;
    }

  data->count++;
  return TRUE;
}

static void
gsk_rounded_rect_contour_init_curve (const GskContour *contour,
                                     unsigned int      idx,
                                     GskCurve         *curve)
{
  InitCurveData data;

  data.curve = curve;
  data.idx = idx;
  data.count = 0;

  gsk_contour_foreach (contour, 0.5, init_curve_cb, &data);
}

static void
gsk_rounded_rect_contour_get_position (const GskContour *contour,
                                       GskRealPathPoint *point,
                                       graphene_point_t *position)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->idx, &curve);
  gsk_curve_get_point (&curve, point->t, position);
}

static void
gsk_rounded_rect_contour_get_tangent (const GskContour *contour,
                                      GskRealPathPoint *point,
                                      GskPathDirection  direction,
                                      graphene_vec2_t  *tangent)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->idx, &curve);
  gsk_curve_get_tangent (&curve, point->t, tangent);
}

static float
gsk_rounded_rect_contour_get_curvature (const GskContour *contour,
                                        GskRealPathPoint *point,
                                        GskPathDirection  direction,
                                        graphene_point_t *center)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->idx, &curve);
  return gsk_curve_get_curvature (&curve, point->t, center);
}

static void
gsk_rounded_rect_contour_add_segment (const GskContour *contour,
                                      GskPathBuilder   *builder,
                                      gboolean          emit_move_to,
                                      GskRealPathPoint *start,
                                      GskRealPathPoint *end)
{
  GskPath *path;
  const GskContour *std;

  path = convert_to_standard_contour (contour);
  std = gsk_path_get_contour (path, 0);

  gsk_standard_contour_add_segment (std, builder, emit_move_to, start, end);

  gsk_path_unref (path);
}

typedef struct
{
  const GskContour *contour;
  gpointer measure_data;
} RoundedRectMeasureData;

static gpointer
gsk_rounded_rect_contour_init_measure (const GskContour *contour,
                                       float             tolerance,
                                       float            *out_length)
{
  RoundedRectMeasureData *data;
  GskPath *path;

  path = convert_to_standard_contour (contour);
  data = g_new (RoundedRectMeasureData, 1);
  data->contour = gsk_contour_dup (gsk_path_get_contour (path, 0));
  data->measure_data = gsk_standard_contour_init_measure (data->contour, tolerance, out_length);
  gsk_path_unref (path);

  return data;
}

static void
gsk_rounded_rect_contour_free_measure (const GskContour *contour,
                                       gpointer          measure_data)
{
  RoundedRectMeasureData *data = measure_data;

  gsk_standard_contour_free_measure (data->contour, data->measure_data);
  g_free (data);
}

static void
gsk_rounded_rect_contour_get_point (const GskContour *contour,
                                    gpointer          measure_data,
                                    float             distance,
                                    GskRealPathPoint *result)
{
  RoundedRectMeasureData *data = measure_data;

  gsk_standard_contour_get_point (data->contour, data->measure_data, distance, result);
}

static float
gsk_rounded_rect_contour_get_distance (const GskContour *contour,
                                       GskRealPathPoint *point,
                                       gpointer          measure_data)
{
  RoundedRectMeasureData *data = measure_data;

  return gsk_standard_contour_get_distance (data->contour, point, data->measure_data);
}

static const GskContourClass GSK_ROUNDED_RECT_CONTOUR_CLASS =
{
  sizeof (GskRoundedRectContour),
  "GskRoundedRectContour",
  gsk_rounded_rect_contour_copy,
  gsk_contour_get_size_default,
  gsk_rounded_rect_contour_get_flags,
  gsk_contour_print_default,
  gsk_rounded_rect_contour_get_bounds,
  gsk_rounded_rect_contour_get_stroke_bounds,
  gsk_rounded_rect_contour_get_start_end,
  gsk_rounded_rect_contour_foreach,
  gsk_rounded_rect_contour_reverse,
  gsk_rounded_rect_contour_get_winding,
  gsk_rounded_rect_contour_get_n_ops,
  gsk_rounded_rect_contour_get_closest_point,
  gsk_rounded_rect_contour_get_position,
  gsk_rounded_rect_contour_get_tangent,
  gsk_rounded_rect_contour_get_curvature,
  gsk_rounded_rect_contour_add_segment,
  gsk_rounded_rect_contour_init_measure,
  gsk_rounded_rect_contour_free_measure,
  gsk_rounded_rect_contour_get_point,
  gsk_rounded_rect_contour_get_distance,
};

GskContour *
gsk_rounded_rect_contour_new (const GskRoundedRect *rect)
{
  GskRoundedRectContour *self;

  self = g_new0 (GskRoundedRectContour, 1);

  self->contour.klass = &GSK_ROUNDED_RECT_CONTOUR_CLASS;

  self->rect = *rect;

  return (GskContour *) self;
}

/* }}} */
/* {{{ API */

const char *
gsk_contour_get_type_name (const GskContour *self)
{
  return self->klass->type_name;
}

gsize
gsk_contour_get_size (const GskContour *self)
{
  return self->klass->get_size (self);
}

void
gsk_contour_copy (GskContour       *dest,
                  const GskContour *src)
{
  src->klass->copy (src, dest);
}

GskContour *
gsk_contour_dup (const GskContour *src)
{
  GskContour *copy;

  copy = g_malloc0 (gsk_contour_get_size (src));
  gsk_contour_copy (copy, src);

  return copy;
}

GskContour *
gsk_contour_reverse (const GskContour *src)
{
  return src->klass->reverse (src);
}

GskPathFlags
gsk_contour_get_flags (const GskContour *self)
{
  return self->klass->get_flags (self);
}

void
gsk_contour_print (const GskContour *self,
                   GString          *string)
{
  self->klass->print (self, string);
}

gboolean
gsk_contour_get_bounds (const GskContour *self,
                        GskBoundingBox   *bounds)
{
  return self->klass->get_bounds (self, bounds);
}

gboolean
gsk_contour_get_stroke_bounds (const GskContour *self,
                               const GskStroke  *stroke,
                               GskBoundingBox   *bounds)
{
  return self->klass->get_stroke_bounds (self, stroke, bounds);
}

gboolean
gsk_contour_foreach (const GskContour   *self,
                     float               tolerance,
                     GskPathForeachFunc  func,
                     gpointer            user_data)
{
  return self->klass->foreach (self, tolerance, func, user_data);
}

void
gsk_contour_get_start_end (const GskContour *self,
                           graphene_point_t *start,
                           graphene_point_t *end)
{
  self->klass->get_start_end (self, start, end);
}

int
gsk_contour_get_winding (const GskContour       *self,
                         const graphene_point_t *point)
{
  return self->klass->get_winding (self, point);
}

gboolean
gsk_contour_get_closest_point (const GskContour       *self,
                               const graphene_point_t *point,
                               float                   threshold,
                               GskRealPathPoint       *result,
                               float                  *out_dist)
{
  return self->klass->get_closest_point (self, point, threshold, result, out_dist);
}

gsize
gsk_contour_get_n_ops (const GskContour *self)
{
  return self->klass->get_n_ops (self);
}

void
gsk_contour_get_position (const GskContour *self,
                          GskRealPathPoint *point,
                          graphene_point_t *pos)
{
  self->klass->get_position (self, point, pos);
}

void
gsk_contour_get_tangent (const GskContour *self,
                         GskRealPathPoint *point,
                         GskPathDirection  direction,
                         graphene_vec2_t  *tangent)
{
  self->klass->get_tangent (self, point, direction, tangent);
}

float
gsk_contour_get_curvature (const GskContour *self,
                           GskRealPathPoint *point,
                           GskPathDirection  direction,
                           graphene_point_t *center)
{
  return self->klass->get_curvature (self, point, direction, center);
}

void
gsk_contour_add_segment (const GskContour *self,
                         GskPathBuilder   *builder,
                         gboolean          emit_move_to,
                         GskRealPathPoint *start,
                         GskRealPathPoint *end)
{
  self->klass->add_segment (self, builder, emit_move_to, start, end);
}

gpointer
gsk_contour_init_measure (const GskContour *self,
                          float             tolerance,
                          float            *out_length)
{
  return self->klass->init_measure (self, tolerance, out_length);
}

void
gsk_contour_free_measure (const GskContour *self,
                          gpointer          data)
{
  self->klass->free_measure (self, data);
}

void
gsk_contour_get_point (const GskContour *self,
                       gpointer          measure_data,
                       float             distance,
                       GskRealPathPoint *result)
{
  self->klass->get_point (self, measure_data, distance, result);
}

float
gsk_contour_get_distance (const GskContour *self,
                          GskRealPathPoint *point,
                          gpointer          measure_data)
{
  return self->klass->get_distance (self, point, measure_data);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
