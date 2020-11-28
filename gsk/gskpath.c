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

#include "gskpathbuilder.h"
#include "gsksplineprivate.h"


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
  void                  (* get_start_end)       (const GskContour       *self,
                                                 graphene_point_t       *start,
                                                 graphene_point_t       *end);
  gboolean              (* foreach)             (const GskContour       *contour,
                                                 float                   tolerance,
                                                 GskPathForeachFunc      func,
                                                 gpointer                user_data);
  gpointer              (* init_measure)        (const GskContour       *contour,
                                                 float                   tolerance,
                                                 float                  *out_length);
  void                  (* free_measure)        (const GskContour       *contour,
                                                 gpointer                measure_data);
  void                  (* get_point)           (const GskContour       *contour,
                                                 gpointer                measure_data,
                                                 float                   distance,
                                                 graphene_point_t       *pos,
                                                 graphene_vec2_t        *tangent);
  gboolean              (* get_closest_point)   (const GskContour       *contour,
                                                 gpointer                measure_data,
                                                 float                   tolerance,
                                                 const graphene_point_t *point,
                                                 float                   threshold,
                                                 float                  *out_offset,
                                                 graphene_point_t       *out_pos,
                                                 float                  *out_distance,
                                                 graphene_vec2_t        *out_tangent);
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

static void
gsk_find_point_on_line (const graphene_point_t *a,
                        const graphene_point_t *b,
                        const graphene_point_t *p,
                        float                  *offset,
                        graphene_point_t       *pos)
{
  graphene_vec2_t n;
  graphene_vec2_t ap;
  float t;

  graphene_vec2_init (&n, b->x - a->x, b->y - a->y);
  graphene_vec2_init (&ap, p->x - a->x, p->y - a->y);

  t = graphene_vec2_dot (&ap, &n) / graphene_vec2_dot (&n, &n);

  if (t <= 0)
    {
      *pos = *a;
      *offset = 0;
    }
  else if (t >= 1)
    {
      *pos = *b;
      *offset = 1;
    }
  else
    {
      graphene_point_interpolate (a, b, t, pos);
      *offset = t;
    }
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
gsk_rect_contour_get_point (const GskContour *contour,
                            gpointer          measure_data,
                            float             distance,
                            graphene_point_t *pos,
                            graphene_vec2_t  *tangent)
{
  const GskRectContour *self = (const GskRectContour *) contour;

  if (distance < fabsf (self->width))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + copysignf (distance, self->width), self->y);
      if (tangent)
        graphene_vec2_init (tangent, copysignf (1.0f, self->width), 0.0f);
      return;
    }
  distance -= fabsf (self->width);

  if (distance < fabsf (self->height))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width, self->y + copysignf (distance, self->height));
      if (tangent)
        graphene_vec2_init (tangent, 0.0f, copysignf (self->height, 1.0f));
      return;
    }
  distance -= fabs (self->height);

  if (distance < fabsf (self->width))
    {
      if (pos)
        *pos = GRAPHENE_POINT_INIT (self->x + self->width - copysignf (distance, self->width), self->y + self->height);
      if (tangent)
        graphene_vec2_init (tangent, - copysignf (1.0f, self->width), 0.0f);
      return;
    }
  distance -= fabsf (self->width);

  if (pos)
    *pos = GRAPHENE_POINT_INIT (self->x, self->y + self->height - copysignf (distance, self->height));
  if (tangent)
    graphene_vec2_init (tangent, 0.0f, - copysignf (self->height, 1.0f));
}

