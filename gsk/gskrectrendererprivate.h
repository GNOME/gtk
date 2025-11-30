#pragma once

#include "gsktypes.h"

G_BEGIN_DECLS

typedef struct _GskRectRenderer GskRectRenderer;

struct _GskRectRenderer
{
  graphene_rect_t rect;
};

#define GSK_RECT_RENDERER_INIT_EMPTY() (GskRectRenderer) { GRAPHENE_RECT_INIT(0, 0, 0, 0) }
#define GSK_RECT_RENDERER_INIT_COPY(other) (GskRectRenderer) *(other)

void                    gsk_rect_renderer_finish                        (GskRectRenderer               *self);

gboolean                gsk_rect_renderer_is_empty                      (const GskRectRenderer         *self);
gboolean                gsk_rect_renderer_contains_rect                 (const GskRectRenderer         *self,
                                                                         const graphene_rect_t         *rect);
const graphene_rect_t * gsk_rect_renderer_get_rect                      (const GskRectRenderer         *self);

void                    gsk_rect_renderer_add                           (GskRectRenderer               *self,
                                                                         const GskRectRenderer         *other);
void                    gsk_rect_renderer_intersect                     (GskRectRenderer               *self,
                                                                         const GskRectRenderer         *other);

void                    gsk_rect_renderer_add_rect                      (GskRectRenderer               *self,
                                                                         const graphene_rect_t         *rect);
void                    gsk_rect_renderer_subtract_rect                 (GskRectRenderer               *self,
                                                                         const graphene_rect_t         *rect);
void                    gsk_rect_renderer_intersect_rect                (GskRectRenderer               *self,
                                                                         const graphene_rect_t         *rect);

void                    gsk_rect_renderer_intersect_rounded_rect        (GskRectRenderer               *self,
                                                                         const GskRoundedRect          *rect);

void                    gsk_rect_renderer_shrink                        (GskRectRenderer               *self,
                                                                         const graphene_size_t         *size);
void                    gsk_rect_renderer_transform                     (GskRectRenderer               *self,
                                                                         GskTransform                  *transform);


G_END_DECLS
