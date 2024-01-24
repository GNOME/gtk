#pragma once

#include "gsktypesprivate.h"
#include <graphene.h>
#include <math.h>
#include <smmintrin.h>

#include "scaleprivate.h"

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
