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
                                                 graphene_rect_t        *bounds);
  gboolean              (* get_stroke_bounds)   (const GskContour       *contour,
                                                 const GskStroke        *stroke,
                                                 graphene_rect_t        *bounds);
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

static gsize
gsk_contour_get_size_default (const GskContour *contour)
{
  return contour->klass->struct_size;
}

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

        case GSK_PATH_CONIC:
          /* This is not valid SVG */
          g_string_append (string, " O ");
          _g_string_append_point (string, &pt[1]);
          g_string_append (string, ", ");
          _g_string_append_point (string, &pt[3]);
          g_string_append (string, ", ");
          _g_string_append_double (string, pt[2].x);
          break;

        default:
          g_assert_not_reached();
          return;
      }
    }
}

static void
rect_add_point (graphene_rect_t        *rect,
                const graphene_point_t *point)
{
  if (point->x < rect->origin.x)
    {
      rect->size.width += rect->origin.x - point->x;
      rect->origin.x = point->x;
    }
  else if (point->x > rect->origin.x + rect->size.width)
    {
      rect->size.width = point->x - rect->origin.x;
    }

  if (point->y < rect->origin.y)
    {
      rect->size.height += rect->origin.y - point->y;
      rect->origin.y = point->y;
    }
  else if (point->y > rect->origin.y + rect->size.height)
    {
      rect->size.height = point->y - rect->origin.y;
    }
}

static gboolean
gsk_standard_contour_get_bounds (const GskContour *contour,
                                 graphene_rect_t  *bounds)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;

  if (self->n_points == 0)
    return FALSE;

  graphene_rect_init (bounds,
                      self->points[0].x, self->points[0].y,
                      0, 0);

  for (i = 1; i < self->n_points; i ++)
    {
      rect_add_point (bounds, &self->points[i]);
    }

  return bounds->size.width > 0 && bounds->size.height > 0;
}

static gboolean
add_stroke_bounds (GskPathOperation        op,
                   const graphene_point_t *pts,
                   gsize                   n_pts,
                   float                   weight,
                   gpointer                user_data)
{
  struct {
    graphene_rect_t *bounds;
    float lw;
    float mw;
  } *data = user_data;
  graphene_rect_t bounds;

  for (int i = 1; i < n_pts - 1; i++)
    {
      graphene_rect_init (&bounds, pts[i].x - data->lw/2, pts[i].y - data->lw/2, data->lw, data->lw);
      graphene_rect_union (&bounds, data->bounds, data->bounds);
    }

  graphene_rect_init (&bounds, pts[n_pts - 1].x - data->mw/2, pts[n_pts  - 1].y - data->mw/2, data->mw, data->mw);
  graphene_rect_union (&bounds, data->bounds, data->bounds);

  return TRUE;
}

static gboolean
gsk_standard_contour_get_stroke_bounds (const GskContour *contour,
                                        const GskStroke  *stroke,
                                        graphene_rect_t  *bounds)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  struct {
    graphene_rect_t *bounds;
    float lw;
    float mw;
  } data;

  data.bounds = bounds;
  data.lw = stroke->line_width;
  data.mw = MAX (stroke->miter_limit, 1.f) * stroke->line_width;

  graphene_rect_init (bounds, self->points[0].x - data.mw/2, self->points[0].y - data.mw/2, data.mw, data.mw);

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
      if (gsk_curve_get_closest_point (&c, point, threshold, &distance, &t))
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

  if (G_UNLIKELY (point->data.std.idx == 0))
    {
      graphene_vec2_init (tangent, 1, 0);
      return;
    }

  gsk_curve_init (&curve, self->ops[point->data.std.idx]);
  gsk_curve_get_tangent (&curve, point->data.std.t, tangent);
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
/* {{{ Rectangle */

typedef struct _GskRectContour GskRectContour;
struct _GskRectContour
{
  GskContour contour;

  float x;
  float y;
  float width;
  float height;
};

static void
gsk_rect_contour_copy (const GskContour *contour,
                       GskContour       *dest)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  GskRectContour *target = (GskRectContour *) dest;

  *target = *self;
}

static GskPathFlags
gsk_rect_contour_get_flags (const GskContour *contour)
{
  return GSK_PATH_FLAT | GSK_PATH_CLOSED;
}

static void
gsk_rect_contour_print (const GskContour *contour,
                        GString          *string)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  g_string_append (string, "M ");
  _g_string_append_point (string, &GRAPHENE_POINT_INIT (self->x, self->y));
  g_string_append (string, " h ");
  _g_string_append_double (string, self->width);
  g_string_append (string, " v ");
  _g_string_append_double (string, self->height);
  g_string_append (string, " h ");
  _g_string_append_double (string, - self->width);
  g_string_append (string, " z");
}

