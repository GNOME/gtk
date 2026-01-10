#pragma once

#include "gskradialgradientnode.h"

#include "gskgradientprivate.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

GskRenderNode * gsk_radial_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start_center,
                                                         float                    start_radius,
                                                         const graphene_point_t  *end_center,
                                                         float                    end_radius,
                                                         float                    aspect_ratio,
                                                         const GskGradient       *gradient);

const graphene_point_t *gsk_radial_gradient_node_get_start_center     (const GskRenderNode *node) G_GNUC_PURE;
const graphene_point_t *gsk_radial_gradient_node_get_end_center       (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_start_radius     (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_end_radius       (const GskRenderNode *node) G_GNUC_PURE;
float                   gsk_radial_gradient_node_get_aspect_ratio     (const GskRenderNode *node) G_GNUC_PURE;
gboolean                gsk_radial_gradient_fills_plane               (const graphene_point_t *c1,
                                                                       float                   r1,
                                                                       const graphene_point_t *c2,
                                                                       float                   r2);

G_END_DECLS
