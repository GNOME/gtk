#pragma once

#include "gskblendnode.h"
#include "gskrendernodeprivate.h"

G_BEGIN_DECLS

GskRenderNode * gsk_blend_node_new2 (GskRenderNode *bottom,
                                     GskRenderNode *top,
                                     GdkColorState *color_state,
                                     GskBlendMode   blend_mode);

GdkColorState * gsk_blend_node_get_color_state (const GskRenderNode    *node) G_GNUC_PURE;

G_END_DECLS
