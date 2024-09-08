#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_colorize_op                             (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image,
                                                                         const GdkColor                 *color);

void                    gsk_gpu_colorize_op2                            (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image,
                                                                         const GdkColor                 *color);


G_END_DECLS

