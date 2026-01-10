#include "gskconicgradientnode.h"
#include "gskgradientprivate.h"

GskRenderNode * gsk_conic_gradient_node_new2 (const graphene_rect_t   *bounds,
                                              const graphene_point_t  *center,
                                              float                    rotation,
                                              const GskGradient       *gradient);

