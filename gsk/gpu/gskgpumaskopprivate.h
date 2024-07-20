#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_mask_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         GskMaskMode                     mask_mode,
                                                                         const GskGpuShaderImage        *source,
                                                                         const GskGpuShaderImage        *mask);


G_END_DECLS