static gboolean
gsk_rect_contour_get_bounds (const GskContour *contour,
                             graphene_rect_t  *rect)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  graphene_rect_init (rect, self->x, self->y, self->width, self->height);

  return TRUE;
}

static gboolean
gsk_rect_contour_get_stroke_bounds (const GskContour *contour,
                                    const GskStroke  *stroke,
                                    graphene_rect_t  *bounds)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  graphene_rect_init (bounds, self->x, self->y, self->width, self->height);
  graphene_rect_inset (bounds, - stroke->line_width / 2, - stroke->line_width / 2);
  return TRUE;
}

static void
gsk_rect_contour_get_start_end (const GskContour *contour,
                                graphene_point_t *start,
                                graphene_point_t *end)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  if (start)
    *start = GRAPHENE_POINT_INIT (self->x, self->y);

  if (end)
    *end = GRAPHENE_POINT_INIT (self->x, self->y);
}

static gboolean
gsk_rect_contour_foreach (const GskContour   *contour,
                          float               tolerance,
                          GskPathForeachFunc  func,
                          gpointer            user_data)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  graphene_point_t pts[] = {
    GRAPHENE_POINT_INIT (self->x,               self->y),
    GRAPHENE_POINT_INIT (self->x + self->width, self->y),
    GRAPHENE_POINT_INIT (self->x + self->width, self->y + self->height),
    GRAPHENE_POINT_INIT (self->x,               self->y + self->height),
    GRAPHENE_POINT_INIT (self->x,               self->y)
  };

  return func (GSK_PATH_MOVE, &pts[0], 1, 0, user_data)
      && func (GSK_PATH_LINE, &pts[0], 2, 0, user_data)
      && func (GSK_PATH_LINE, &pts[1], 2, 0, user_data)
      && func (GSK_PATH_LINE, &pts[2], 2, 0, user_data)
      && func (GSK_PATH_CLOSE, &pts[3], 2, 0, user_data);
}

static GskContour *
gsk_rect_contour_reverse (const GskContour *contour)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  return gsk_rect_contour_new (&GRAPHENE_RECT_INIT (self->x + self->width,
                                                    self->y,
                                                    - self->width,
                                                    self->height));
}

static gpointer
gsk_rect_contour_init_measure (const GskContour *contour,
                               float             tolerance,
                               float            *out_length)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  *out_length = 2 * ABS (self->width) + 2 * ABS (self->height);

  return NULL;
}

static void
gsk_rect_contour_free_measure (const GskContour *contour,
                               gpointer          data)
{
}

static void
gsk_rect_contour_point_tangent (const GskRectContour *self,
                                float                 distance,
                                GskPathDirection      direction,
                                graphene_point_t     *pos,
                                graphene_vec2_t      *tangent)
{
  if (distance == 0)
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x, self->y);

      if (tangent)
        {
          if (direction == GSK_PATH_START)
            graphene_vec2_init (tangent, 0.0f, - copysignf (1.0f, self->height));
          else
            graphene_vec2_init (tangent, copysignf (1.0f, self->width), 0.0f);
        }
      return;
    }

  if (distance < fabsf (self->width))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + copysignf (distance, self->width), self->y);
      if (tangent)
        graphene_vec2_init (tangent, copysignf (1.0f, self->width), 0.0f);
      return;
    }
  distance -= fabsf (self->width);

  if (distance == 0)
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width, self->y);

      if (tangent)
        {
          if (direction == GSK_PATH_START)
            graphene_vec2_init (tangent, copysignf (1.0f, self->width), 0.0f);
          else
            graphene_vec2_init (tangent, 0.0f, copysignf (1.0f, self->height));
        }
      return;
    }

  if (distance < fabsf (self->height))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width, self->y + copysignf (distance, self->height));
      if (tangent)
        graphene_vec2_init (tangent, 0.0f, copysignf (1.0f, self->height));
      return;
    }
  distance -= fabs (self->height);

  if (distance == 0)
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width, self->y + self->height);

      if (tangent)
        {
          if (direction == GSK_PATH_START)
            graphene_vec2_init (tangent, 0.0f, copysignf (1.0f, self->height));
          else
            graphene_vec2_init (tangent, - copysignf (1.0f, self->width), 0.0f);
        }
      return;
    }

  if (distance < fabsf (self->width))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width - copysignf (distance, self->width), self->y + self->height);
      if (tangent)
        graphene_vec2_init (tangent, - copysignf (1.0f, self->width), 0.0f);
      return;
    }
  distance -= fabsf (self->width);

  if (distance == 0)
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x, self->y + self->height);

      if (tangent)
        {
          if (direction == GSK_PATH_START)
            graphene_vec2_init (tangent, - copysignf (1.0f, self->width), 0.0f);
          else
            graphene_vec2_init (tangent, 0.0f, - copysignf (1.0f, self->height));
        }
      return;
    }

  if (distance < fabsf (self->height))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x, self->y + self->height - copysignf (distance, self->height));
      if (tangent)
        graphene_vec2_init (tangent, 0.0f, - copysignf (1.0f, self->height));
      return;
    }

  if (pos)
    *pos = GRAPHENE_POINT_INIT (self->x, self->y);

  if (tangent)
    {
      if (direction == GSK_PATH_START)
        graphene_vec2_init (tangent, 0.0f, - copysignf (1.0f, self->height));
      else
        graphene_vec2_init (tangent, copysignf (1.0f, self->width), 0.0f);
    }
}

