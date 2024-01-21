#pragma once

#include "gsktypes.h"
#include <graphene.h>
#include <math.h>

struct _GskScale
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
gsk_scale_get_x (const GskScale scale)
{
  return graphene_simd4f_get_x (scale.v);
}

static inline float
gsk_scale_get_y (const GskScale scale)
{
  return graphene_simd4f_get_y (scale.v);
}

static inline GskScale
gsk_scale_init (float x,
                float y)
{
  GskScale scale;

  scale.v = graphene_simd4f_init (x, y, 0.f, 0.f);

  return scale;
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
  graphene_simd4f_dup_2f (scale->v, values);
}

static inline gboolean
gsk_scale_is_one (const GskScale scale)
{
  const graphene_vec2_t *one = graphene_vec2_one ();

  return (gboolean) graphene_simd4f_cmp_eq (scale.v, one->__graphene_private_value);
}

static inline gboolean
gsk_scale_is_uniform (const GskScale scale)
{
  return gsk_scale_get_x (scale) == gsk_scale_get_y (scale);
}

static inline GskScale
gsk_scale_invert (const GskScale scale)
{
  GskScale inv;

  inv.v = graphene_simd4f_reciprocal (scale.v);

  return inv;
}

static inline GskScale
gsk_scale_multiply (const GskScale scale1,
                    const GskScale scale2)
{
  GskScale scale;

  scale.v = graphene_simd4f_mul (scale1.v, scale2.v);

  return scale;
}

static inline GskScale
gsk_scale_divide (const GskScale scale1,
                  const GskScale scale2)
{
  GskScale scale;

  scale.v = graphene_simd4f_div (scale1.v, scale2.v);

  return scale;
}

#  define graphene_simd4f_shuffle_yxzw(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_shuffle_ps ((v), (v), _MM_SHUFFLE (1, 0, 2, 3)); \
  }))


static inline GskScale
gsk_scale_max (const GskScale scale)
{
  GskScale m;

  m.v = graphene_simd4f_max (graphene_simd4f_shuffle_yxzw (scale.v), scale.v);

  return m;
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
  return (graphene_point_t) { point.x * gsk_scale_get_x (scale), point.y * gsk_scale_get_y (scale) };
}

static inline graphene_point_t
gsk_point_divide (const graphene_point_t point,
                  const GskScale         scale)
{
  return (graphene_point_t) { point.x / gsk_scale_get_x (scale), point.y / gsk_scale_get_y (scale) };
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

GskScale gsk_scale_extract_from_transform (GskTransform *transform);
