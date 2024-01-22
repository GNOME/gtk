#pragma once

#include "gsktypes.h"
#include <graphene.h>
#include <math.h>
#include <smmintrin.h>

#include "gskscaleprivate.h"

struct _GskPoint
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
gsk_point_get_x (const GskPoint p)
{
  return graphene_simd4f_get_x (p.v);
}

static inline float
gsk_point_get_y (const GskPoint p)
{
  return graphene_simd4f_get_y (p.v);
}

static inline GskPoint
gsk_point_init (float x,
                float y)
{
  GskPoint p;

  p.v = graphene_simd4f_init (x, y, 0.f, 0.f);

  return p;
}

static inline GskPoint
gsk_point_init_from_graphene (const graphene_point_t *p)
{
  return gsk_point_init (p->x, p->y);
}

static inline void
gsk_point_to_float (const GskPoint *p,
                    float           v[2])
{
  graphene_simd4f_dup_2f (p->v, v);
}

static inline gboolean
gsk_point_is_zero (const GskPoint p)
{
  return gsk_point_get_x (p) == 0 && gsk_point_get_y (p) == 0;
}

static inline GskPoint
gsk_point_negate (const GskPoint p)
{
  GskPoint neg;

  neg.v = graphene_simd4f_neg (p.v);

  return neg;
}

static inline GskPoint
gsk_point_multiply (const GskPoint p,
                    const GskScale s)
{
  GskPoint q;

  q.v = graphene_simd4f_mul (p.v, s.v);

  return q;
}

static inline GskPoint
gsk_point_divide (const GskPoint p,
                  const GskScale s)
{
  GskPoint q;

  q.v = graphene_simd4f_div (p.v, s.v);

  return q;
}

static inline GskPoint
gsk_point_add (const GskPoint p1,
               const GskPoint p2)
{
  GskPoint p;

  p.v = graphene_simd4f_add (p1.v, p2.v);

  return p;
}

static inline GskPoint
gsk_point_subtract (const GskPoint p1,
                    const GskPoint p2)
{
  GskPoint p;

  p.v = graphene_simd4f_sub (p1.v, p2.v);

  return p;
}

#ifdef HAVE_SSE4

#  define graphene_simd4f_floor(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_floor_ps ((v)); \
  }))

#  define graphene_simd4f_ceil(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_ceil_ps ((v)); \
  }))

static inline GskPoint
gsk_point_floor (const GskPoint p)
{
  GskPoint q;

  q.v = graphene_simd4f_floor (p.v);

  return q;
}

static inline GskPoint
gsk_point_ceil (const GskPoint p)
{
  GskPoint q;

  q.v = graphene_simd4f_ceil (p.v);

  return q;
}

#else

static inline GskPoint
gsk_point_floor (const GskPoint p)
{
  return gsk_point_init (floorf (gsk_point_get_x (p)), floorf (gsk_point_get_y (p)));
}

static inline GskPoint
gsk_point_ceil (const GskPoint p)
{
  return gsk_point_init (ceilf (gsk_point_get_x (p)), ceilf (gsk_point_get_y (p)));
}

#endif
