#pragma once

#include <graphene.h>
#include <math.h>
#include "gskscaleprivate.h"
#include "gskpointprivate.h"


static inline void
gsk_rect_init (graphene_rect_t *r,
               float            x,
               float            y,
               float            width,
               float            height)
{
  r->origin.x = x;
  r->origin.y = y;
  r->size.width = width;
  r->size.height = height;
}

static inline void
gsk_rect_init_from_rect (graphene_rect_t       *r,
                         const graphene_rect_t *r1)
{
  gsk_rect_init (r, r1->origin.x, r1->origin.y, r1->size.width, r1->size.height);
}

static inline graphene_rect_t
gsk_rect_init_offset (const graphene_rect_t src,
                      const GskPoint        offset)
{
  return GRAPHENE_RECT_INIT (src.origin.x + gsk_point_get_x (offset),
                             src.origin.y + gsk_point_get_y (offset),
                             src.size.width,
                             src.size.height);
}

static inline gboolean G_GNUC_PURE
gsk_rect_contains_rect (const graphene_rect_t *r1,
                        const graphene_rect_t *r2)
{
  return r2->origin.x >= r1->origin.x &&
         (r2->origin.x + r2->size.width) <= (r1->origin.x + r1->size.width) &&
         r2->origin.y >= r1->origin.y &&
         (r2->origin.y + r2->size.height) <= (r1->origin.y + r1->size.height);
}

static inline gboolean G_GNUC_PURE
gsk_rect_intersects (const graphene_rect_t *r1,
                     const graphene_rect_t *r2)
{
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    return FALSE;
  else
    return TRUE;
}

static inline gboolean
gsk_rect_intersection (const graphene_rect_t *r1,
                       const graphene_rect_t *r2,
                       graphene_rect_t       *res)
{
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    {
      gsk_rect_init (res, 0.f, 0.f, 0.f, 0.f);
      return FALSE;
    }
  else
    {
      gsk_rect_init (res, x1, y1, x2 - x1, y2 - y1);
      return TRUE;
    }
}

static inline gboolean G_GNUC_PURE
gsk_rect_is_empty (const graphene_rect_t *rect)
{
  return rect->size.width == 0 || rect->size.height == 0;
}

static inline void
gsk_rect_to_float (const graphene_rect_t *rect,
                   float                  values[4])
{
  values[0] = rect->origin.x;
  values[1] = rect->origin.y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline gboolean
gsk_rect_equal (const graphene_rect_t *r1,
                const graphene_rect_t *r2)
{
  return r1->origin.x == r2->origin.x &&
         r1->origin.y == r2->origin.y &&
         r1->size.width == r2->size.width &&
         r1->size.height == r2->size.height;
}

static inline void
gsk_gpu_rect_to_float (const graphene_rect_t  *rect,
                       const GskPoint         *offset,
                       float                   values[4])
{
  values[0] = rect->origin.x + gsk_point_get_x (*offset);
  values[1] = rect->origin.y + gsk_point_get_y (*offset);
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline graphene_rect_t
gsk_rect_round_larger (const graphene_rect_t rect)
{
  float x = floorf (rect.origin.x);
  float y = floorf (rect.origin.y);

  return GRAPHENE_RECT_INIT (x, y,
                             ceilf (rect.origin.x + rect.size.width) - x,
                             ceilf (rect.origin.y + rect.size.height) - y);
}

static inline graphene_rect_t
gsk_rect_scale (const graphene_rect_t r,
                const GskScale        scale)
{
  float sx = gsk_scale_get_x (scale);
  float sy = gsk_scale_get_y (scale);

  if (G_UNLIKELY (sx < 0 || sy < 0))
    {
      graphene_rect_t res;
      graphene_rect_scale (&r, sx, sy, &res);
      return res;
    }

  return GRAPHENE_RECT_INIT (r.origin.x * sx,
                             r.origin.y * sy,
                             r.size.width * sx,
                             r.size.height * sy);
}

static inline graphene_rect_t
gsk_rect_add_offset (const graphene_rect_t r,
                     const GskPoint        offset)
{
  return GRAPHENE_RECT_INIT (r.origin.x + gsk_point_get_x (offset),
                             r.origin.y + gsk_point_get_y (offset),
                             r.size.width,
                             r.size.height);
}

static inline graphene_rect_t
gsk_rect_subtract_offset (const graphene_rect_t r,
                          const GskPoint        offset)
{
  return GRAPHENE_RECT_INIT (r.origin.x - gsk_point_get_x (offset),
                             r.origin.y - gsk_point_get_y (offset),
                             r.size.width,
                             r.size.height);
}

static inline graphene_rect_t
gsk_rect_from_points (GskPoint p0,
                      GskPoint p1)
{
  return GRAPHENE_RECT_INIT (gsk_point_get_x (p0),
                             gsk_point_get_y (p0),
                             gsk_point_get_x (p1) - gsk_point_get_x (p0),
                             gsk_point_get_y (p1) - gsk_point_get_y (p0));
}

static inline GskPoint
gsk_rect_get_origin (const graphene_rect_t rect)
{
  return gsk_point_init (rect.origin.x, rect.origin.y);
}

static inline GskPoint
gsk_rect_get_opposite (const graphene_rect_t rect)
{
  return gsk_point_init (rect.origin.x + rect.size.width, rect.origin.y + rect.size.height);
}

static inline graphene_rect_t
gsk_rect_round_to_pixels (const graphene_rect_t src,
                          const GskScale        scale,
                          const GskPoint        offset)
{
  GskPoint p0, p1;

  p0 = gsk_point_subtract (gsk_point_divide (gsk_point_floor (gsk_point_multiply (gsk_point_add (gsk_rect_get_origin (src), offset), scale)), scale), offset);

  p1 = gsk_point_divide (gsk_point_subtract (gsk_point_ceil (gsk_point_multiply (gsk_point_add (gsk_rect_get_opposite (src), offset), scale)), p0), scale);

  return gsk_rect_from_points (p0, p1);
}
