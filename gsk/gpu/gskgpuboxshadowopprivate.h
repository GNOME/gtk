#pragma once

#include "gskgputypesprivate.h"
#include "gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_box_shadow_op                                  (GskGpuFrame                    *frame,
                                                                                GskGpuShaderClip                clip,
                                                                                GskGpuColorStates               color_states,
                                                                                gboolean                        inset,
                                                                                const graphene_rect_t          *bounds,
                                                                                const GskRoundedRect           *outline,
                                                                                const graphene_point_t         *shadow_offset,
                                                                                float                           spread,
                                                                                float                           blur_radius,
                                                                                const graphene_point_t         *offset,
                                                                                const float                     color[4]);


G_END_DECLS

