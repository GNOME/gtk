#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_convert_op                              (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         float                           opacity,
                                                                         gboolean                        straight_alpha,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image);


G_END_DECLS

