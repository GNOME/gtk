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
#include "gsksplineprivate.h"

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
  void                  (* get_start_end)       (const GskContour       *self,
                                                 graphene_point_t       *start,
                                                 graphene_point_t       *end);
  gboolean              (* foreach)             (const GskContour       *contour,
                                                 float                   tolerance,
                                                 GskPathForeachFunc      func,
                                                 gpointer                user_data);
  GskContour *          (* reverse)             (const GskContour       *contour);
};

static gsize
gsk_contour_get_size_default (const GskContour *contour)
{
  return contour->klass->struct_size;
}

/* {{{ Utilities */

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
                           gssize                  offset);

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

static const GskContourClass GSK_STANDARD_CONTOUR_CLASS =
{
  sizeof (GskStandardContour),
  "GskStandardContour",
  gsk_standard_contour_copy,
  gsk_standard_contour_get_size,
  gsk_standard_contour_get_flags,
  gsk_standard_contour_print,
  gsk_standard_contour_get_bounds,
  gsk_standard_contour_get_start_end,
  gsk_standard_contour_foreach,
  gsk_standard_contour_reverse,
};

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
    self->ops[i] = gsk_pathop_encode (gsk_pathop_op (ops[i]),
                                      gsk_pathop_points (ops[i]) + offset);
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

static const GskContourClass GSK_RECT_CONTOUR_CLASS =
{
  sizeof (GskRectContour),
  "GskRectContour",
  gsk_rect_contour_copy,
  gsk_contour_get_size_default,
  gsk_rect_contour_get_flags,
  gsk_rect_contour_print,
  gsk_rect_contour_get_bounds,
  gsk_rect_contour_get_start_end,
  gsk_rect_contour_foreach,
  gsk_rect_contour_reverse,
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
  pts[6] = GRAPHENE_POINT_INIT (rect->bounds.origin.x + rect->bounds.size.width - rect->corner[GSK_CORNER_TOP_RIGHT].width, rect->bounds.origin.y + rect->bounds.size.height);
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

static const GskContourClass GSK_ROUNDED_RECT_CONTOUR_CLASS =
{
  sizeof (GskRoundedRectContour),
  "GskRoundedRectContour",
  gsk_rounded_rect_contour_copy,
  gsk_contour_get_size_default,
  gsk_rounded_rect_contour_get_flags,
  gsk_rounded_rect_contour_print,
  gsk_rounded_rect_contour_get_bounds,
  gsk_rounded_rect_contour_get_start_end,
  gsk_rounded_rect_contour_foreach,
  gsk_rounded_rect_contour_reverse,
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
/* {{{ Circle */

#define DEG_TO_RAD(x)          ((x) * (G_PI / 180.f))
#define RAD_TO_DEG(x)          ((x) / (G_PI / 180.f))

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

static const GskContourClass GSK_CIRCLE_CONTOUR_CLASS =
{
  sizeof (GskCircleContour),
  "GskCircleContour",
  gsk_circle_contour_copy,
  gsk_contour_get_size_default,
  gsk_circle_contour_get_flags,
  gsk_circle_contour_print,
  gsk_circle_contour_get_bounds,
  gsk_circle_contour_get_start_end,
  gsk_circle_contour_foreach,
  gsk_circle_contour_reverse,
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

/* }}} */

/* vim:set foldmethod=marker expandtab: */
