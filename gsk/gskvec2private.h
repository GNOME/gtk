#pragma once

#include <graphene.h>
#include <math.h>

static inline float
gsk_vec2_get_x (const graphene_vec2_t *v)
{
  return graphene_simd4f_get_x (v->__graphene_private_value);
}

static inline float
gsk_vec2_get_y (const graphene_vec2_t *v)
{
  return graphene_simd4f_get_y (v->__graphene_private_value);
}

static inline graphene_vec2_t *
gsk_vec2_init (graphene_vec2_t *v,
               float            x,
               float            y)
{
  v->__graphene_private_value = graphene_simd4f_init (x, y, 0.f, 0.f);

  return v;
}

static inline void
gsk_vec2_multiply (const graphene_vec2_t *a,
                   const graphene_vec2_t *b,
                   graphene_vec2_t       *res)
{
  res->__graphene_private_value = graphene_simd4f_mul (a->__graphene_private_value,
                                                       b->__graphene_private_value);
}

static inline bool
gsk_vec2_equal (const graphene_vec2_t *v1,
                const graphene_vec2_t *v2)
{
  return graphene_simd4f_cmp_eq (v1->__graphene_private_value, v2->__graphene_private_value);
}

