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
#include "gsksplineprivate.h"
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
                                                 graphene_point_t       *center);
  gpointer              (* init_measure)        (const GskContour       *contour,
                                                 float                   tolerance,
                                                 float                  *out_length);
  void                  (* free_measure)        (const GskContour       *contour,
                                                 gpointer                measure_data);
  void                  (* add_segment)         (const GskContour       *contour,
                                                 GskPathBuilder         *builder,
                                                 gpointer                measure_data,
                                                 gboolean                emit_move_to,
                                                 float                   start,
                                                 float                   end);
  void                  (* get_point)           (const GskContour       *contour,
                                                 gpointer                measure_data,
                                                 float                   offset,
                                                 GskRealPathPoint       *result);
  float                 (* get_distance)        (const GskContour       *contour,
                                                 GskRealPathPoint       *point,
                                                 gpointer                measure_data);
};

/* {{{ Utilities */

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))
#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))

static void
_g_string_append_double (GString *string,
                         double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, d);
  g_string_append (string, buf);
}

static void
_g_string_append_point (GString                *string,
                        const graphene_point_t *pt)
{
  _g_string_append_double (string, pt->x);
  g_string_append_c (string, ' ');
  _g_string_append_double (string, pt->y);
}


/* }}} */
/* {{{ Standard */

typedef struct _GskStandardContour GskStandardContour;
struct _GskStandardContour
{
  GskContour contour;

  GskPathFlags flags;

  gsize n_ops;
  gsize n_points;
  graphene_point_t *points;
  gskpathop ops[];
};

static gsize
gsk_standard_contour_compute_size (gsize n_ops,
                                   gsize n_points)
{
  return sizeof (GskStandardContour)
       + sizeof (gskpathop) * n_ops
       + sizeof (graphene_point_t) * n_points;
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
             gpointer                user_data)
{
  GskPathBuilder *builder = user_data;
  GskCurve c, r;

  if (op == GSK_PATH_MOVE)
    return TRUE;

  if (op == GSK_PATH_CLOSE)
    op = GSK_PATH_LINE;

  gsk_curve_init_foreach (&c, op, pts, n_pts);
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

static void
gsk_standard_contour_print (const GskContour *contour,
                            GString          *string)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  for (i = 0; i < self->n_ops; i ++)
    {
      const graphene_point_t *pt = gsk_pathop_points (self->ops[i]);

      switch (gsk_pathop_op (self->ops[i]))
      {
        case GSK_PATH_MOVE:
          g_string_append (string, "M ");
          _g_string_append_point (string, &pt[0]);
          break;

        case GSK_PATH_CLOSE:
          g_string_append (string, " Z");
          break;

        case GSK_PATH_LINE:
          g_string_append (string, " L ");
          _g_string_append_point (string, &pt[1]);
          break;

        case GSK_PATH_QUAD:
          g_string_append (string, " Q ");
          _g_string_append_point (string, &pt[1]);
          g_string_append (string, ", ");
          _g_string_append_point (string, &pt[2]);
          break;

        case GSK_PATH_CUBIC:
          g_string_append (string, " C ");
          _g_string_append_point (string, &pt[1]);
          g_string_append (string, ", ");
          _g_string_append_point (string, &pt[2]);
          g_string_append (string, ", ");
          _g_string_append_point (string, &pt[3]);
          break;

        default:
          g_assert_not_reached();
          return;
      }
    }
}

static gboolean
gsk_standard_contour_get_bounds (const GskContour *contour,
                                 GskBoundingBox   *bounds)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  if (self->n_points == 0)
    return FALSE;

  gsk_bounding_box_init (bounds,  &self->points[0], &self->points[0]);
  for (i = 1; i < self->n_points; i ++)
    gsk_bounding_box_expand (bounds, &self->points[i]);

  return bounds->max.x > bounds->min.x && bounds->max.y > bounds->min.y;
}

