#pragma once

#include "gsktexturenode.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_texture_node_new2                   (GdkTexture             *texture,
                                                                 const graphene_rect_t  *bounds,
                                                                 GskRectSnap             snap);

G_END_DECLS
