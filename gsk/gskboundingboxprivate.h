#pragma once

#include <gsk/gsktypes.h>

G_BEGIN_DECLS

typedef struct _GskBoundingBox GskBoundingBox;

struct _GskBoundingBox {
  graphene_point_t min;
  graphene_point_t max;
};

GDK_AVAILABLE_IN_ALL
GskBoundingBox * gsk_bounding_box_init (GskBoundingBox         *self,
                                        const graphene_point_t *a,
                                        const graphene_point_t *b);

GDK_AVAILABLE_IN_ALL
GskBoundingBox * gsk_bounding_box_init_copy (GskBoundingBox       *self,
                                             const GskBoundingBox *src);

GDK_AVAILABLE_IN_ALL
GskBoundingBox * gsk_bounding_box_init_from_rect (GskBoundingBox        *self,
                                                  const graphene_rect_t *bounds);

GDK_AVAILABLE_IN_ALL
void             gsk_bounding_box_expand (GskBoundingBox         *self,
                                          const graphene_point_t *p);

GDK_AVAILABLE_IN_ALL
graphene_rect_t *gsk_bounding_box_to_rect        (const GskBoundingBox  *self,
                                                  graphene_rect_t       *rect);

GDK_AVAILABLE_IN_ALL
gboolean         gsk_bounding_box_contains_point (const GskBoundingBox   *self,
                                                  const graphene_point_t *p);

GDK_AVAILABLE_IN_ALL
gboolean         gsk_bounding_box_intersection (const GskBoundingBox *a,
                                                const GskBoundingBox *b,
                                                GskBoundingBox       *res);

G_END_DECLS
