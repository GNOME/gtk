#pragma once

#include "gskcomponenttransfernode.h"

G_BEGIN_DECLS

GskRenderNode * gsk_component_transfer_node_new2            (const graphene_rect_t      *clip,
                                                             GskRectSnap                 snap,
                                                             GskRenderNode              *child,
                                                             GdkColorState              *color_state,
                                                             const GskComponentTransfer *r,
                                                             const GskComponentTransfer *g,
                                                             const GskComponentTransfer *b,
                                                             const GskComponentTransfer *a);

GdkColorState * gsk_component_transfer_node_get_color_state (const GskRenderNode *node) G_GNUC_PURE;
const graphene_rect_t *
                gsk_component_transfer_node_get_clip        (const GskRenderNode *node) G_GNUC_PURE;

G_END_DECLS