static gboolean
gsk_rect_contour_get_closest_point (const GskContour       *contour,
                                    gpointer                measure_data,
                                    float                   tolerance,
                                    const graphene_point_t *point,
                                    float                   threshold,
                                    float                  *out_distance,
                                    graphene_point_t       *out_pos,
                                    float                  *out_offset,
                                    graphene_vec2_t        *out_tangent)
{
  const GskRectContour *self = (const GskRectContour *) contour;
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

  p = GRAPHENE_POINT_INIT (self->x + t.x * self->width,
                           self->y + t.y * self->height);

  distance = graphene_point_distance (point, &p, NULL, NULL);
  if (distance > threshold)
    return FALSE;

  if (out_distance)
    *out_distance = distance;

  if (out_pos)
    *out_pos = p;

  if (out_offset)
    *out_offset = (t.x == 0.0 && self->width > 0 ? 2 - t.y : t.y) * ABS (self->height) +
                  (t.y == 1.0 ? 2 - t.x : t.x) * ABS (self->width);

  if (out_tangent)
    {
      if (t.y == 0 && t.x < 1)
        graphene_vec2_init (out_tangent, copysignf(1.0, self->width), 0);
      else if (t.x == 0)
        graphene_vec2_init (out_tangent, 0, - copysignf(1.0, self->height));
      else if (t.y == 1)
        graphene_vec2_init (out_tangent, - copysignf(1.0, self->width), 0);
      else if (t.x == 1)
        graphene_vec2_init (out_tangent, 0, copysignf(1.0, self->height));
    }

  return TRUE;
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
  gsk_rect_contour_get_start_end,
  gsk_rect_contour_foreach,
  gsk_rect_contour_init_measure,
  gsk_rect_contour_free_measure,
  gsk_rect_contour_get_point,
  gsk_rect_contour_get_closest_point,
  gsk_rect_contour_copy,
  gsk_rect_contour_add_segment
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

/* CIRCLE CONTOUR */

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

  g_string_append (string, "M ");
  _g_string_append_point (string, &GSK_CIRCLE_POINT_INIT (self, self->start_angle));
  g_string_append (string, " A ");
  _g_string_append_point (string, &GRAPHENE_POINT_INIT (self->radius, self->radius));
  g_string_append_printf (string, " 0 %u %u ",
                          fabs (self->start_angle - self->end_angle) > 180 ? 1 : 0,
                          self->start_angle < self->end_angle ? 0 : 1);
  _g_string_append_point (string, &GSK_CIRCLE_POINT_INIT (self, self->end_angle));
  if (fabs (self->start_angle - self->end_angle >= 360))
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

  return wrapper->func (GSK_PATH_CURVE, curve, 4, wrapper->user_data);
}

static gboolean
gsk_circle_contour_foreach (const GskContour   *contour,
                            float               tolerance,
                            GskPathForeachFunc  func,
                            gpointer            user_data)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  graphene_point_t start = GSK_CIRCLE_POINT_INIT (self, self->start_angle);

  if (!func (GSK_PATH_MOVE, &start, 1, user_data))
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
      if (!func (GSK_PATH_CLOSE, (graphene_point_t[2]) { start, start }, 2, user_data))
        return FALSE;
    }

  return TRUE;
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

static void
gsk_circle_contour_get_point (const GskContour *contour,
                              gpointer          measure_data,
                              float             distance,
                              graphene_point_t *pos,
                              graphene_vec2_t  *tangent)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float delta = self->end_angle - self->start_angle;
  float length = self->radius * DEG_TO_RAD (delta);
  float angle = self->start_angle + distance/length * delta;
  graphene_point_t p;

  p = GSK_CIRCLE_POINT_INIT (self, angle);

  if (pos)
    *pos = p;

  if (tangent)
    {
      graphene_vec2_init (tangent,
                          p.y - self->center.y,
                          - p.x + self->center.x);
      graphene_vec2_normalize (tangent, tangent);
    }
}

static gboolean
gsk_circle_contour_get_closest_point (const GskContour       *contour,
                                      gpointer                measure_data,
                                      float                   tolerance,
                                      const graphene_point_t *point,
                                      float                   threshold,
                                      float                  *out_distance,
                                      graphene_point_t       *out_pos,
                                      float                  *out_offset,
                                      graphene_vec2_t        *out_tangent)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float angle;
  float closest_angle;
  float offset;
  graphene_point_t pos;
  graphene_vec2_t tangent;
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

  offset = self->radius * 2 * M_PI * (closest_angle - self->start_angle) / (self->end_angle - self->start_angle);

  gsk_circle_contour_get_point (contour, NULL, offset, &pos, out_tangent ? &tangent : NULL);

  distance = graphene_point_distance (&pos, point, NULL, NULL);
  if (threshold < distance)
    return FALSE;

  if (out_offset)
    *out_offset = offset;
  if (out_pos)
    *out_pos = pos;
  if (out_distance)
    *out_distance = distance;
  if (out_tangent)
    *out_tangent = tangent;

  return TRUE;
}

static void
gsk_circle_contour_copy (const GskContour *contour,
                         GskContour       *dest)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  GskCircleContour *target = (GskCircleContour *) dest;

  *target = *self;
}

static void
gsk_circle_contour_add_segment (const GskContour *contour,
                                GskPathBuilder   *builder,
                                gpointer          measure_data,
                                float             start,
                                float             end)
{
  const GskCircleContour *self = (const GskCircleContour *) contour;
  float delta = self->end_angle - self->start_angle;
  float length = self->radius * DEG_TO_RAD (delta);
  GskContour *segment;

  segment = gsk_circle_contour_new (&self->center, self->radius,
                                    self->start_angle + start/length * delta,
                                    self->start_angle + end/length * delta);
  gsk_path_builder_add_contour (builder, segment);
}

