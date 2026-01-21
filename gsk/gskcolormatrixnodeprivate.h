#pragma once

#include "gskcolormatrixnode.h"
#include "gskrendernodeprivate.h"

G_BEGIN_DECLS

GskRenderNode * gsk_color_matrix_node_new2 (GskRenderNode           *child,
                                            GdkColorState           *color_state,
                                            const graphene_matrix_t *color_matrix,
                                            const graphene_vec4_t   *color_offset);

GdkColorState * gsk_color_matrix_node_get_color_state (const GskRenderNode    *node) G_GNUC_PURE;

void apply_color_matrix_to_pattern (cairo_pattern_t         *pattern,
                                    const graphene_matrix_t *color_matrix,
                                    const graphene_vec4_t   *color_offset,
                                    GdkColorState           *color_state,
                                    GskCairoData            *data);

G_END_DECLS
