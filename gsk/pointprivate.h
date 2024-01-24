#pragma once

#include "gsktypesprivate.h"
#include <graphene.h>
#include <math.h>
#include <smmintrin.h>

#include "scaleprivate.h"

#ifndef USE_SIMD

struct _Point
{
  float x, y;
};

static inline float
point_x (const Point p)
{
  return p.x;
}

static inline float
point_y (const Point p)
{
  return p.y;
}

static inline Point
point (float x,
       float y)
{
  return (Point) { .x = x, .y = y };
}

static inline Point
point_from_graphene (const graphene_point_t *p)
{
  return point (p->x, p->y);
}

static inline void
point_to_float (const Point p,
                float       v[2])
{
  v[0] = p.x;
  v[1] = p.y;
}

static inline Point
point_zero (void)
{
  return point (0, 0);
}

static inline Point
point_neg (const Point p)
{
  return (Point) { .x = -p.x, .y = -p.y };
}

static inline Point
point_mul (const Point p,
           const Scale s)
{
  return (Point) { .x = p.x * s.x, .y = p.y * s.y };
}

static inline Point
point_div (const Point p,
           const Scale s)
{
  return (Point) { .x = p.x / s.x, .y = p.y / s.y };
}

static inline Point
point_add (const Point p1,
           const Point p2)
{
  return (Point) { .x = p1.x + p2.x, .y = p1.y + p2.y };
}

static inline Point
point_sub (const Point p1,
           const Point p2)
{
  return (Point) { .x = p1.x - p2.x, .y = p1.y - p2.y };
}

static inline Point
point_floor (const Point p)
{
  return (Point) { .x = floorf (p.x), .y = floorf (p.y) };
}

static inline Point
point_ceil (const Point p)
{
  return (Point) { .x = ceilf (p.x), .y = ceilf (p.y) };
}

static inline Point
point_round (const Point p)
{
  return (Point) { .x = roundf (p.x), .y = roundf (p.y) };
}

#else  /* USE_SIMD */

#include <smmintrin.h>

struct _Point
{
  GRAPHENE_ALIGNED_DECL (graphene_simd4f_t v, 16);
};

static inline float
point_x (const Point p)
{
  return graphene_simd4f_get_x (p.v);
}

static inline float
point_y (const Point p)
{
  return graphene_simd4f_get_y (p.v);
}

static inline Point
point (float x,
       float y)
{
  return (Point) { .v = graphene_simd4f_init (x, y, 0.f, 0.f) };
}

static inline Point
point_from_graphene (const graphene_point_t *p)
{
  return point (p->x, p->y);
}

static inline void
point_to_float (const Point p,
                float       v[2])
{
  graphene_simd4f_dup_2f (p.v, v);
}

static inline Point
point_zero (void)
{
  return point (0, 0);
}

static inline Point
point_neg (const Point p)
{
  return (Point) { .v = graphene_simd4f_neg (p.v) };
}

static inline Point
point_mul (const Point p,
           const Scale s)
{
  return (Point) { .v = graphene_simd4f_mul (p.v, s.v) };
}

static inline Point
point_div (const Point p,
           const Scale s)
{
  return (Point) { .v = graphene_simd4f_div (p.v, s.v) };
}

static inline Point
point_add (const Point p1,
           const Point p2)
{
  return (Point) { .v = graphene_simd4f_add (p1.v, p2.v) };
}

static inline Point
point_sub (const Point p1,
           const Point p2)
{
  return (Point) { .v = graphene_simd4f_sub (p1.v, p2.v) };
}

#ifdef __SSE4_1__

#ifndef graphene_simd4f_floor
#  define graphene_simd4f_floor(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_floor_ps ((v)); \
  }))
#endif

#ifndef graphene_simd4f_ceil
#  define graphene_simd4f_ceil(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_ceil_ps ((v)); \
  }))
#endif

#ifndef graphene_simd4f_round
#  define graphene_simd4f_round(v) \
  (__extension__ ({ \
    (graphene_simd4f_t) _mm_round_ps ((v)); \
  }))
#endif

static inline Point
point_floor (const Point p)
{
  return (Point) { .v = graphene_simd4f_floor (p.v) };
}

static inline Point
point_ceil (const Point p)
{
  return (Point) { .v = graphene_simd4f_ceil (p.v) };
}

static inline Point
point_round (const Point p)
{
  return (Point) { .v = graphene_simd4f_round (p.v) };
}

#else

static inline Point
point_floor (const Point p)
{
  return point (floorf (point_x (p)), floorf (point_y (p)));
}

static inline Point
point_ceil (const Point p)
{
  return point (ceilf (point_x (p)), ceilf (point_y (p)));
}

static inline Point
point_round (const Point p)
{
  return point (roundf (point_x (p)), roundf (point_y (p)));
}

#endif

#endif
