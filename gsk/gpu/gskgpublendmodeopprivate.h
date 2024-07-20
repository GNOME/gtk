#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_blend_mode_op                           (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         GskBlendMode                    blend_mode,
                                                                         const GskGpuShaderImage        *bottom,
                                                                         const GskGpuShaderImage        *top);


G_END_DECLS

