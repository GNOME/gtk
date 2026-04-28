#pragma once

#include "gskpastenode.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_paste_node_new2                     (const graphene_rect_t  *bounds,
                                                                 GskRectSnap             snap,
                                                                 gsize                   depth);

G_END_DECLS
