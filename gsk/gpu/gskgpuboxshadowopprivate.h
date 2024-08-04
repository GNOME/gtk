#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"
#include "gdkcolorprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_box_shadow_op                                  (GskGpuFrame                    *frame,
                                                                                GskGpuShaderClip                clip,
                                                                                GdkColorState                  *ccs,
                                                                                float                           opacity,
                                                                                const graphene_point_t         *offset,
                                                                                gboolean                        inset,
                                                                                const graphene_rect_t          *bounds,
                                                                                const GskRoundedRect           *outline,
                                                                                const graphene_point_t         *shadow_offset,
                                                                                float                           spread,
                                                                                float                           blur_radius,
                                                                                const GdkColor                 *color);


G_END_DECLS