static gboolean
gsk_rect_contour_closest_point (const GskRectContour   *self,
                                const graphene_point_t *point,
                                float                   threshold,
                                float                  *out_distance,
                                float                  *out_offset)
{
  graphene_point_t t, p;
  float distance;

  /* offset coords to be relative to rectangle */
  t.x = point->x - self->x;
  t.y = point->y - self->y;

  if (self->width)
    {
      /* do unit square math */
      t.x /= self->width;
      /* move point onto the square */
      t.x = CLAMP (t.x, 0.f, 1.f);
    }
  else
    t.x = 0.f;

  if (self->height)
    {
      t.y /= self->height;
      t.y = CLAMP (t.y, 0.f, 1.f);
    }
  else
    t.y = 0.f;

  if (t.x > 0 && t.x < 1 && t.y > 0 && t.y < 1)
    {
      float diff = MIN (t.x, 1.f - t.x) * ABS (self->width) - MIN (t.y, 1.f - t.y) * ABS (self->height);

      if (diff < 0.f)
        t.x = ceilf (t.x - 0.5f); /* round 0.5 down */
      else if (diff > 0.f)
        t.y = roundf (t.y); /* round 0.5 up */
      else
        {
          /* at least 2 points match, return the first one in the stroke */
          if (t.y <= 1.f - t.y)
            t.y = 0.f;
          else if (1.f - t.x <= t.x)
            t.x = 1.f;
          else
            t.y = 1.f;
        }
    }

  /* Don't let -0 confuse us */
  t.x = fabs (t.x);
  t.y = fabs (t.y);

  p = GRAPHENE_POINT_INIT (self->x + t.x * self->width,
                           self->y + t.y * self->height);

  distance = graphene_point_distance (point, &p, NULL, NULL);
  if (distance > threshold)
    return FALSE;

  *out_distance = distance;

  if (t.y == 0)
    *out_offset = t.x * fabs (self->width);
  else if (t.y == 1)
    *out_offset = (2 - t.x) * fabs (self->width) + fabs (self->height);
  else if (t.x == 1)
    *out_offset = fabs (self->width) + t.y * fabs (self->height);
  else
    *out_offset = 2 * fabs (self->width) + (2 - t.y) * fabs (self->height);

  return TRUE;
}

static void
gsk_rect_contour_add_segment (const GskContour *contour,
                              GskPathBuilder   *builder,
                              gpointer          measure_data,
                              gboolean          emit_move_to,
                              float             start,
                              float             end)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  float w = ABS (self->width);
  float h = ABS (self->height);

  if (start < w)
    {
      if (emit_move_to)
        gsk_path_builder_move_to (builder, self->x + start * (w / self->width), self->y);
      if (end <= w)
        {
          gsk_path_builder_line_to (builder, self->x + end * (w / self->width), self->y);
          return;
        }
      gsk_path_builder_line_to (builder, self->x + self->width, self->y);
    }
  start -= w;
  end -= w;

  if (start < h)
    {
      if (start >= 0 && emit_move_to)
        gsk_path_builder_move_to (builder, self->x + self->width, self->y + start * (h / self->height));
      if (end <= h)
        {
          gsk_path_builder_line_to (builder, self->x + self->width, self->y + end * (h / self->height));
          return;
        }
      gsk_path_builder_line_to (builder, self->x + self->width, self->y + self->height);
    }
  start -= h;
  end -= h;

  if (start < w)
    {
      if (start >= 0 && emit_move_to)
        gsk_path_builder_move_to (builder, self->x + (w - start) * (w / self->width), self->y + self->height);
      if (end <= w)
        {
          gsk_path_builder_line_to (builder, self->x + (w - end) * (w / self->width), self->y + self->height);
          return;
        }
      gsk_path_builder_line_to (builder, self->x, self->y + self->height);
    }
  start -= w;
  end -= w;

  if (start < h)
    {
      if (start >= 0 && emit_move_to)
        gsk_path_builder_move_to (builder, self->x, self->y + (h - start) * (h / self->height));
      if (end <= h)
        {
          gsk_path_builder_line_to (builder, self->x, self->y + (h - end) * (h / self->height));
          return;
        }
      gsk_path_builder_line_to (builder, self->x, self->y);
    }
}

