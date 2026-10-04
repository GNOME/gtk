#pragma once

#include "gskblurnode.h"

G_BEGIN_DECLS

void                    gsk_blur_node_get_padding               (const graphene_size_t  *sigma,
                                                                 graphene_size_t        *out_padding);

G_END_DECLS
