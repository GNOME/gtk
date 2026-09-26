#pragma once

#include "gskcontainernode.h"

#include "gskrendernodeprivate.h"

G_BEGIN_DECLS

GskRenderNode *         gsk_container_node_new_with_color_state (GdkColorState          *ccs,
                                                                 GskRenderNode         **children,
                                                                 guint                   n_children);

void                    gsk_container_node_diff_with            (GskRenderNode          *container,
                                                                 GskRenderNode          *other,
                                                                 GskDiffData            *data);

gboolean                gsk_container_node_is_disjoint          (const GskRenderNode    *node) G_GNUC_PURE;

G_END_DECLS
