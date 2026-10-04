#pragma once

#include "gskblurnode.h"

G_BEGIN_DECLS

/* FIXME: Before making this public, check if we need a colorstate arg for the blur colorstate */
GskRenderNode *         gsk_blur_node_new2                      (GskRenderNode          *child,
                                                                 const graphene_size_t  *sigma);

void                    gsk_blur_node_get_padding               (const graphene_size_t  *sigma,
                                                                 graphene_size_t        *out_padding);

G_END_DECLS
