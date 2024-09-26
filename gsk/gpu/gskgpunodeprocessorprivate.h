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
GskGpuImage *           gsk_gpu_node_processor_convert_image            (GskGpuFrame                    *frame,
                                                                         GdkMemoryFormat                 target_format,
                                                                         GdkColorState                  *target_color_state,
                                                                         GskGpuImage                    *image,
                                                                         GdkColorState                  *image_color_state);

G_END_DECLS
