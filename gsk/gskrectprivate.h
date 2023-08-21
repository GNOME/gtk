#pragma once

#include <graphene.h>

static inline void
gsk_rect_init (graphene_rect_t *r,
               float            x,
               float            y,
               float            width,
               float            height)
{
  r->origin.x = x;
  r->origin.y = y;
  r->size.width = width;
  r->size.height = height;
}

static inline void
gsk_rect_init_from_rect (graphene_rect_t       *r,
                         const graphene_rect_t *r1)
{
  gsk_rect_init (r, r1->origin.x, r1->origin.y, r1->size.width, r1->size.height);
}

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
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    return FALSE;
  else
    return TRUE;
}

static inline gboolean
gsk_rect_intersection (const graphene_rect_t *r1,
                       const graphene_rect_t *r2,
                       graphene_rect_t       *res)
{
  float x1, y1, x2, y2;

  /* Assume both rects are already normalized, as they usually are */
  x1 = MAX (r1->origin.x, r2->origin.x);
  y1 = MAX (r1->origin.y, r2->origin.y);
  x2 = MIN (r1->origin.x + r1->size.width, r2->origin.x + r2->size.width);
  y2 = MIN (r1->origin.y + r1->size.height, r2->origin.y + r2->size.height);

  if (x1 >= x2 || y1 >= y2)
    {
      gsk_rect_init (res, 0.f, 0.f, 0.f, 0.f);
      return FALSE;
    }
  else
    {
      gsk_rect_init (res, x1, y1, x2 - x1, y2 - y1);
      return TRUE;
    }
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

static inline void
gsk_gpu_rect_to_float (const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       float                   values[4])
{
  values[0] = rect->origin.x + offset->x;
  values[1] = rect->origin.y + offset->y;
  values[2] = rect->size.width;
  values[3] = rect->size.height;
}