static int
gsk_rect_contour_get_winding (const GskContour       *contour,
                              const graphene_point_t *point)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  graphene_rect_t rect;

  graphene_rect_init (&rect, self->x, self->y, self->width, self->height);

  if (graphene_rect_contains_point (&rect, point))
    return -1;

  return 0;
}

static gboolean
gsk_rect_contour_get_closest_point (const GskContour       *contour,
                                    const graphene_point_t *point,
                                    float                   threshold,
                                    GskRealPathPoint       *result,
                                    float                  *out_dist)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  float distance;

  if (gsk_rect_contour_closest_point (self, point, threshold, out_dist, &distance))
    {
      result->data.rect.distance = distance;
      return TRUE;
    }

  return FALSE;
}

static void
gsk_rect_contour_get_point (const GskContour *contour,
                            gpointer          measure_data,
                            float             offset,
                            GskRealPathPoint *result)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  result->data.rect.distance = CLAMP (offset, 0, 2 * (ABS (self->width) + ABS (self->height)));
}

static void
gsk_rect_contour_get_position (const GskContour *contour,
                               GskRealPathPoint *point,
                               graphene_point_t *position)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  gsk_rect_contour_point_tangent (self, point->data.rect.distance, GSK_PATH_END, position, NULL);
}

static void
gsk_rect_contour_get_tangent (const GskContour *contour,
                              GskRealPathPoint *point,
                              GskPathDirection  direction,
                              graphene_vec2_t  *tangent)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  gsk_rect_contour_point_tangent (self, point->data.rect.distance, direction, NULL, tangent);
}

static float
gsk_rect_contour_get_curvature (const GskContour *contour,
                                GskRealPathPoint *point,
                                graphene_point_t *center)
{
  return 0;
}

static float
gsk_rect_contour_get_distance (const GskContour *contour,
                               GskRealPathPoint *point,
                               gpointer          measure_data)
{
  return point->data.rect.distance;
}

static const GskContourClass GSK_RECT_CONTOUR_CLASS =
{
  sizeof (GskRectContour),
  "GskRectContour",
  gsk_rect_contour_copy,
  gsk_contour_get_size_default,
  gsk_rect_contour_get_flags,
  gsk_rect_contour_print,
  gsk_rect_contour_get_bounds,
  gsk_rect_contour_get_stroke_bounds,
  gsk_rect_contour_get_start_end,
  gsk_rect_contour_foreach,
  gsk_rect_contour_reverse,
  gsk_rect_contour_get_winding,
  gsk_rect_contour_get_closest_point,
  gsk_rect_contour_get_position,
  gsk_rect_contour_get_tangent,
  gsk_rect_contour_get_curvature,
  gsk_rect_contour_init_measure,
  gsk_rect_contour_free_measure,
  gsk_rect_contour_add_segment,
  gsk_rect_contour_get_point,
  gsk_rect_contour_get_distance,
};

GskContour *
gsk_rect_contour_new (const graphene_rect_t *rect)
{
  GskRectContour *self;

  self = g_new0 (GskRectContour, 1);

  self->contour.klass = &GSK_RECT_CONTOUR_CLASS;

  self->x = rect->origin.x;
  self->y = rect->origin.y;
  self->width = rect->size.width;
  self->height = rect->size.height;

  return (GskContour *) self;
}

/* }}} */
/* {{{ Circle */

typedef struct _GskCircleContour GskCircleContour;
struct _GskCircleContour
{
  GskContour contour;

  graphene_point_t center;
  float radius;
  float start_angle; /* in degrees */
  float end_angle; /* start_angle +/- 360 */
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
  const GskCircleContour *self = (const GskCircleContour *) contour;

  /* XXX: should we explicitly close paths? */
  if (fabs (self->start_angle - self->end_angle) >= 360)
    return GSK_PATH_CLOSED;
  else
    return 0;
}

#define GSK_CIRCLE_POINT_INIT(self, angle) \
  GRAPHENE_POINT_INIT ((self)->center.x + cos (DEG_TO_RAD (angle)) * self->radius, \
                       (self)->center.y + sin (DEG_TO_RAD (angle)) * self->radius)

static void
gsk_circle_contour_print (const GskContour *contour,
                          GString          *string)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float mid_angle = (self->end_angle - self->start_angle) / 2;

  g_string_append (string, "M ");
  _g_string_append_point (string, &GSK_CIRCLE_POINT_INIT (self, self->start_angle));
  g_string_append (string, " A ");
  _g_string_append_point (string, &GRAPHENE_POINT_INIT (self->radius, self->radius));
  g_string_append_printf (string, " 0 0 %u ",
                          self->start_angle < self->end_angle ? 0 : 1);
  _g_string_append_point (string, &GSK_CIRCLE_POINT_INIT (self, mid_angle));
  g_string_append (string, " A ");
  _g_string_append_point (string, &GRAPHENE_POINT_INIT (self->radius, self->radius));
  g_string_append_printf (string, " 0 0 %u ",
                          self->start_angle < self->end_angle ? 0 : 1);
  _g_string_append_point (string, &GSK_CIRCLE_POINT_INIT (self, self->end_angle));
  if (fabs (self->start_angle - self->end_angle) >= 360)
    g_string_append (string, " z");
}

