#pragma once

#include "gsktexturescalenode.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_texture_scale_node_new2             (GdkTexture             *texture,
                                                                 const graphene_rect_t  *bounds,
                                                                 GskRectSnap             snap,
                                                                 GskScalingFilter        filter);

G_END_DECLS

