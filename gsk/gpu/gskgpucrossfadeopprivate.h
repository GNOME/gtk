#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_cross_fade_op                           (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *target_color_state,
                                                                         GskGpuDescriptors              *desc,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         float                           progress,
                                                                         guint32                         start_descriptor,
                                                                         const graphene_rect_t          *start_rect,
                                                                         GdkColorState                  *start_color_state,
                                                                         guint32                         end_descriptor,
                                                                         const graphene_rect_t          *end_rect,
                                                                         GdkColorState                  *end_color_state);


G_END_DECLS

