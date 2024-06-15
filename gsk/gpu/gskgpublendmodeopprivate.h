#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_blend_mode_op                           (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *target_color_state,
                                                                         GskGpuDescriptors              *desc,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         GskBlendMode                    blend_mode,
                                                                         guint32                         bottom_descriptor,
                                                                         const graphene_rect_t          *bottom_rect,
                                                                         GdkColorState                  *bottom_color_state,
                                                                         guint32                         top_descriptor,
                                                                         const graphene_rect_t          *top_rect,
                                                                         GdkColorState                  *top_color_state);


G_END_DECLS

