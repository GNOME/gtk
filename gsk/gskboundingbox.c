#include "config.h"

#include "gskboundingboxprivate.h"

GskBoundingBox *
gsk_bounding_box_init (GskBoundingBox         *self,
                       const graphene_point_t *a,
                       const graphene_point_t *b)
{
  self->min.x = MIN (a->x, b->x);
  self->min.y = MIN (a->y, b->y);
  self->max.x = MAX (a->x, b->x);
  self->max.y = MAX (a->y, b->y);
  return self;
}

GskBoundingBox *
gsk_bounding_box_init_copy (GskBoundingBox       *self,
                            const GskBoundingBox *src)
{
  self->min = src->min;
  self->max = src->max;
  return self;
}

GskBoundingBox *
gsk_bounding_box_init_from_rect (GskBoundingBox        *self,
                                 const graphene_rect_t *bounds)
{
  self->min = bounds->origin;
  self->max.x = bounds->origin.x + bounds->size.width;
  self->max.y = bounds->origin.y + bounds->size.height;
  return self;
}

void
gsk_bounding_box_expand (GskBoundingBox         *self,
                         const graphene_point_t *p)
{
  self->min.x = MIN (self->min.x, p->x);
  self->min.y = MIN (self->min.y, p->y);
  self->max.x = MAX (self->max.x, p->x);
  self->max.y = MAX (self->max.y, p->y);
}

graphene_rect_t *
gsk_bounding_box_to_rect (const GskBoundingBox  *self,
                          graphene_rect_t       *rect)
{
  rect->origin = self->min;
  rect->size.width = self->max.x - self->min.x;
  rect->size.height = self->max.y - self->min.y;
  return rect;
}

gboolean
gsk_bounding_box_contains_point (const GskBoundingBox     *self,
                                 const graphene_point_t *p)
{
  return self->min.x <= p->x && p->x <= self->max.x &&
         self->min.y <= p->y && p->y <= self->max.y;
}

gboolean
gsk_bounding_box_intersection (const GskBoundingBox *a,
                               const GskBoundingBox *b,
                               GskBoundingBox       *res)
{
  graphene_point_t min, max;

  min.x = MAX (a->min.x, b->min.x);
  min.y = MAX (a->min.y, b->min.y);
  max.x = MIN (a->max.x, b->max.x);
  max.y = MIN (a->max.y, b->max.y);

  if (res)
    gsk_bounding_box_init (res, &min, &max);

  return min.x <= max.x && min.y <= max.y;
}
