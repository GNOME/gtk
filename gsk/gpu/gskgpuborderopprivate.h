#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"
#include "gdkcolorprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_border_op                               (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskRoundedRect           *outline,
                                                                         const graphene_point_t         *inside_offset,
                                                                         const float                     widths[4],
                                                                         const GdkColor                  colors[4]);


G_END_DECLS