static const GskContourClass GSK_CIRCLE_CONTOUR_CLASS =
{
  sizeof (GskCircleContour),
  "GskCircleContour",
  gsk_contour_get_size_default,
  gsk_circle_contour_get_flags,
  gsk_circle_contour_print,
  gsk_circle_contour_get_bounds,
  gsk_circle_contour_get_start_end,
  gsk_circle_contour_foreach,
  gsk_circle_contour_init_measure,
  gsk_circle_contour_free_measure,
  gsk_circle_contour_get_point,
  gsk_circle_contour_get_closest_point,
  gsk_circle_contour_copy,
  gsk_circle_contour_add_segment
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

/* STANDARD CONTOUR */

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
gsk_standard_contour_measure_get_point (GskStandardContour        *self,
                                        gsize                      op,
                                        float                      progress,
                                        graphene_point_t          *pos,
                                        graphene_vec2_t           *tangent)
{
  const graphene_point_t *pts;

  pts = &self->points[self->ops[op].point];
  switch (self->ops[op].op)
    {
      case GSK_PATH_LINE:
      case GSK_PATH_CLOSE:
        if (pos)
          graphene_point_interpolate (&pts[0], &pts[1], progress, pos);
        if (tangent)
          {
            graphene_vec2_init (tangent, pts[1].x - pts[0].x, pts[1].y - pts[0].y);
            graphene_vec2_normalize (tangent, tangent);
          }
        break;

      case GSK_PATH_CURVE:
        gsk_spline_get_point_cubic (pts, progress, pos, tangent);
        break;

      case GSK_PATH_MOVE:
      default:
        g_assert_not_reached ();
        return;
    }
}

static void
gsk_standard_contour_get_point (const GskContour *contour,
                                gpointer          measure_data,
                                float             distance,
                                graphene_point_t *pos,
                                graphene_vec2_t  *tangent)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GArray *array = measure_data;
  guint index;
  float progress;
  GskStandardContourMeasure *measure;

  if (array->len == 0)
    {
      g_assert (distance == 0);
      g_assert (self->ops[0].op == GSK_PATH_MOVE);
      if (pos)
        *pos = self->points[0];
      if (tangent)
        graphene_vec2_init (tangent, 1.f, 0.f);
      return;
    }

  if (!g_array_binary_search (array, &distance, gsk_standard_contour_find_measure, &index))
    index = array->len - 1;
  measure = &g_array_index (array, GskStandardContourMeasure, index);
  progress = (distance - measure->start) / (measure->end - measure->start);
  progress = measure->start_progress + (measure->end_progress - measure->start_progress) * progress;
  g_assert (progress >= 0 && progress <= 1);

  gsk_standard_contour_measure_get_point (self, measure->op, progress, pos, tangent);
}

static gboolean
gsk_standard_contour_get_closest_point (const GskContour       *contour,
                                        gpointer                measure_data,
                                        float                   tolerance,
                                        const graphene_point_t *point,
                                        float                   threshold,
                                        float                  *out_distance,
                                        graphene_point_t       *out_pos,
                                        float                  *out_offset,
                                        graphene_vec2_t        *out_tangent)
{
  GskStandardContour *self = (GskStandardContour *) contour;
  GskStandardContourMeasure *measure;
  float progress, dist;
  GArray *array = measure_data;
  graphene_point_t p, last_point;
  gsize i;
  gboolean result = FALSE;

  g_assert (self->ops[0].op == GSK_PATH_MOVE);
  last_point = self->points[0];

  if (array->len == 0)
    {
      /* This is the special case for point-only */
      dist = graphene_point_distance (&last_point, point, NULL, NULL);

      if (dist > threshold)
        return FALSE;

      if (out_offset)
        *out_offset = 0;

      if (out_distance)
        *out_distance = dist;

      if (out_pos)
        *out_pos = last_point;

      if (out_tangent)
        *out_tangent = *graphene_vec2_x_axis ();

      return TRUE;
    }

  for (i = 0; i < array->len; i++)
    {
      measure = &g_array_index (array, GskStandardContourMeasure, i);

      gsk_find_point_on_line (&last_point,
                              &measure->end_point,
                              point,
                              &progress,
                              &p);
      last_point = measure->end_point;
      dist = graphene_point_distance (point, &p, NULL, NULL);
      /* add some wiggleroom for the accurate check below */
      //g_print ("%zu: (%g-%g) dist %g\n", i, measure->start, measure->end, dist);
      if (dist <= threshold + 1.0f)
        {
          graphene_vec2_t t;
          float found_progress;

          found_progress = measure->start_progress + (measure->end_progress - measure->start_progress) * progress;

          gsk_standard_contour_measure_get_point (self, measure->op, found_progress, &p, out_tangent ? &t : NULL);
          dist = graphene_point_distance (point, &p, NULL, NULL);
          /* double check that the point actually is closer */
          //g_print ("!!! %zu: (%g-%g) dist %g\n", i, measure->start, measure->end, dist);
          if (dist <= threshold)
            {
              if (out_distance)
                *out_distance = dist;
              if (out_pos)
                *out_pos = p;
              if (out_offset)
                *out_offset = measure->start + (measure->end - measure->start) * progress;
              if (out_tangent)
                *out_tangent = t;
              result = TRUE;
              if (tolerance >= dist)
                  return TRUE;
              threshold = dist - tolerance;
            }
        }
    }

  return result;
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
  gsk_standard_contour_get_start_end,
  gsk_standard_contour_foreach,
  gsk_standard_contour_init_measure,
  gsk_standard_contour_free_measure,
  gsk_standard_contour_get_point,
  gsk_standard_contour_get_closest_point,
  gsk_standard_contour_copy,
  gsk_standard_contour_add_segment
};

