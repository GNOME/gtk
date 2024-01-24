#pragma once

#include "gsktypesprivate.h"
#include <graphene.h>
#include <math.h>

#ifndef USE_SIMD

struct _Scale
{
  float x, y;
};

static inline float
scale_x (const Scale s)
{
  return s.x;
}

static inline float
scale_y (const Scale s)
{
  return s.y;
}

static inline Scale
scale (float x,
       float y)
{
  return (Scale) { .x = x, .y = y };
}

static inline Scale
scale_from_float (float s)
{
  return scale (s, s);
}

static inline Scale
scale_from_graphene (const graphene_vec2_t *v)
{
  return (Scale) { .x = graphene_vec2_get_x (v), .y = graphene_vec2_get_y (v) };
}

static inline void
scale_to_float (const Scale s,
                float       v[2])
{
  v[0] = s.x;
  v[1] = s.y;
}

static inline gboolean
scale_equal (const Scale s1,
             const Scale s2)
{
  return (gboolean) (s1.x == s2.x && s1.y == s2.y);
}

static inline Scale
scale_one (void)
{
  return scale (1, 1);
}

static inline Scale
scale_inv (const Scale s)
{
  return (Scale) { .x = 1 / s.x, .y = 1 / s.y };
}

static inline Scale
scale_mul (const Scale s1,
           const Scale s2)
{
  return (Scale) { .x = s1.x * s2.x, .y = s1.y * s2.y };
}

static inline Scale
scale_div (const Scale s1,
           const Scale s2)
{
  return (Scale) { .x = s1.x / s2.x, .y = s1.y / s2.y };
}

static inline Scale
scale_max (const Scale s)
{
  return (Scale) { .x = MAX (s.x, s.y), .y = MAX (s.x, s.y) };
}

#else  /* USE_SIMD */

struct _Scale
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
scale_x (const Scale s)
{
  return graphene_simd4f_get_x (s.v);
}

static inline float
scale_y (const Scale s)
{
  return graphene_simd4f_get_y (s.v);
}

static inline Scale
scale (float x,
       float y)
{
  return (Scale) { .v = graphene_simd4f_init (x, y, 0.f, 0.f) };
}

static inline Scale
scale_from_float (float s)
{
  return scale (s, s);
}

static inline Scale
scale_from_graphene (const graphene_vec2_t *v)
{
  return (Scale) { .v = v->__graphene_private_value };
}

static inline void
scale_to_float (const Scale s,
                float       v[2])
{
  graphene_simd4f_dup_2f (s.v, v);
}

static inline gboolean
scale_equal (const Scale s1,
             const Scale s2)
{
  return (gboolean) graphene_simd4f_cmp_eq (s1.v, s2.v);
}

static inline Scale
scale_one (void)
{
  return scale (1, 1);
}

static inline Scale
scale_inv (const Scale s)
{
  return (Scale) { .v = graphene_simd4f_reciprocal (s.v) };
}

static inline Scale
scale_mul (const Scale s1,
           const Scale s2)
{
  return (Scale) { .v = graphene_simd4f_mul (s1.v, s2.v) };
}

static inline Scale
scale_div (const Scale s1,
           const Scale s2)
{
  return (Scale) { .v = graphene_simd4f_div (s1.v, s2.v) };
}

#ifndef graphene_simd4f_shuffle_yxzw
#  define graphene_simd4f_shuffle_yxzw(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_shuffle_ps ((v), (v), _MM_SHUFFLE (3, 2, 0, 1)); \
  }))
#endif

static inline Scale
scale_max (const Scale s)
{
  return (Scale) { .v = graphene_simd4f_max (graphene_simd4f_shuffle_yxzw (s.v), s.v) };
}

#endif
