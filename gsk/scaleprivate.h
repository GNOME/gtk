#pragma once

#include "gsktypesprivate.h"
#include <graphene.h>
#include <math.h>

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
scale_to_graphene (const Scale      s,
                   graphene_vec2_t *v)
{
  graphene_vec2_init (v, s.x, s.y);
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
