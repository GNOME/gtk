#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"

G_BEGIN_DECLS

void                    gsk_gpu_node_processor_process                  (GskGpuFrame                    *frame,
                                                                         GskGpuImage                    *target,
                                                                         GdkColorState                  *target_color_state,
                                                                         const cairo_rectangle_int_t    *clip,
                                                                         GskRenderNode                  *node,
                                                                         const graphene_rect_t          *viewport);

G_END_DECLS
