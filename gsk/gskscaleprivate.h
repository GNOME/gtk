#pragma once

#include "gsktypes.h"
#include <graphene.h>
#include <math.h>

struct _GskScale
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
gsk_scale_get_x (const GskScale s)
{
  return graphene_simd4f_get_x (s.v);
}

static inline float
gsk_scale_get_y (const GskScale s)
{
  return graphene_simd4f_get_y (s.v);
}

static inline GskScale
gsk_scale_init (float x,
                float y)
{
  GskScale s;

  s.v = graphene_simd4f_init (x, y, 0.f, 0.f);

  return s;
}

static inline GskScale
gsk_scale_init_uniform (float x)
{
  return gsk_scale_init (x, x);
}

static inline void
gsk_scale_to_float (const GskScale *s,
                    float           v[2])
{
  graphene_simd4f_dup_2f (s->v, v);
}

static inline gboolean
gsk_scale_is_one (const GskScale s)
{
  return gsk_scale_get_x (s) == 1 && gsk_scale_get_y (s) == 1;
}

static inline gboolean
gsk_scale_is_uniform (const GskScale s)
{
  return gsk_scale_get_x (s) == gsk_scale_get_y (s);
}

static inline GskScale
gsk_scale_invert (const GskScale s)
{
  GskScale inv;

  inv.v = graphene_simd4f_reciprocal (s.v);

  return inv;
}

static inline GskScale
gsk_scale_multiply (const GskScale s1,
                    const GskScale s2)
{
  GskScale s;

  s.v = graphene_simd4f_mul (s1.v, s2.v);

  return s;
}

static inline GskScale
gsk_scale_divide (const GskScale s1,
                  const GskScale s2)
{
  GskScale s;

  s.v = graphene_simd4f_div (s1.v, s2.v);

  return s;
}

#  define graphene_simd4f_shuffle_yxzw(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_shuffle_ps ((v), (v), _MM_SHUFFLE (1, 0, 2, 3)); \
  }))


static inline GskScale
gsk_scale_max (const GskScale s)
{
  GskScale m;

  m.v = graphene_simd4f_max (graphene_simd4f_shuffle_yxzw (s.v), s.v);

  return m;
}

GskScale gsk_scale_extract_from_transform (GskTransform *transform);