/* You must ensure the contour has enough size allocated,
 * see gsk_standard_contour_compute_size()
 */
static void
gsk_standard_contour_init (GskContour   *contour,
                           GskPathFlags  flags,
                           const         GskStandardOperation *ops,
                           gsize         n_ops,
                           const         graphene_point_t *points,
                           gsize         n_points)
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

GskContour *
gsk_standard_contour_new (GskPathFlags flags,
                          const        GskStandardOperation *ops,
                          gsize        n_ops,
                          const        graphene_point_t *points,
                          gsize        n_points)
{
  GskContour *contour;

  contour = g_malloc0 (gsk_standard_contour_compute_size (n_ops, n_points));

  gsk_standard_contour_init (contour, flags, ops, n_ops, points, n_points);

  return contour;
}

/* CONTOUR */

static gsize
gsk_contour_get_size (const GskContour *contour)
{
  return contour->klass->get_size (contour);
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

void
gsk_contour_get_start_end (const GskContour *self,
                           graphene_point_t *start,
                           graphene_point_t *end)
{
  self->klass->get_start_end (self, start, end);
}

void
gsk_contour_get_point (GskPath          *path,
                       gsize             i,
                       gpointer          measure_data,
                       float             distance,
                       graphene_point_t *pos,
                       graphene_vec2_t  *tangent)
{
  GskContour *self = path->contours[i];

  self->klass->get_point (self, measure_data, distance, pos, tangent);
}

gboolean
gsk_contour_get_closest_point (GskPath                *path,
                               gsize                   i,
                               gpointer                measure_data,
                               float                   tolerance,
                               const graphene_point_t *point,
                               float                   threshold,
                               float                  *out_distance,
                               graphene_point_t       *out_pos,
                               float                  *out_offset,
                               graphene_vec2_t        *out_tangent)
{
  GskContour *self = path->contours[i];

  return self->klass->get_closest_point (self,
                                         measure_data,
                                         tolerance,
                                         point,
                                         threshold,
                                         out_distance,
                                         out_pos,
                                         out_offset,
                                         out_tangent);
}

void
gsk_contour_add_segment (const GskContour *self,
                         GskPathBuilder   *builder,
                         gpointer          measure_data,
                         float             start,
                         float             end)
{
  self->klass->add_segment (self, builder, measure_data, start, end);
}

static inline void
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

/* PATH */

GskPath *
gsk_path_new_from_contours (const GSList *contours)
{
  GskPath *path;
  const GSList *l;
  gsize size;
  gsize n_contours;
  guint8 *contour_data;
  GskPathFlags flags;

  flags = GSK_PATH_CLOSED | GSK_PATH_FLAT;
  size = 0;
  n_contours = 0;
  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      n_contours++;
      size += sizeof (GskContour *);
      size += gsk_contour_get_size (contour);
      flags &= contour->klass->get_flags (contour);
    }

  path = g_malloc0 (sizeof (GskPath) + size);
  path->ref_count = 1;
  path->flags = flags;
  path->n_contours = n_contours;
  contour_data = (guint8 *) &path->contours[n_contours];
  n_contours = 0;

  for (l = contours; l; l = l->next)
    {
      GskContour *contour = l->data;

      path->contours[n_contours] = (GskContour *) contour_data;
      gsk_contour_copy ((GskContour *) contour_data, contour);
      size = gsk_contour_get_size (contour);
      contour_data += size;
      n_contours++;
    }

  return path;
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

const GskContour *
gsk_path_get_contour (GskPath *path,
                      gsize    i)
{
  return path->contours[i];
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