static gboolean
gsk_circle_contour_get_bounds (const GskContour *contour,
                               graphene_rect_t  *rect)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  /* XXX: handle partial circles */
  graphene_rect_init (rect,
                      self->center.x - self->radius,
                      self->center.y - self->radius,
                      2 * self->radius,
                      2 * self->radius);

  return TRUE;
}

static gboolean
gsk_circle_contour_get_stroke_bounds (const GskContour       *contour,
                                      const GskStroke        *stroke,
                                      graphene_rect_t        *bounds)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  graphene_rect_init (bounds,
                      self->center.x - self->radius,
                      self->center.y - self->radius,
                      2 * self->radius,
                      2 * self->radius);
  graphene_rect_inset (bounds, - stroke->line_width / 2, - stroke->line_width / 2);

  return TRUE;
}

static void
gsk_circle_contour_get_start_end (const GskContour *contour,
                                  graphene_point_t *start,
                                  graphene_point_t *end)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (start)
    *start = GSK_CIRCLE_POINT_INIT (self, self->start_angle);

  if (end)
    *end = GSK_CIRCLE_POINT_INIT (self, self->end_angle);
}

typedef struct
{
  GskPathForeachFunc func;
  gpointer           user_data;
} ForeachWrapper;

static gboolean
gsk_circle_contour_curve (const graphene_point_t curve[4],
                          gpointer               data)
{
  ForeachWrapper *wrapper = data;

  return wrapper->func (GSK_PATH_CUBIC, curve, 4, 0, wrapper->user_data);
}

static gboolean
gsk_circle_contour_foreach (const GskContour   *contour,
                            float               tolerance,
                            GskPathForeachFunc  func,
                            gpointer            user_data)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  graphene_point_t start = GSK_CIRCLE_POINT_INIT (self, self->start_angle);

  if (!func (GSK_PATH_MOVE, &start, 1, 0, user_data))
    return FALSE;

  if (!gsk_spline_decompose_arc (&self->center,
                                 self->radius,
                                 tolerance,
                                 DEG_TO_RAD (self->start_angle),
                                 DEG_TO_RAD (self->end_angle),
                                 gsk_circle_contour_curve,
                                 &(ForeachWrapper) { func, user_data }))
    return FALSE;

  if (fabs (self->start_angle - self->end_angle) >= 360)
    {
      if (!func (GSK_PATH_CLOSE, (graphene_point_t[2]) { start, start }, 2, 0, user_data))
        return FALSE;
    }

  return TRUE;
}

static GskContour *
gsk_circle_contour_reverse (const GskContour *contour)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  return gsk_circle_contour_new (&self->center,
                                 self->radius,
                                 self->end_angle,
                                 self->start_angle);
}

static gpointer
gsk_circle_contour_init_measure (const GskContour *contour,
                                 float             tolerance,
                                 float            *out_length)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  *out_length = DEG_TO_RAD (fabs (self->start_angle - self->end_angle)) * self->radius;

  return NULL;
}

static void
gsk_circle_contour_free_measure (const GskContour *contour,
                                 gpointer          data)
{
}

static gboolean
gsk_circle_contour_closest_point (const GskCircleContour *self,
                                  const graphene_point_t *point,
                                  float                   threshold,
                                  float                  *out_distance,
                                  float                  *out_angle)
{
  float angle;
  float closest_angle;
  graphene_point_t pos;
  float distance;

  if (graphene_point_distance (point, &self->center, NULL, NULL) > threshold + self->radius)
    return FALSE;

  angle = atan2f (point->y - self->center.y, point->x - self->center.x);
  angle = RAD_TO_DEG (angle);
  if (angle < 0)
    angle += 360;

  if ((self->start_angle <= angle && angle <= self->end_angle) ||
      (self->end_angle <= angle && angle <= self->start_angle))
    {
      closest_angle = angle;
    }
  else
    {
      float d1, d2;

      d1 = fabs (self->start_angle - angle);
      d1 = MIN (d1, 360 - d1);
      d2 = fabs (self->end_angle - angle);
      d2 = MIN (d2, 360 - d2);
      if (d1 < d2)
        closest_angle = self->start_angle;
      else
        closest_angle = self->end_angle;
    }

  pos = GSK_CIRCLE_POINT_INIT (self, closest_angle);

  distance = graphene_point_distance (&pos, point, NULL, NULL);
  if (threshold < distance)
    return FALSE;

  *out_distance = distance;
  *out_angle = closest_angle;

  return TRUE;
}