static gboolean
add_stroke_bounds (GskPathOperation        op,
                   const graphene_point_t *pts,
                   gsize                   n_pts,
                   gpointer                user_data)
{
  struct {
    GskBoundingBox *bounds;
    float lw;
    float mw;
  } *data = user_data;
  GskBoundingBox bounds;

  for (int i = 1; i < n_pts - 1; i++)
    {
      gsk_bounding_box_init (&bounds,
                             &GRAPHENE_POINT_INIT (pts[i].x - data->lw/2, pts[i].y - data->lw/2),
                             &GRAPHENE_POINT_INIT (pts[i].x + data->lw/2, pts[i].y + data->lw/2));
      gsk_bounding_box_union (&bounds, data->bounds, data->bounds);
    }

  gsk_bounding_box_init (&bounds,
                         &GRAPHENE_POINT_INIT (pts[n_pts - 1].x - data->mw/2, pts[n_pts  - 1].y - data->mw/2),
                         &GRAPHENE_POINT_INIT (pts[n_pts - 1].x + data->mw/2, pts[n_pts  - 1].y + data->mw/2));
  gsk_bounding_box_union (&bounds, data->bounds, data->bounds);

  return TRUE;
}

static gboolean
gsk_standard_contour_get_stroke_bounds (const GskContour *contour,
                                        const GskStroke  *stroke,
                                        GskBoundingBox   *bounds)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  struct {
    GskBoundingBox *bounds;
    float lw;
    float mw;
  } data;

  data.bounds = bounds;
  data.lw = stroke->line_width;
  data.mw = gsk_stroke_get_join_width (stroke);

  gsk_bounding_box_init (bounds,
                         &GRAPHENE_POINT_INIT (self->points[0].x - data.mw/2, self->points[0].y - data.mw/2),
                         &GRAPHENE_POINT_INIT (self->points[0].x + data.mw/2, self->points[0].y + data.mw/2));

  gsk_standard_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, add_stroke_bounds, &data);

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

typedef struct
{
  float start;
  float end;
  float start_progress;
  float end_progress;
  GskCurveLineReason reason;
  graphene_point_t start_point;
  graphene_point_t end_point;
  gsize op;
} GskStandardContourMeasure;

typedef struct
{
  GArray *array;
  GskStandardContourMeasure measure;
} LengthDecompose;

static void
gsk_standard_contour_measure_get_point_at (GskStandardContourMeasure *measure,
                                           float                      progress,
                                           graphene_point_t          *out_point)
{
  graphene_point_interpolate (&measure->start_point,
                              &measure->end_point,
                              (progress - measure->start) / (measure->end - measure->start),
                              out_point);
}

static gboolean
gsk_standard_contour_measure_add_point (const graphene_point_t *from,
                                        const graphene_point_t *to,
                                        float                   from_progress,
                                        float                   to_progress,
                                        GskCurveLineReason      reason,
                                        gpointer                user_data)
{
  LengthDecompose *decomp = user_data;
  float seg_length;

  seg_length = graphene_point_distance (from, to, NULL, NULL);
  if (seg_length == 0)
    return TRUE;

  decomp->measure.end += seg_length;
  decomp->measure.start_progress = from_progress;
  decomp->measure.end_progress = to_progress;
  decomp->measure.start_point = *from;
  decomp->measure.end_point = *to;
  decomp->measure.reason = reason;

  g_array_append_val (decomp->array, decomp->measure);

  decomp->measure.start += seg_length;

  return TRUE;
}

static gpointer
gsk_standard_contour_init_measure (const GskContour *contour,
                                   float             tolerance,
                                   float            *out_length)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;
  float length;
  GArray *array;

  array = g_array_new (FALSE, FALSE, sizeof (GskStandardContourMeasure));
  length = 0;

  for (i = 1; i < self->n_ops; i ++)
    {
      GskCurve curve;
      LengthDecompose decomp = { array, { length, length, 0, 0, GSK_CURVE_LINE_REASON_SHORT, { 0, 0 }, { 0, 0 }, i } };

      gsk_curve_init (&curve, self->ops[i]);
      gsk_curve_decompose (&curve, tolerance, gsk_standard_contour_measure_add_point, &decomp);
      length = decomp.measure.start;
    }

  *out_length = length;

  return array;
}

