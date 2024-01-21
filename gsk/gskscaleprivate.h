#pragma once

#include "gsktypes.h"
#include <graphene.h>
#include <math.h>

struct _GskScale
{
  float x;
  float y;
};

#define GSK_SCALE_INIT(x,y) (GskScale) { (x), (y) }

static inline GskScale
gsk_scale_init (float    x,
                float    y)
{
  return (GskScale) { x, y };
}

static inline GskScale
gsk_scale_init_uniform (float s)
{
  return gsk_scale_init (s, s);
}

static inline void
gsk_scale_to_float (const GskScale *scale,
                    float           values[2])
{
  values[0] = scale->x;
  values[1] = scale->y;
}

static inline gboolean
gsk_scale_is_one (const GskScale scale)
{
  return scale.x == 1 && scale.y == 1;
}

static inline gboolean
gsk_scale_is_uniform (const GskScale scale)
{
  return scale.x == scale.y;
}

static inline GskScale
gsk_scale_invert (const GskScale scale)
{
  return (GskScale) { 1 / scale.x, 1 / scale.y };
}

static inline GskScale
gsk_scale_multiply (const GskScale scale1,
                    const GskScale scale2)
{
  return (GskScale) { scale1.x * scale2.x, scale1.y * scale2.y };
}

static inline GskScale
gsk_scale_divide (const GskScale scale1,
                  const GskScale scale2)
{
  return (GskScale) { scale1.x / scale2.x, scale1.y / scale2.y };
}

static inline GskScale
gsk_scale_max (const GskScale scale)
{
  float s = MAX (scale.x, scale.y);

  return (GskScale) { s, s };
}

static inline graphene_point_t
gsk_point_init (float x,
                float y)
{
  return (graphene_point_t) { x, y };
}

static inline gboolean
gsk_point_is_zero (const graphene_point_t point)
{
  return point.x == 0 && point.y == 0;
}

static inline graphene_point_t
gsk_point_negate (const graphene_point_t point)
{
  return (graphene_point_t) { - point.x, - point.y };
}

static inline graphene_point_t
gsk_point_multiply (const graphene_point_t point,
                    const GskScale         scale)
{
  return (graphene_point_t) { point.x * scale.x, point.y * scale.y };
}

static inline graphene_point_t
gsk_point_divide (const graphene_point_t point,
                  const GskScale         scale)
{
  return (graphene_point_t) { point.x / scale.x, point.y / scale.y };
}

static inline graphene_point_t
gsk_point_add (const graphene_point_t point1,
               const graphene_point_t point2)
{
  return (graphene_point_t) { point1.x + point2.x, point1.y + point2.y };
}

static inline graphene_point_t
gsk_point_subtract (const graphene_point_t point1,
                    const graphene_point_t point2)
{
  return (graphene_point_t) { point1.x - point2.x, point1.y - point2.y };
}

static inline graphene_point_t
gsk_point_floor (const graphene_point_t point)
{
  return (graphene_point_t) { floorf (point.x), floorf (point.y) };
}

static inline graphene_point_t
gsk_point_ceil (const graphene_point_t point)
{
  return (graphene_point_t) { ceilf (point.x), ceilf (point.y) };
}
void
gsk_scale_extract_from_transform (GskTransform *transform,
                                  GskScale     *scale);
