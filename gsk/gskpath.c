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

#include "gskpathprivate.h"

#include "gsksplineprivate.h"


typedef enum
{
  GSK_PATH_FLAT,
  GSK_PATH_CLOSED
} GskPathFlags;

typedef struct _GskContour GskContour;
typedef struct _GskContourClass GskContourClass;

struct _GskContour
{
  const GskContourClass *klass;
};

struct _GskContourClass
{
  gsize struct_size;
  const char *type_name;

  gsize                 (* get_size)            (const GskContour       *contour);
  GskPathFlags          (* get_flags)           (const GskContour       *contour);
  void                  (* print)               (const GskContour       *contour,
                                                 GString                *string);
  gboolean              (* get_bounds)          (const GskContour       *contour,
                                                 graphene_rect_t        *bounds);
  gboolean              (* foreach)             (const GskContour       *contour,
                                                 float                   tolerance,
                                                 GskPathForeachFunc      func,
                                                 gpointer                user_data);
  gpointer              (* init_measure)        (const GskContour       *contour,
                                                 float                   tolerance,
                                                 float                  *out_length);
  void                  (* free_measure)        (const GskContour       *contour,
                                                 gpointer                measure_data);
  void                  (* copy)                (const GskContour       *contour,
                                                 GskContour             *dest);
  void                  (* add_segment)         (const GskContour       *contour,
                                                 GskPathBuilder         *builder,
                                                 gpointer                measure_data,
                                                 float                   start,
                                                 float                   end);
};

struct _GskPath
{
  /*< private >*/
  guint ref_count;

  GskPathFlags flags;

  gsize n_contours;
  GskContour *contours[];
  /* followed by the contours data */
};

/**
 * GskPath:
 *
 * `GskPath` is used to describe lines and curves that are more
 * complex than simple rectangles.
 *
 * A `GskPath` struct is a reference counted struct
 * and should be treated as opaque.
 *
 * `GskPath` is an immutable struct. After creation, you cannot change
 * the types it represents. Instead, new `GskPath` have to be created.
 * The `GskPathBuilder` structure is meant to help in this endeavor.
 */

G_DEFINE_BOXED_TYPE (GskPath, gsk_path,
                     gsk_path_ref,
                     gsk_path_unref)

static gsize
gsk_contour_get_size_default (const GskContour *contour)
{
  return contour->klass->struct_size;
}

/* RECT CONTOUR */

typedef struct _GskRectContour GskRectContour;
struct _GskRectContour
{
  GskContour contour;

  float x;
  float y;
  float width;
  float height;
};

static GskPathFlags
gsk_rect_contour_get_flags (const GskContour *contour)
{
  return GSK_PATH_FLAT | GSK_PATH_CLOSED;
}

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

  return func (GSK_PATH_MOVE, &pts[0], 1, user_data)
      && func (GSK_PATH_LINE, &pts[0], 2, user_data)
      && func (GSK_PATH_LINE, &pts[1], 2, user_data)
      && func (GSK_PATH_LINE, &pts[2], 2, user_data)
      && func (GSK_PATH_CLOSE, &pts[3], 2, user_data);
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
gsk_rect_contour_copy (const GskContour *contour,
                       GskContour       *dest)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  GskRectContour *target = (GskRectContour *) dest;

  *target = *self;
}

static void
gsk_rect_contour_add_segment (const GskContour *contour,
                              GskPathBuilder   *builder,
                              gpointer          measure_data,
                              float             start,
                              float             end)
{
  const GskRectContour *self = (const GskRectContour *) contour;
  float w = ABS (self->width);
  float h = ABS (self->height);

  if (start < w)
    {
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
      if (start >= 0)
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
      if (start >= 0)
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
      if (start >= 0)
        gsk_path_builder_move_to (builder, self->x, self->y + (h - start) * (h / self->height));
      if (end <= h)
        {
          gsk_path_builder_line_to (builder, self->x, self->y + (h - end) * (h / self->height));
          return;
        }
      gsk_path_builder_line_to (builder, self->x, self->y);
    }
}

