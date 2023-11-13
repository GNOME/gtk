#pragma once

#include <graphene.h>

static inline gboolean G_GNUC_PURE
gsk_rect_contains_rect (const graphene_rect_t *r1,
                        const graphene_rect_t *r2)
{
  return r2->origin.x >= r1->origin.x &&
         (r2->origin.x + r2->size.width) <= (r1->origin.x + r1->size.width) &&
         r2->origin.y >= r1->origin.y &&
         (r2->origin.y + r2->size.height) <= (r1->origin.y + r1->size.height);
}

static inline gboolean G_GNUC_PURE
gsk_rect_intersects (const graphene_rect_t *r1,
                     const graphene_rect_t *r2)
{
  /* Assume both rects are already normalized, as they usually are */
  if (r1->origin.x >= (r2->origin.x + r2->size.width) ||
      (r1->origin.x + r1->size.width) <= r2->origin.x ||
      r1->origin.y >= (r2->origin.y + r2->size.height) ||
      (r1->origin.y + r1->size.height) <= r2->origin.y)
    return FALSE;
  else
    return TRUE;
}

static inline void
gsk_rect_to_float (const graphene_rect_t *rect,
                   float                  values[4])
{
  values[0] = rect->origin.x;
  values[1] = rect->origin.y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

static inline gboolean
gsk_rect_equal (const graphene_rect_t *r1,
                const graphene_rect_t *r2)
{
  return r1->origin.x == r2->origin.x &&
         r1->origin.y == r2->origin.y &&
         r1->size.width == r2->size.width &&
         r1->size.height == r2->size.height;
}