static gboolean
add_curve_to_segment (GskPathOperation        op,
                      const graphene_point_t *pts,
                      gsize                   n_pts,
                      float                   weight,
                      gpointer                data)
{
  GskPathBuilder *builder = data;
  GskCurve curve;

  gsk_curve_init_foreach (&curve, op, pts, n_pts, weight);
  gsk_curve_builder_to (&curve, builder);

  return TRUE;
}

static void
gsk_circle_contour_add_segment (const GskContour *contour,
                                GskPathBuilder   *builder,
                                gpointer          measure_data,
                                gboolean          emit_move_to,
                                float             start,
                                float             end)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float delta = self->end_angle - self->start_angle;
  float length = self->radius * DEG_TO_RAD (delta);
  float start_angle = self->start_angle + start/length * delta;
  float end_angle = self->start_angle + end/length * delta;

  if (emit_move_to)
    {
      GskContour *segment;

      segment = gsk_circle_contour_new (&self->center, self->radius,
                                        start_angle, end_angle);
      gsk_path_builder_add_contour (builder, segment);
    }
  else
    {
      /* convert to a standard contour */
      gsk_spline_decompose_arc (&self->center,
                                self->radius,
                                GSK_PATH_TOLERANCE_DEFAULT,
                                DEG_TO_RAD (start_angle),
                                DEG_TO_RAD (end_angle),
                                gsk_circle_contour_curve,
                                &(ForeachWrapper) { add_curve_to_segment, builder });
    }
}

static int
gsk_circle_contour_get_winding (const GskContour       *contour,
                                const graphene_point_t *point)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (graphene_point_distance (point, &self->center, NULL, NULL) >= self->radius)
    return 0;

  if (fabs (self->start_angle - self->end_angle) >= 360)
    {
      return -1;
    }
  else
    {
      /* Check if the point and the midpoint are on the same side
       * of the chord through start and end.
       */
      double mid_angle = (self->end_angle - self->start_angle) / 2;
      graphene_point_t start = GRAPHENE_POINT_INIT (self->center.x + cos (DEG_TO_RAD (self->start_angle)) * self->radius,
                                                    self->center.y + sin (DEG_TO_RAD (self->start_angle)) * self->radius);
      graphene_point_t mid = GRAPHENE_POINT_INIT (self->center.x + cos (DEG_TO_RAD (mid_angle)) * self->radius,
                                                  self->center.y + sin (DEG_TO_RAD (mid_angle)) * self->radius);
      graphene_point_t end = GRAPHENE_POINT_INIT (self->center.x + cos (DEG_TO_RAD (self->end_angle)) * self->radius,
                                                  self->center.y + sin (DEG_TO_RAD (self->end_angle)) * self->radius);

      graphene_vec2_t n, m;
      float a, b;

      graphene_vec2_init (&n, start.y - end.y, end.x - start.x);
      graphene_vec2_init (&m, mid.x, mid.y);
      a = graphene_vec2_dot (&m, &n);
      graphene_vec2_init (&m, point->x, point->y);
      b = graphene_vec2_dot (&m, &n);

      if ((a < 0) != (b < 0))
        return -1;
    }

  return 0;
}

typedef struct
{
  float angle;
} CirclePointData;

static gboolean
gsk_circle_contour_get_closest_point (const GskContour       *contour,
                                      const graphene_point_t *point,
                                      float                   threshold,
                                      GskRealPathPoint       *result,
                                      float                  *out_dist)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float dist, angle;

  if (gsk_circle_contour_closest_point (self, point, threshold, &dist, &angle))
    {
      result->data.circle.angle = angle;
      *out_dist = dist;
      return TRUE;
    }

  return FALSE;
}

static void
gsk_circle_contour_get_point (const GskContour *contour,
                              gpointer          measure_data,
                              float             distance,
                              GskRealPathPoint *result)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float delta = self->end_angle - self->start_angle;
  float length = self->radius * DEG_TO_RAD (delta);

  result->data.circle.angle = self->start_angle + distance/length * delta;
}

static void
gsk_circle_contour_get_position (const GskContour *contour,
                                 GskRealPathPoint *point,
                                 graphene_point_t *position)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  *position = GSK_CIRCLE_POINT_INIT (self, point->data.circle.angle);
}

static void
gsk_circle_contour_get_tangent (const GskContour *contour,
                                GskRealPathPoint *point,
                                GskPathDirection  direction,
                                graphene_vec2_t  *tangent)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  graphene_point_t pos = GSK_CIRCLE_POINT_INIT (self, point->data.circle.angle);

  graphene_vec2_init (tangent, pos.y - self->center.y, - pos.x + self->center.x);
  graphene_vec2_normalize (tangent, tangent);
}

static float
gsk_circle_contour_get_curvature (const GskContour *contour,
                                  GskRealPathPoint *point,
                                  graphene_point_t *center)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;

  if (center)
    *center = self->center;

  return 1 / self->radius;
}

