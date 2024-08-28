#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_blur_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image,
                                                                         const graphene_vec2_t          *blur_direction);

void                    gsk_gpu_blur_shadow_op                          (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image,
                                                                         const graphene_vec2_t          *blur_direction,
                                                                         const GdkColor                 *shadow_color);


G_END_DECLS