static void
gsk_standard_contour_free_measure (const GskContour *contour,
                                   gpointer          data)
{
  g_array_free (data, TRUE);
}

static int
gsk_standard_contour_find_measure (gconstpointer m,
                                   gconstpointer l)
{
  const GskStandardContourMeasure *measure = m;
  float length = *(const float *) l;

  if (measure->start > length)
    return 1;
  else if (measure->end <= length)
    return -1;
  else
    return 0;
}

static void
gsk_standard_contour_add_segment (const GskContour *contour,
                                  GskPathBuilder   *builder,
                                  gpointer          measure_data,
                                  gboolean          emit_move_to,
                                  float             start,
                                  float             end)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GArray *array = measure_data;
  guint start_index, end_index;
  float start_progress, end_progress;
  GskStandardContourMeasure *start_measure, *end_measure;
  gsize i;

  if (start > 0)
    {
      if (!g_array_binary_search (array, (float[1]) { start }, gsk_standard_contour_find_measure, &start_index))
        start_index = array->len - 1;
      start_measure = &g_array_index (array, GskStandardContourMeasure, start_index);
      start_progress = (start - start_measure->start) / (start_measure->end - start_measure->start);
      start_progress = start_measure->start_progress + (start_measure->end_progress - start_measure->start_progress) * start_progress;
      g_assert (start_progress >= 0 && start_progress <= 1);
    }
  else
    {
      start_measure = NULL;
      start_progress = 0.0;
    }

  if (g_array_binary_search (array, (float[1]) { end }, gsk_standard_contour_find_measure, &end_index))
    {
      end_measure = &g_array_index (array, GskStandardContourMeasure, end_index);
      end_progress = (end - end_measure->start) / (end_measure->end - end_measure->start);
      end_progress = end_measure->start_progress + (end_measure->end_progress - end_measure->start_progress) * end_progress;
      g_assert (end_progress >= 0 && end_progress <= 1);
    }
  else
    {
      end_measure = NULL;
      end_progress = 1.0;
    }

  /* Add the first partial operation,
   * taking care that first and last operation might be identical */
  if (start_measure)
    {
      GskCurve curve, cut;
      const graphene_point_t *start_point;

      gsk_curve_init (&curve, self->ops[start_measure->op]);

      if (start_measure->reason == GSK_CURVE_LINE_REASON_STRAIGHT)
        {
          graphene_point_t p;

          gsk_standard_contour_measure_get_point_at (start_measure, start, &p);
          if (emit_move_to)
            gsk_path_builder_move_to (builder, p.x, p.y);

          if (end_measure == start_measure)
            {
              gsk_standard_contour_measure_get_point_at (end_measure, end, &p);
              gsk_path_builder_line_to (builder, p.x, p.y);
              return;
            }
          else
            {
              gsk_path_builder_line_to (builder,
                                        start_measure->end_point.x,
                                        start_measure->end_point.y);
              start_index++;
              if (start_index >= array->len)
                return;

              start_measure++;
              start_progress = start_measure->start_progress;
              emit_move_to = FALSE;
              gsk_curve_init (&curve, self->ops[start_measure->op]);
            }
        }

      if (end_measure && end_measure->op == start_measure->op)
        {
          if (end_measure->reason == GSK_CURVE_LINE_REASON_SHORT)
            {
              gsk_curve_segment (&curve, start_progress, end_progress, &cut);
              if (emit_move_to)
                {
                  start_point = gsk_curve_get_start_point (&cut);
                  gsk_path_builder_move_to (builder, start_point->x, start_point->y);
                }
              gsk_curve_builder_to (&cut, builder);
            }
          else
            {
              graphene_point_t p;

              gsk_curve_segment (&curve, start_progress, end_measure->start_progress, &cut);
              if (emit_move_to)
                {
                  start_point = gsk_curve_get_start_point (&cut);
                  gsk_path_builder_move_to (builder, start_point->x, start_point->y);
                }
              gsk_curve_builder_to (&cut, builder);

              gsk_standard_contour_measure_get_point_at (end_measure, end, &p);
              gsk_path_builder_line_to (builder, p.x, p.y);
            }
          return;
        }

      gsk_curve_split (&curve, start_progress, NULL, &cut);

      start_point = gsk_curve_get_start_point (&cut);
      if (emit_move_to)
        gsk_path_builder_move_to (builder, start_point->x, start_point->y);
      gsk_curve_builder_to (&cut, builder);
      i = start_measure->op + 1;
    }
  else 
    i = emit_move_to ? 0 : 1;

  for (; i < (end_measure ? end_measure->op : self->n_ops - 1); i++)
    {
      gsk_path_builder_pathop_to (builder, self->ops[i]);
    }

  /* Add the last partial operation */
  if (end_measure)
    {
      GskCurve curve, cut;

      gsk_curve_init (&curve, self->ops[end_measure->op]);

      if (end_measure->reason == GSK_CURVE_LINE_REASON_SHORT)
        {
          gsk_curve_split (&curve, end_progress, &cut, NULL);
          gsk_curve_builder_to (&cut, builder);
        }
      else
        {
          graphene_point_t p;
          gsk_curve_split (&curve, end_measure->start_progress, &cut, NULL);
          gsk_curve_builder_to (&cut, builder);

          gsk_standard_contour_measure_get_point_at (end_measure, end, &p);
          gsk_path_builder_line_to (builder, p.x, p.y);
        }
    }
  else if (i == self->n_ops - 1)
    {
      gskpathop op = self->ops[i];
      if (gsk_pathop_op (op) == GSK_PATH_CLOSE)
        gsk_path_builder_pathop_to (builder, gsk_pathop_encode (GSK_PATH_LINE, gsk_pathop_points (op)));
      else
        gsk_path_builder_pathop_to (builder, op);
    }
}

