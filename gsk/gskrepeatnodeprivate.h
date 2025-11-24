#pragma once

#include "gskrepeatnode.h"

/* for GskRepeat */
#include "gskgradientprivate.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_repeat_node_new2                    (const graphene_rect_t  *bounds,
                                                                 GskRenderNode          *child,
                                                                 const graphene_rect_t  *child_bounds,
                                                                 GskRepeat               repeat);

GskRepeat               gsk_repeat_node_get_repeat              (GskRenderNode          *node);

G_END_DECLS