static const GskContourClass GSK_RECT_CONTOUR_CLASS =
{
  sizeof (GskRectContour),
  "GskRectContour",
  gsk_contour_get_size_default,
  gsk_rect_contour_get_flags,
  gsk_rect_contour_print,
  gsk_rect_contour_get_bounds,
  gsk_rect_contour_foreach,
  gsk_rect_contour_init_measure,
  gsk_rect_contour_free_measure,
  gsk_rect_contour_copy,
  gsk_rect_contour_add_segment
};

static void
gsk_rect_contour_init (GskContour *contour,
                       float       x,
                       float       y,
                       float       width,
                       float       height)
{
  GskRectContour *self = (GskRectContour *) contour;

  self->contour.klass = &GSK_RECT_CONTOUR_CLASS;
  self->x = x;
  self->y = y;
  self->width = width;
  self->height = height;
}

/* STANDARD CONTOUR */

typedef struct _GskStandardOperation GskStandardOperation;

struct _GskStandardOperation {
  GskPathOperation op;
  gsize point; /* index into points array of the start point (last point of previous op) */
};

typedef struct _GskStandardContour GskStandardContour;
struct _GskStandardContour
{
  GskContour contour;

  GskPathFlags flags;

  gsize n_ops;
  gsize n_points;
  graphene_point_t *points;
  GskStandardOperation ops[];
};