static float
gsk_circle_contour_get_distance (const GskContour *contour,
                                 GskRealPathPoint *point,
                                 gpointer          measure_data)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float delta = self->end_angle - self->start_angle;
  float length = self->radius * DEG_TO_RAD (delta);

  return ((point->data.circle.angle - self->start_angle) / delta) * length;
}

static const GskContourClass GSK_CIRCLE_CONTOUR_CLASS =
{
  sizeof (GskCircleContour),
  "GskCircleContour",
  gsk_circle_contour_copy,
  gsk_contour_get_size_default,
  gsk_circle_contour_get_flags,
  gsk_circle_contour_print,
  gsk_circle_contour_get_bounds,
  gsk_circle_contour_get_stroke_bounds,
  gsk_circle_contour_get_start_end,
  gsk_circle_contour_foreach,
  gsk_circle_contour_reverse,
  gsk_circle_contour_get_winding,
  gsk_circle_contour_get_closest_point,
  gsk_circle_contour_get_position,
  gsk_circle_contour_get_tangent,
  gsk_circle_contour_get_curvature,
  gsk_circle_contour_init_measure,
  gsk_circle_contour_free_measure,
  gsk_circle_contour_add_segment,
  gsk_circle_contour_get_point,
  gsk_circle_contour_get_distance,
};

GskContour *
gsk_circle_contour_new (const graphene_point_t *center,
                        float                   radius,
                        float                   start_angle,
                        float                   end_angle)
{
  GskCircleContour *self;

  self = g_new0 (GskCircleContour, 1);

  self->contour.klass = &GSK_CIRCLE_CONTOUR_CLASS;

  g_assert (fabs (start_angle - end_angle) <= 360);

  self->contour.klass = &GSK_CIRCLE_CONTOUR_CLASS;
  self->center = *center;
  self->radius = radius;
  self->start_angle = start_angle;
  self->end_angle = end_angle;

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

static inline void
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

static void
gsk_rounded_rect_contour_print (const GskContour *contour,
                                GString          *string)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;
  graphene_point_t pts[13];

  get_rounded_rect_points (&self->rect, pts);

#define APPEND_MOVE(p) \
  g_string_append (string, "M "); \
  _g_string_append_point (string, p);
#define APPEND_LINE(p) \
  g_string_append (string, " L "); \
  _g_string_append_point (string, p);
#define APPEND_CONIC(p1, p2) \
  g_string_append (string, " O "); \
  _g_string_append_point (string, p1); \
  g_string_append (string, ", "); \
  _g_string_append_point (string, p2); \
  g_string_append (string, ", "); \
  _g_string_append_double (string, M_SQRT1_2);
#define APPEND_CLOSE g_string_append (string, " z");
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
      APPEND_MOVE (&pts[0]);
      APPEND_CONIC (&pts[1], &pts[2]);
      APPEND_LINE (&pts[3]);
      APPEND_CONIC (&pts[4], &pts[5]);
      APPEND_LINE (&pts[6]);
      APPEND_CONIC (&pts[7], &pts[8]);
      APPEND_LINE (&pts[9]);
      APPEND_CONIC (&pts[10], &pts[11]);
      APPEND_LINE (&pts[12]);
      APPEND_CLOSE
    }
  else
    {
      APPEND_MOVE (&pts[0]);
      APPEND_LINE (&pts[1]);
      APPEND_CONIC (&pts[2], &pts[3]);
      APPEND_LINE (&pts[4]);
      APPEND_CONIC (&pts[5], &pts[6]);
      APPEND_LINE (&pts[7]);
      APPEND_CONIC (&pts[8], &pts[9]);
      APPEND_LINE (&pts[10]);
      APPEND_CONIC (&pts[11], &pts[12]);
      APPEND_CLOSE;
    }
#undef APPEND_MOVE
#undef APPEND_LINE
#undef APPEND_CONIC
#undef APPEND_CLOSE
}

static gboolean
gsk_rounded_rect_contour_get_bounds (const GskContour *contour,
                                     graphene_rect_t  *rect)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  graphene_rect_init_from_rect (rect, &self->rect.bounds);

  return TRUE;
}

static gboolean
gsk_rounded_rect_contour_get_stroke_bounds (const GskContour *contour,
                                            const GskStroke  *stroke,
                                            graphene_rect_t  *bounds)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  graphene_rect_init_from_rect (bounds, &self->rect.bounds);
  graphene_rect_inset (bounds, - stroke->line_width / 2, - stroke->line_width / 2);

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

      return func (GSK_PATH_MOVE, &pts[0], 1, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[0], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[2], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[3], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[5], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[6], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[8], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[9], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[11], 2, 0, user_data) &&
             func (GSK_PATH_CLOSE, &pts[12], 2, 0, user_data);
    }
  else
    {
      return func (GSK_PATH_MOVE, &pts[0], 1, 0, user_data) &&
             func (GSK_PATH_LINE, &pts[0], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[1], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[3], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[4], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[6], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[7], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_LINE, &pts[9], 2, 0, user_data) &&
             func (GSK_PATH_CONIC, &pts[10], 3, M_SQRT1_2, user_data) &&
             func (GSK_PATH_CLOSE, &pts[12], 2, 0, user_data);
    }
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

  gsk_contour_foreach (contour, GSK_PATH_TOLERANCE_DEFAULT, init_curve_cb, &data);
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

