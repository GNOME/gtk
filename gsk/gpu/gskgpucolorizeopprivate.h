#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_colorize_op                             (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image,
                                                                         const float                     color[4]);


G_END_DECLS

