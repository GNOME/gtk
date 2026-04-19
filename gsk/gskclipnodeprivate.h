#pragma once

#include "gskclipnode.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_clip_node_new2                      (GskRenderNode          *child,
                                                                 const graphene_rect_t  *clip,
                                                                 GskRectSnap             snap);

G_END_DECLS