static gsize
gsk_standard_contour_compute_size (gsize n_ops,
                                   gsize n_points)
{
  return sizeof (GskStandardContour)
       + sizeof (GskStandardOperation) * n_ops
       + sizeof (graphene_point_t) * n_points;
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
  const gsize n_points[] = {
    [GSK_PATH_MOVE] = 1,
    [GSK_PATH_CLOSE] = 2,
    [GSK_PATH_LINE] = 2,
    [GSK_PATH_CURVE] = 4
  };

  for (i = 0; i < self->n_ops; i ++)
    {
      if (!func (self->ops[i].op, &self->points[self->ops[i].point], n_points[self->ops[i].op], user_data))
        return FALSE;
    }

  return TRUE;
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
      graphene_point_t *pt = &self->points[self->ops[i].point];

      switch (self->ops[i].op)
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

        case GSK_PATH_CURVE:
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

typedef struct
{
  float start;
  float end;
  float start_progress;
  float end_progress;
  graphene_point_t end_point;
  gsize op;
} GskStandardContourMeasure;

typedef struct
{
  GArray *array;
  GskStandardContourMeasure measure;
} LengthDecompose;

static void
gsk_standard_contour_measure_add_point (const graphene_point_t *from,
                                        const graphene_point_t *to,
                                        float                   from_progress,
                                        float                   to_progress,
                                        gpointer                user_data)
{
  LengthDecompose *decomp = user_data;
  float seg_length;

  seg_length = graphene_point_distance (from, to, NULL, NULL);
  decomp->measure.end += seg_length;
  decomp->measure.start_progress = from_progress;
  decomp->measure.end_progress = to_progress;
  decomp->measure.end_point = *to;

  g_array_append_val (decomp->array, decomp->measure);

  decomp->measure.start += seg_length;
}

static gpointer
gsk_standard_contour_init_measure (const GskContour *contour,
                                   float             tolerance,
                                   float            *out_length)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;
  gsize i;
  float length, seg_length;
  GArray *array;

  array = g_array_new (FALSE, FALSE, sizeof (GskStandardContourMeasure));
  length = 0;

  for (i = 1; i < self->n_ops; i ++)
    {
      graphene_point_t *pt = &self->points[self->ops[i].point];

      switch (self->ops[i].op)
      {
        case GSK_PATH_MOVE:
          break;

        case GSK_PATH_CLOSE:
        case GSK_PATH_LINE:
          seg_length = graphene_point_distance (&pt[0], &pt[1], NULL, NULL);
          if (seg_length > 0)
            {
              g_array_append_vals (array,
                                   &(GskStandardContourMeasure) {
                                     length,
                                     length + seg_length,
                                     0, 1,
                                     pt[1],
                                     i,
                                   }, 1);
              length += seg_length;
            }
          break;

        case GSK_PATH_CURVE:
          {
            LengthDecompose decomp = { array, { length, length, 0, 0, pt[0], i } };
            gsk_spline_decompose_cubic (pt, tolerance, gsk_standard_contour_measure_add_point, &decomp);
            length = decomp.measure.start;
          }
          break;

        default:
          g_assert_not_reached();
          return NULL;
      }
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

static void
gsk_standard_contour_init (GskContour *contour,
                           GskPathFlags flags,
                           const GskStandardOperation *ops,
                           gsize n_ops,
                           const graphene_point_t *points,
                           gsize n_points);

static void
gsk_standard_contour_copy (const GskContour *contour,
                           GskContour       *dest)
{
  const GskStandardContour *self = (const GskStandardContour *) contour;

  gsk_standard_contour_init (dest, self->flags, self->ops, self->n_ops, self->points, self->n_points);
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
      switch (self->ops[start_measure->op].op)
      {
        case GSK_PATH_CLOSE:
        case GSK_PATH_LINE:
          {
            graphene_point_t *pts = &self->points[self->ops[start_measure->op].point];
            graphene_point_t point;

            graphene_point_interpolate (&pts[0], &pts[1], start_progress, &point);
            gsk_path_builder_move_to (builder, point.x, point.y);
            if (end_measure && end_measure->op == start_measure->op)
              {
                graphene_point_interpolate (&pts[0], &pts[1], end_progress, &point);
                gsk_path_builder_line_to (builder, point.x, point.y);
                return;
              }
            gsk_path_builder_line_to (builder, pts[1].x, pts[1].y);
          }
          break;

        case GSK_PATH_CURVE:
          {
            graphene_point_t *pts = &self->points[self->ops[start_measure->op].point];
            graphene_point_t curve[4], discard[4];

            gsk_spline_split_cubic (pts, discard, curve, start_progress);
            if (end_measure && end_measure->op == start_measure->op)
              {
                graphene_point_t tiny[4];
                gsk_spline_split_cubic (curve, tiny, discard, (end_progress - start_progress) / (1 - start_progress));
                gsk_path_builder_move_to (builder, tiny[0].x, tiny[0].y);
                gsk_path_builder_curve_to (builder, tiny[1].x, tiny[1].y, tiny[2].x, tiny[2].y, tiny[3].x, tiny[3].y);
                return;
              }
            gsk_path_builder_move_to (builder, curve[0].x, curve[0].y);
            gsk_path_builder_curve_to (builder, curve[1].x, curve[1].y, curve[2].x, curve[2].y, curve[3].x, curve[3].y);
          }
          break;

        case GSK_PATH_MOVE:
        default:
          g_assert_not_reached();
          return;
      }
      i = start_measure->op + 1;
    }
  else
    i = 0;

  for (; i < (end_measure ? end_measure->op : self->n_ops); i++)
    {
      graphene_point_t *pt = &self->points[self->ops[i].point];

      switch (self->ops[i].op)
      {
        case GSK_PATH_MOVE:
          gsk_path_builder_move_to (builder, pt[0].x, pt[0].y);
          break;

        case GSK_PATH_LINE:
        case GSK_PATH_CLOSE:
          gsk_path_builder_line_to (builder, pt[1].x, pt[1].y);
          break;

        case GSK_PATH_CURVE:
          gsk_path_builder_curve_to (builder, pt[1].x, pt[1].y, pt[2].x, pt[2].y, pt[3].x, pt[3].y);
          break;

        default:
          g_assert_not_reached();
          return;
      }
    }

  /* Add the last partial operation */
  if (end_measure)
    {
      switch (self->ops[end_measure->op].op)
      {
        case GSK_PATH_CLOSE:
        case GSK_PATH_LINE:
          {
            graphene_point_t *pts = &self->points[self->ops[end_measure->op].point];
            graphene_point_t point;

            graphene_point_interpolate (&pts[0], &pts[1], end_progress, &point);
            gsk_path_builder_line_to (builder, point.x, point.y);
          }
          break;

        case GSK_PATH_CURVE:
          {
            graphene_point_t *pts = &self->points[self->ops[end_measure->op].point];
            graphene_point_t curve[4], discard[4];

            gsk_spline_split_cubic (pts, curve, discard, end_progress);
            gsk_path_builder_curve_to (builder, curve[1].x, curve[1].y, curve[2].x, curve[2].y, curve[3].x, curve[3].y);
          }
          break;

        case GSK_PATH_MOVE:
        default:
          g_assert_not_reached();
          return;
      }
    }
}

static const GskContourClass GSK_STANDARD_CONTOUR_CLASS =
{
  sizeof (GskStandardContour),
  "GskStandardContour",
  gsk_standard_contour_get_size,
  gsk_standard_contour_get_flags,
  gsk_standard_contour_print,
  gsk_standard_contour_get_bounds,
  gsk_standard_contour_foreach,
  gsk_standard_contour_init_measure,
  gsk_standard_contour_free_measure,
  gsk_standard_contour_copy,
  gsk_standard_contour_add_segment
};

/* You must ensure the contour has enough size allocated,
 * see gsk_standard_contour_compute_size()
 */
static void
gsk_standard_contour_init (GskContour *contour,
                           GskPathFlags flags,
                           const GskStandardOperation *ops,
                           gsize n_ops,
                           const graphene_point_t *points,
                           gsize n_points)
{
  GskStandardContour *self = (GskStandardContour *) contour;

  self->contour.klass = &GSK_STANDARD_CONTOUR_CLASS;

  self->flags = flags;
  self->n_ops = n_ops;
  memcpy (self->ops, ops, sizeof (GskStandardOperation) * n_ops);
  self->n_points = n_points;
  self->points = (graphene_point_t *) &self->ops[n_ops];
  memcpy (self->points, points, sizeof (graphene_point_t) * n_points);
}

/* CONTOUR */

static gsize
gsk_contour_get_size (const GskContour *contour)
{
  return contour->klass->get_size (contour);
}

static void
gsk_contour_copy (GskContour       *dest,
                  const GskContour *src)
{
  src->klass->copy (src, dest);
}

static GskContour *
gsk_contour_dup (const GskContour *src)
{
  GskContour *copy;

  copy = g_malloc0 (gsk_contour_get_size (src));
  gsk_contour_copy (copy, src);

  return copy;
}

static gboolean
gsk_contour_foreach (const GskContour   *contour,
                     float               tolerance,
                     GskPathForeachFunc  func,
                     gpointer            user_data)
{
  return contour->klass->foreach (contour, tolerance, func, user_data);
}

gpointer
gsk_contour_init_measure (GskPath *path,
                          gsize    i,
                          float    tolerance,
                          float   *out_length)
{
  GskContour *self = path->contours[i];

  return self->klass->init_measure (self, tolerance, out_length);
}

void
gsk_contour_free_measure (GskPath  *path,
                          gsize     i,
                          gpointer  data)
{
  GskContour *self = path->contours[i];

  self->klass->free_measure (self, data);
}

/* PATH */

static GskPath *
gsk_path_alloc (gsize extra_size)
{
  GskPath *self;

  self = g_malloc0 (sizeof (GskPath) + extra_size);
  self->ref_count = 1;
  self->n_contours = 0;

  return self;
}

/**
 * gsk_path_new_from_cairo:
 * @path: a Cairo path
 *
 * This is a convenience function that constructs a `GskPath` from a Cairo path.
 *
 * You can use cairo_copy_path() to access the path from a Cairo context.
 *
 * Returns: a new `GskPath`
 **/
GskPath *
gsk_path_new_from_cairo (const cairo_path_t *path)
{
  GskPathBuilder *builder;
  gsize i;

  g_return_val_if_fail (path != NULL, NULL);

  builder = gsk_path_builder_new ();

  for (i = 0; i < path->num_data; i += path->data[i].header.length)
    {
      const cairo_path_data_t *data = &path->data[i];

      switch (data->header.type)
      {
        case CAIRO_PATH_MOVE_TO:
          gsk_path_builder_move_to (builder, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_LINE_TO:
          gsk_path_builder_line_to (builder, data[1].point.x, data[1].point.y);
          break;

        case CAIRO_PATH_CURVE_TO:
          gsk_path_builder_curve_to (builder,
                                     data[1].point.x, data[1].point.y,
                                     data[2].point.x, data[2].point.y,
                                     data[3].point.x, data[3].point.y);
          break;

        case CAIRO_PATH_CLOSE_PATH:
          gsk_path_builder_close (builder);
          break;

        default:
          g_assert_not_reached ();
          break;
      }
    }

  return gsk_path_builder_free_to_path (builder);
}

/**
 * gsk_path_ref:
 * @self: a `GskPath`
 *
 * Increases the reference count of a `GskPath` by one.
 *
 * Returns: the passed in `GskPath`.
 **/
GskPath *
gsk_path_ref (GskPath *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gsk_path_unref:
 * @self: a `GskPath`
 *
 * Decreases the reference count of a `GskPath` by one.
 *
 * If the resulting reference count is zero, frees the path.
 **/
void
gsk_path_unref (GskPath *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  g_free (self);
}

/**
 * gsk_path_print:
 * @self: a `GskPath`
 * @string:  The string to print into
 *
 * Converts @self into a human-readable string representation suitable
 * for printing.
 *
 * The string is compatible with
 * [SVG path syntax](https://www.w3.org/TR/SVG11/paths.html#PathData),
 * with the exception that conic curves will generate a string of the
 * form "O x1 y1, x2 y2, w" where x1, y1 is the control point, x2, y2
 * is the end point, and w is the weight.
 **/
void
gsk_path_print (GskPath *self,
                GString *string)
{
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (string != NULL);

  for (i = 0; i < self->n_contours; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      self->contours[i]->klass->print (self->contours[i], string);
    }
}

/**
 * gsk_path_to_string:
 * @self: a `GskPath`
 *
 * Converts the path into a string that is suitable for printing.
 *
 * You can use this function in a debugger to get a quick overview
 * of the path.
 *
 * This is a wrapper around [method@Gsk.Path.print], see that function
 * for details.
 *
 * Returns: A new string for @self
 **/
char *
gsk_path_to_string (GskPath *self)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new ("");

  gsk_path_print (self, string);

  return g_string_free (string, FALSE);
}

static gboolean
gsk_path_to_cairo_add_op (GskPathOperation        op,
                          const graphene_point_t *pts,
                          gsize                   n_pts,
                          gpointer                cr)
{
  switch (op)
  {
    case GSK_PATH_MOVE:
      cairo_move_to (cr, pts[0].x, pts[0].y);
      break;

    case GSK_PATH_CLOSE:
      cairo_close_path (cr);
      break;

    case GSK_PATH_LINE:
      cairo_line_to (cr, pts[1].x, pts[1].y);
      break;

    case GSK_PATH_CURVE:
      cairo_curve_to (cr, pts[1].x, pts[1].y, pts[2].x, pts[2].y, pts[3].x, pts[3].y);
      break;

    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

/**
 * gsk_path_to_cairo:
 * @self: a `GskPath`
 * @cr: a cairo context
 *
 * Appends the given @path to the given cairo context for drawing
 * with Cairo.
 *
 * This may cause some suboptimal conversions to be performed as Cairo
 * may not support all features of `GskPath`.
 *
 * This function does not clear the existing Cairo path. Call
 * cairo_new_path() if you want this.
 **/
void
gsk_path_to_cairo (GskPath *self,
                   cairo_t *cr)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (cr != NULL);

  gsk_path_foreach_with_tolerance (self,
                                   cairo_get_tolerance (cr),
                                   gsk_path_to_cairo_add_op,
                                   cr);
}

/*
 * gsk_path_get_n_contours:
 * @path: a #GskPath
 *
 * Gets the number of contours @path is composed out of.
 *
 * Returns: the number of contours in @path
 **/
gsize
gsk_path_get_n_contours (GskPath *path)
{
  return path->n_contours;
}

/**
 * gsk_path_is_empty:
 * @self: a `GskPath`
 *
 * Checks if the path is empty, i.e. contains no lines or curves.
 *
 * Returns: %TRUE if the path is empty
 **/
gboolean
gsk_path_is_empty (GskPath *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->n_contours == 0;
}

/**
 * gsk_path_get_bounds:
 * @self: a `GskPath`
 * @bounds: (out caller-allocates): the bounds of the given path
 *
 * Computes the bounds of the given path.
 *
 * The returned bounds may be larger than necessary, because this
 * function aims to be fast, not accurate. The bounds are guaranteed
 * to contain the path.
 *
 * It is possible that the returned rectangle has 0 width and/or height.
 * This can happen when the path only describes a point or an
 * axis-aligned line.
 *
 * If the path is empty, %FALSE is returned and @bounds are set to
 * graphene_rect_zero(). This is different from the case where the path
 * is a single point at the origin, where the @bounds will also be set to
 * the zero rectangle but 0 will be returned.
 *
 * Returns: %TRUE if the path has bounds, %FALSE if the path is known
 *   to be empty and have no bounds.
 **/
gboolean
gsk_path_get_bounds (GskPath         *self,
                     graphene_rect_t *bounds)
{
  gsize i;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (bounds != NULL, FALSE);

  for (i = 0; i < self->n_contours; i++)
    {
      if (self->contours[i]->klass->get_bounds (self->contours[i], bounds))
        break;
    }

  if (i >= self->n_contours)
    {
      graphene_rect_init_from_rect (bounds, graphene_rect_zero ());
      return FALSE;
    }

  for (i++; i < self->n_contours; i++)
    {
      graphene_rect_t tmp;

      if (self->contours[i]->klass->get_bounds (self->contours[i], &tmp))
        graphene_rect_union (bounds, &tmp, bounds);
    }

  return TRUE;
}

/**
 * gsk_path_foreach:
 * @self: a `GskPath`
 * @func: (scope call) (closure user_data): the function to call for operations
 * @user_data: (nullable): user data passed to @func
 *
 * Calls @func for every operation of the path.
 *
 * Note that this only approximates @self, because paths can contain optimizations
 * for various specialized contours.
 *
 * Returns: %FALSE if @func returned %FALSE, %TRUE otherwise.
 **/
gboolean
gsk_path_foreach (GskPath            *self,
                  GskPathForeachFunc  func,
                  gpointer            user_data)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (func, FALSE);

  return gsk_path_foreach_with_tolerance (self, GSK_PATH_TOLERANCE_DEFAULT, func, user_data);
}

gboolean
gsk_path_foreach_with_tolerance (GskPath            *self,
                                 double              tolerance,
                                 GskPathForeachFunc  func,
                                 gpointer            user_data)
{
  gsize i;

  for (i = 0; i < self->n_contours; i++)
    {
      if (!gsk_contour_foreach (self->contours[i], tolerance, func, user_data))
        return FALSE;
    }

  return TRUE;
}

/* BUILDER */

/**
 * GskPathBuilder:
 *
 * A `GskPathBuilder` struct is an opaque struct. It is meant to
 * not be kept around and only be used to create new `GskPath`
 * objects.
 */

struct _GskPathBuilder
{
  int ref_count;

  GSList *contours; /* (reverse) list of already recorded contours */

  GskPathFlags flags; /* flags for the current path */
  GArray *ops; /* operations for current contour - size == 0 means no current contour */
  GArray *points; /* points for the operations */
};

G_DEFINE_BOXED_TYPE (GskPathBuilder,
                     gsk_path_builder,
                     gsk_path_builder_ref,
                     gsk_path_builder_unref)


void
gsk_path_builder_add_contour (GskPathBuilder *builder,
                              GskPath        *path,
                              gsize           i)
{
  GskContour *copy;

  copy = gsk_contour_dup (path->contours[i]);
  builder->contours = g_slist_prepend (builder->contours, copy);
}

void
gsk_path_builder_add_contour_segment (GskPathBuilder *builder,
                                      GskPath        *path,
                                      gsize           i,
                                      gpointer        measure_data,
                                      float           start,
                                      float           end)
{
  const GskContour *self = path->contours[i];

  self->klass->add_segment (self, builder, measure_data, start, end);
}

/**
 * gsk_path_builder_new:
 *
 * Create a new `GskPathBuilder` object. The resulting builder
 * would create an empty `GskPath`. Use addition functions to add
 * types to it.
 *
 * Returns: a new `GskPathBuilder`
 **/
GskPathBuilder *
gsk_path_builder_new (void)
{
  GskPathBuilder *builder;

  builder = g_slice_new0 (GskPathBuilder);
  builder->ref_count = 1;

  builder->ops = g_array_new (FALSE, FALSE, sizeof (GskStandardOperation));
  builder->points = g_array_new (FALSE, FALSE, sizeof (graphene_point_t));
  return builder;
}

/**
 * gsk_path_builder_ref:
 * @builder: a `GskPathBuilder`
 *
 * Acquires a reference on the given @builder.
 *
 * This function is intended primarily for bindings. `GskPathBuilder` objects
 * should not be kept around.
 *
 * Returns: (transfer none): the given `GskPathBuilder` with
 *   its reference count increased
 */
GskPathBuilder *
gsk_path_builder_ref (GskPathBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (builder->ref_count > 0, NULL);

  builder->ref_count += 1;

  return builder;
}

static void
gsk_path_builder_append_current (GskPathBuilder         *builder,
                                 GskPathOperation        op,
                                 gsize                   n_points,
                                 const graphene_point_t *points)
{
  g_assert (builder->ops->len > 0);
  g_assert (builder->points->len > 0);
  g_assert (n_points > 0);

  g_array_append_vals (builder->ops, &(GskStandardOperation) { op, builder->points->len - 1 }, 1);
  g_array_append_vals (builder->points, points, n_points);
}

static void
gsk_path_builder_end_current (GskPathBuilder *builder)
{
  GskContour *contour;

  if (builder->ops->len == 0)
   return;

  contour = g_malloc0 (gsk_standard_contour_compute_size (builder->ops->len, builder->points->len));
  gsk_standard_contour_init (contour,
                             0,
                             (GskStandardOperation *) builder->ops->data,
                             builder->ops->len,
                             (graphene_point_t *) builder->points->data,
                             builder->points->len);
  builder->contours = g_slist_prepend (builder->contours, contour);

  g_array_set_size (builder->ops, 0);
  g_array_set_size (builder->points, 0);
}

static void
gsk_path_builder_clear (GskPathBuilder *builder)
{
  gsk_path_builder_end_current (builder);

  g_slist_free_full (builder->contours, g_free);
  builder->contours = NULL;
}

/**
 * gsk_path_builder_unref:
 * @builder: a `GskPathBuilder`
 *
 * Releases a reference on the given @builder.
 */
void
gsk_path_builder_unref (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (builder->ref_count > 0);

  builder->ref_count -= 1;

  if (builder->ref_count > 0)
    return;

  gsk_path_builder_clear (builder);
  g_array_unref (builder->ops);
  g_array_unref (builder->points);
  g_slice_free (GskPathBuilder, builder);
}

/**
 * gsk_path_builder_free_to_path: (skip)
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the current state of the
 * given @builder, and frees the @builder instance.
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_free_to_path (GskPathBuilder *builder)
{
  GskPath *res;

  g_return_val_if_fail (builder != NULL, NULL);

  res = gsk_path_builder_to_path (builder);

  gsk_path_builder_unref (builder);

  return res;
}

/**
 * gsk_path_builder_to_path:
 * @builder: a `GskPathBuilder`
 *
 * Creates a new `GskPath` from the given @builder.
 *
 * The given `GskPathBuilder` is reset once this function returns;
 * you cannot call this function multiple times on the same @builder instance.
 *
 * This function is intended primarily for bindings. C code should use
 * gsk_path_builder_free_to_path().
 *
 * Returns: (transfer full): the newly created `GskPath`
 *   with all the contours added to @builder
 */
GskPath *
gsk_path_builder_to_path (GskPathBuilder *builder)
{
  GskPath *path;
  GSList *l;
  gsize size;
  gsize n_contours;
  guint8 *contour_data;
  GskPathFlags flags;

  g_return_val_if_fail (builder != NULL, NULL);

  gsk_path_builder_end_current (builder);

  builder->contours = g_slist_reverse (builder->contours);
  flags = GSK_PATH_CLOSED | GSK_PATH_FLAT;
  size = 0;
  n_contours = 0;
  for (l = builder->contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      n_contours++;
      size += sizeof (GskContour *);
      size += gsk_contour_get_size (contour);
      flags &= contour->klass->get_flags (contour);
    }

  path = gsk_path_alloc (size);
  path->flags = flags;
  path->n_contours = n_contours;
  contour_data = (guint8 *) &path->contours[n_contours];
  n_contours = 0;

  for (l = builder->contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      path->contours[n_contours] = (GskContour *) contour_data;
      gsk_contour_copy ((GskContour *) contour_data, contour);
      size = gsk_contour_get_size (contour);
      contour_data += size;
      n_contours++;
    }

  gsk_path_builder_clear (builder);

  return path;
}

/**
 * gsk_path_builder_add_path:
 * @builder: a #GskPathBuilder
 * @path: (transfer none): the path to append
 *
 * Appends all of @path to @builder.
 **/
void
gsk_path_builder_add_path (GskPathBuilder *builder,
                           GskPath        *path)
{
  gsize i;

  g_return_if_fail (builder != NULL);
  g_return_if_fail (path != NULL);

  for (i = 0; i < path->n_contours; i++)
    {
      gsk_path_builder_add_contour (builder, path, i);
    }
}

static GskContour *
gsk_path_builder_add_contour_by_klass (GskPathBuilder        *builder,
                                       const GskContourClass *klass)
{
  GskContour *contour;

  gsk_path_builder_end_current (builder);

  contour = g_malloc0 (klass->struct_size);
  builder->contours = g_slist_prepend (builder->contours, contour);

  return contour;
}

/**
 * gsk_path_builder_add_rect:
 * @builder: A `GskPathBuilder`
 * @rect: The rectangle to create a path for
 *
 * Creates a path representing the given rectangle.
 *
 * If the width or height of the rectangle is negative, the start
 * point will be on the right or bottom, respectively.
 *
 * If the the width or height are 0, the path will be a closed
 * horizontal or vertical line. If both are 0, it'll be a closed dot.
 *
 * Returns: a new `GskPath` representing a rectangle
 **/
void
gsk_path_builder_add_rect (GskPathBuilder        *builder,
                           const graphene_rect_t *rect)
{
  GskContour *contour;

  g_return_if_fail (builder != NULL);

  contour = gsk_path_builder_add_contour_by_klass (builder, &GSK_RECT_CONTOUR_CLASS);
  gsk_rect_contour_init (contour,
                         rect->origin.x, rect->origin.y,
                         rect->size.width, rect->size.height);
}

void
gsk_path_builder_move_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  g_return_if_fail (builder != NULL);

  gsk_path_builder_end_current (builder);

  builder->flags = GSK_PATH_FLAT;
  g_array_append_vals (builder->ops, &(GskStandardOperation) { GSK_PATH_MOVE, 0 }, 1);
  g_array_append_val (builder->points, GRAPHENE_POINT_INIT(x, y));
}

void
gsk_path_builder_line_to (GskPathBuilder *builder,
                          float           x,
                          float           y)
{
  g_return_if_fail (builder != NULL);

  if (builder->ops->len == 0)
    {
      gsk_path_builder_move_to (builder, x, y);
      return;
    }

  /* skip the line if it goes to the same point */
  if (graphene_point_equal (&g_array_index (builder->points, graphene_point_t, builder->points->len - 1),
                            &GRAPHENE_POINT_INIT (x, y)))
    return;

  gsk_path_builder_append_current (builder,
                                   GSK_PATH_LINE,
                                   1, (graphene_point_t[1]) {
                                     GRAPHENE_POINT_INIT (x, y)
                                   });
}

void
gsk_path_builder_curve_to (GskPathBuilder *builder,
                           float           x1,
                           float           y1,
                           float           x2,
                           float           y2,
                           float           x3,
                           float           y3)
{
  g_return_if_fail (builder != NULL);

  if (builder->ops->len == 0)
    gsk_path_builder_move_to (builder, x1, y1);

  builder->flags &= ~GSK_PATH_FLAT;
  gsk_path_builder_append_current (builder,
                                   GSK_PATH_CURVE,
                                   3, (graphene_point_t[3]) {
                                     GRAPHENE_POINT_INIT (x1, y1),
                                     GRAPHENE_POINT_INIT (x2, y2),
                                     GRAPHENE_POINT_INIT (x3, y3)
                                   });
}

void
gsk_path_builder_close (GskPathBuilder *builder)
{
  g_return_if_fail (builder != NULL);

  if (builder->ops->len == 0)
    return;

  builder->flags |= GSK_PATH_CLOSED;
  gsk_path_builder_append_current (builder,
                                   GSK_PATH_CLOSE,
                                   1, (graphene_point_t[1]) {
                                     g_array_index (builder->points, graphene_point_t, 0)
                                   });

  gsk_path_builder_end_current (builder);
}

