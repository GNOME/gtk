#include "config.h"

#include "gskrectrendererprivate.h"

#include "gskrectprivate.h"
#include "gskroundedrect.h"
#include "gsktransformprivate.h"

void
gsk_rect_renderer_finish (GskRectRenderer *self)
{
  /* nothing to do here */
}

gboolean
gsk_rect_renderer_is_empty (const GskRectRenderer *self)
{
  return self->rect.size.width <= 0.0f || self->rect.size.height <= 0.0f;
}

gboolean
gsk_rect_renderer_contains_rect (const GskRectRenderer *self,
                                 const graphene_rect_t *rect)
{
  return gsk_rect_contains_rect (&self->rect, rect);
}

const graphene_rect_t *
gsk_rect_renderer_get_rect (const GskRectRenderer *self)
{
  if (gsk_rect_renderer_is_empty (self))
    return NULL;

  return &self->rect;
}

void
gsk_rect_renderer_add (GskRectRenderer       *self,
                       const GskRectRenderer *other)
{
  if (gsk_rect_renderer_is_empty (other))
    return;

  gsk_rect_renderer_add_rect (self, &other->rect);
}

void
gsk_rect_renderer_intersect (GskRectRenderer       *self,
                             const GskRectRenderer *other)
{
  if (gsk_rect_renderer_is_empty (other))
    return;

  gsk_rect_renderer_intersect_rect (self, &other->rect);
}

void
gsk_rect_renderer_add_rect (GskRectRenderer       *self,
                            const graphene_rect_t *rect)
{
  if (gsk_rect_renderer_is_empty (self))
    self->rect = *rect;
  else
    gsk_rect_coverage (&self->rect, rect, &self->rect);
}

void
gsk_rect_renderer_subtract_rect (GskRectRenderer       *self,
                                 const graphene_rect_t *rect)
{
  if (gsk_rect_renderer_is_empty (self))
    return;

  if (!gsk_rect_subtract (&self->rect, rect, &self->rect))
    self->rect = GRAPHENE_RECT_INIT (0, 0, 0, 0);
}

void
gsk_rect_renderer_intersect_rect (GskRectRenderer       *self,
                                  const graphene_rect_t *rect)
{
  if (gsk_rect_renderer_is_empty (self))
    return;

  if (!gsk_rect_intersection (&self->rect, rect, &self->rect))
    self->rect = GRAPHENE_RECT_INIT (0, 0, 0, 0);
}

void
gsk_rect_renderer_intersect_rounded_rect (GskRectRenderer      *self,
                                          const GskRoundedRect *rect)
{
  graphene_rect_t wide_opaque, high_opaque;
  double start, end;

  if (gsk_rect_renderer_is_empty (self))
    return;

  wide_opaque = rect->bounds;
  start = MAX(rect->corner[GSK_CORNER_TOP_LEFT].height, rect->corner[GSK_CORNER_TOP_RIGHT].height);
  end = MAX(rect->corner[GSK_CORNER_BOTTOM_LEFT].height, rect->corner[GSK_CORNER_BOTTOM_RIGHT].height);
  wide_opaque.size.height -= MIN (wide_opaque.size.height, start + end);
  wide_opaque.origin.y += start;
  graphene_rect_intersection (&wide_opaque, &self->rect, &wide_opaque);

  high_opaque = rect->bounds;
  start = MAX(rect->corner[GSK_CORNER_TOP_LEFT].width, rect->corner[GSK_CORNER_BOTTOM_LEFT].width);
  end = MAX(rect->corner[GSK_CORNER_TOP_RIGHT].width, rect->corner[GSK_CORNER_BOTTOM_RIGHT].width);
  high_opaque.size.width -= MIN (high_opaque.size.width, start + end);
  high_opaque.origin.x += start;
  graphene_rect_intersection (&high_opaque, &self->rect, &high_opaque);

  if (wide_opaque.size.width * wide_opaque.size.height > high_opaque.size.width * high_opaque.size.height)
    self->rect = wide_opaque;
  else
    self->rect = high_opaque;
}

void
gsk_rect_renderer_shrink (GskRectRenderer       *self,
                          const graphene_size_t *size)
{
  if (gsk_rect_renderer_is_empty (self))
    return;

  graphene_rect_inset (&self->rect, size->width, size->height);
}

void
gsk_rect_renderer_transform (GskRectRenderer *self,
                             GskTransform    *transform)
{
  /* otherwise the transform won't be exact and then this API isn't good enough */
  g_assert (gsk_transform_get_fine_category (transform) >= GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL);

  gsk_transform_transform_bounds (transform,
                                  &self->rect,
                                  &self->rect);
}
