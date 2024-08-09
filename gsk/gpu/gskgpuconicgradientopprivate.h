#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernodeprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_conic_gradient_op                       (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         GdkColorState                  *ics,
                                                                         GskHueInterpolation             hue_interp,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *center,
                                                                         float                           angle,
                                                                         const GskColorStop2            *stops,
                                                                         gsize                           n_stops);


G_END_DECLS