static int
gsk_standard_contour_get_winding (const GskContour       *contour,
                                  const graphene_point_t *point)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  int winding = 0;

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
          result->data.std.idx = 0;
          result->data.std.t = 0;
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
      result->data.std.idx = best_idx;
      result->data.std.t = best_t;
      return TRUE;
    }

  return FALSE;
}

static void
gsk_standard_contour_get_point (const GskContour *contour,
                                gpointer          measure_data,
                                float             distance,
                                GskRealPathPoint *result)
{
  GArray *array = measure_data;
  unsigned int idx;
  GskStandardContourMeasure *measure;
  float fraction, t;

  if (array->len == 0)
    {
      result->data.std.idx = 0;
      result->data.std.t = 0;
      return;
    }

  if (!g_array_binary_search (array, &distance, gsk_standard_contour_find_measure, &idx))
    idx = array->len - 1;

  measure = &g_array_index (array, GskStandardContourMeasure, idx);

  fraction = (distance - measure->start) / (measure->end - measure->start);
  t = measure->start_progress + fraction * (measure->end_progress - measure->start_progress);

  g_assert (t >= 0 && t <= 1);

  result->data.std.idx = measure->op;
  result->data.std.t = t;
}

static void
gsk_standard_contour_get_position (const GskContour *contour,
                                   GskRealPathPoint *point,
                                   graphene_point_t *position)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve curve;

  if (G_UNLIKELY (point->data.std.idx == 0))
    {
      *position = self->points[0];
      return;
    }

  gsk_curve_init (&curve, self->ops[point->data.std.idx]);
  gsk_curve_get_point (&curve, point->data.std.t, position);
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

  if (G_UNLIKELY (point->data.std.idx == 0))
    {
      graphene_vec2_init (tangent, 1, 0);
      return;
    }

  idx = point->data.std.idx;
  t = point->data.std.t;

  if (t == 0 && direction == GSK_PATH_START)
    {
      /* Look at the previous segment (0 is always a move, so skip it) */
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
  else if (t == 1 && direction == GSK_PATH_END)
    {
      /* Look at the next segment */
      if (idx < self->n_ops - 1)
        {
          idx++;
          t = 0;
        }
      else if (self->flags & GSK_PATH_CLOSED)
        {
          /* segment 0 is always a move */
          idx = 1;
          t = 0;
        }
    }

  gsk_curve_init (&curve, self->ops[idx]);
  gsk_curve_get_tangent (&curve, t, tangent);
}

