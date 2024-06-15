#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_mask_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *target_color_state,
                                                                         GskGpuDescriptors              *desc,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         GskMaskMode                     mask_mode,
                                                                         guint32                         source_descriptor,
                                                                         const graphene_rect_t          *source_rect,
                                                                         GdkColorState                  *source_color_state,
                                                                         guint32                         mask_descriptor,
                                                                         const graphene_rect_t          *mask_rect,
                                                                         GdkColorState                  *mask_color_state);


G_END_DECLS

