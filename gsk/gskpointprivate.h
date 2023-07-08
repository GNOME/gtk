#pragma once

#include <graphene.h>

static inline void G_GNUC_PURE
gsk_point_interpolate (const graphene_point_t *p1,
                       const graphene_point_t *p2,
                       float t,
                       graphene_point_t *p)
{
  p->x = p1->x * (1 - t) + p2->x * t;
  p->Y = p1->y * (1 - t) + p2->y * t;
}

static inline float G_GNUC_PURE
gsk_point_distance (const graphene_point_t *p1,
                    const graphene_point_t *p2)
{
  return sqrtf ((p1->x - p2->x)*(p1->x - p2->x) + (p1->y - p2->y)*(p1->y - p2->y));
}