static float
gsk_standard_contour_get_curvature (const GskContour *contour,
                                    GskRealPathPoint *point,
                                    graphene_point_t *center)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskCurve curve;

  if (G_UNLIKELY (point->data.std.idx == 0))
    return 0;

  gsk_curve_init (&curve, self->ops[point->data.std.idx]);
  return gsk_curve_get_curvature (&curve, point->data.std.t, center);
}

static float
gsk_standard_contour_get_distance (const GskContour *contour,
                                   GskRealPathPoint *point,
                                   gpointer          measure_data)
{
  GArray *array = measure_data;

  if (G_UNLIKELY (point->data.std.idx == 0))
    return 0;

  for (unsigned int i = 0; i < array->len; i++)
    {
      GskStandardContourMeasure *measure = &g_array_index (array, GskStandardContourMeasure, i);
      float fraction;

      if (measure->op != point->data.std.idx)
        continue;

      if (measure->end_progress < point->data.std.t)
        continue;

      g_assert (measure->op == point->data.std.idx);
      g_assert (measure->start_progress <= point->data.std.t && point->data.std.t <= measure->end_progress);

      fraction = (point->data.std.t - measure->start_progress) / (measure->end_progress - measure->start_progress);
      return measure->start + fraction * (measure->end - measure->start);
    }

  return 0;
}

static const GskContourClass GSK_STANDARD_CONTOUR_CLASS =
{
  sizeof (GskStandardContour),
  "GskStandardContour",
  gsk_standard_contour_copy,
  gsk_standard_contour_get_size,
  gsk_standard_contour_get_flags,
  gsk_standard_contour_print,
  gsk_standard_contour_get_bounds,
  gsk_standard_contour_get_stroke_bounds,
  gsk_standard_contour_get_start_end,
  gsk_standard_contour_foreach,
  gsk_standard_contour_reverse,
  gsk_standard_contour_get_winding,
  gsk_standard_contour_get_closest_point,
  gsk_standard_contour_get_position,
  gsk_standard_contour_get_tangent,
  gsk_standard_contour_get_curvature,
  gsk_standard_contour_init_measure,
  gsk_standard_contour_free_measure,
  gsk_standard_contour_add_segment,
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
  gsize i;

  self->contour.klass = &GSK_STANDARD_CONTOUR_CLASS;

  self->flags = flags;
  self->n_ops = n_ops;
  self->n_points = n_points;
  self->points = (graphene_point_t *) &self->ops[n_ops];
  memcpy (self->points, points, sizeof (graphene_point_t) * n_points);

  offset += self->points - points;
  for (i = 0; i < n_ops; i++)
    {
      self->ops[i] = gsk_pathop_encode (gsk_pathop_op (ops[i]),
                                        gsk_pathop_points (ops[i]) + offset);
    }
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
/* {{{ API */

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
gsk_contour_get_start_end (const GskContour *self,
                           graphene_point_t *start,
                           graphene_point_t *end)
{
  self->klass->get_start_end (self, start, end);
}

void
gsk_contour_add_segment (const GskContour *self,
                         GskPathBuilder   *builder,
                         gpointer          measure_data,
                         gboolean          emit_move_to,
                         float             start,
                         float             end)
{
  self->klass->add_segment (self, builder, measure_data, emit_move_to, start, end);
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

void
gsk_contour_get_point (const GskContour *self,
                       gpointer          measure_data,
                       float             offset,
                       GskRealPathPoint *result)
{
  self->klass->get_point (self, measure_data, offset, result);
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
                           graphene_point_t *center)
{
  return self->klass->get_curvature (self, point, center);
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