typedef struct
{
  GskPath *path;
  const GskContour *contour;
  gpointer measure_data;
} RoundedRectMeasureData;

static gboolean
add_cb (GskPathOperation        op,
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
convert_to_standard_contour (const GskContour *contour,
                             float             tolerance)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_contour_foreach (contour, tolerance, add_cb, builder);
  path = gsk_path_builder_free_to_path (builder);

  return path;
}

static gpointer
gsk_rounded_rect_contour_init_measure (const GskContour *contour,
                                       float             tolerance,
                                       float            *out_length)
{
  RoundedRectMeasureData *data;


  data = g_new (RoundedRectMeasureData, 1);
  data->path = convert_to_standard_contour (contour, tolerance);
  data->contour = gsk_path_get_contour (data->path, 0);
  data->measure_data = gsk_standard_contour_init_measure (data->contour, tolerance, out_length);

  return data;
}

static void
gsk_rounded_rect_contour_free_measure (const GskContour *contour,
                                       gpointer          measure_data)
{
  RoundedRectMeasureData *data = measure_data;

  gsk_standard_contour_free_measure (data->contour, data->measure_data);
  gsk_path_unref (data->path);
  g_free (data);
}

static void
gsk_rounded_rect_contour_add_segment (const GskContour *contour,
                                      GskPathBuilder   *builder,
                                      gpointer          measure_data,
                                      gboolean          emit_move_to,
                                      float             start,
                                      float             end)
{
  RoundedRectMeasureData *data = measure_data;

  gsk_standard_contour_add_segment (data->contour,
                                    builder,
                                    data->measure_data,
                                    emit_move_to,
                                    start,
                                    end);
}

static int
gsk_rounded_rect_contour_get_winding (const GskContour       *contour,
                                      const graphene_point_t *point)
{
  const GskRoundedRectContour *self = (const GskRoundedRectContour *) contour;

  if (gsk_rounded_rect_contains_point (&self->rect, point))
    return self->ccw ? 1 : -1;

  return 0;
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

  path = convert_to_standard_contour (contour, GSK_PATH_TOLERANCE_DEFAULT);
  std = gsk_path_get_contour (path, 0);
  ret = gsk_standard_contour_get_closest_point (std, point, threshold, result, out_dist);
  gsk_path_unref (path);

  return ret;
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

static void
gsk_rounded_rect_contour_get_position (const GskContour *contour,
                                       GskRealPathPoint *point,
                                       graphene_point_t *position)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->data.std.idx, &curve);
  gsk_curve_get_point (&curve, point->data.std.t, position);
}

static void
gsk_rounded_rect_contour_get_tangent (const GskContour *contour,
                                      GskRealPathPoint *point,
                                      GskPathDirection  direction,
                                      graphene_vec2_t  *tangent)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->data.std.idx, &curve);
  gsk_curve_get_tangent (&curve, point->data.std.t, tangent);
}

static float
gsk_rounded_rect_contour_get_curvature (const GskContour *contour,
                                        GskRealPathPoint *point,
                                        graphene_point_t *center)
{
  GskCurve curve;

  gsk_rounded_rect_contour_init_curve (contour, point->data.std.idx, &curve);
  return gsk_curve_get_curvature (&curve, point->data.std.t, center);
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
  gsk_rounded_rect_contour_print,
  gsk_rounded_rect_contour_get_bounds,
  gsk_rounded_rect_contour_get_stroke_bounds,
  gsk_rounded_rect_contour_get_start_end,
  gsk_rounded_rect_contour_foreach,
  gsk_rounded_rect_contour_reverse,
  gsk_rounded_rect_contour_get_winding,
  gsk_rounded_rect_contour_get_closest_point,
  gsk_rounded_rect_contour_get_position,
  gsk_rounded_rect_contour_get_tangent,
  gsk_rounded_rect_contour_get_curvature,
  gsk_rounded_rect_contour_init_measure,
  gsk_rounded_rect_contour_free_measure,
  gsk_rounded_rect_contour_add_segment,
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
                        graphene_rect_t  *bounds)
{
  return self->klass->get_bounds (self, bounds);
}

gboolean
gsk_contour_get_stroke_bounds (const GskContour *self,
                               const GskStroke  *stroke,
                               graphene_rect_t  *bounds)
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
  result->contour = self;
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
