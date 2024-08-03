#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"

G_BEGIN_DECLS

void                    gsk_gpu_node_processor_process                  (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *target,
                                                                         GdkColorState                  *target_color_state,
                                                                         cairo_region_t                 *clip,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_rect_t          *viewport,
                                                                         GskRenderPassType               pass_type);

G_END_DECLS
