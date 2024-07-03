#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_border_op                               (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         const GskRoundedRect           *outline,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_point_t         *inside_offset,
                                                                         const float                     widths[4],
                                                                         const float                     colors[4][4]);


G_END_DECLS

