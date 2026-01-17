#pragma once

#include "gsklineargradientnode.h"

#include "gskgradientprivate.h"

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

GskRenderNode * gsk_linear_gradient_node_new2           (const graphene_rect_t   *bounds,
                                                         const graphene_point_t  *start,
                                                         const graphene_point_t  *end,
                                                         const GskGradient       *gradient);

gboolean        gsk_linear_gradient_node_is_zero_length (GskRenderNode           *node) G_GNUC_PURE;


G_END_DECLS
