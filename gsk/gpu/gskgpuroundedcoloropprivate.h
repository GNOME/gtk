#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"
#include "gdkcolorprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_rounded_color_op                               (GskGpuFrame                    *frame,
                                                                                GskGpuShaderClip                clip,
                                                                                GdkColorState                  *ccs,
                                                                                float                           opacity,
                                                                                const graphene_point_t         *offset,
                                                                                const GskRoundedRect           *outline,
                                                                                const GdkColor                 *color);


G_END_DECLS

