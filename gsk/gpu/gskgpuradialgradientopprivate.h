#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernodeprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_radial_gradient_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         GdkColorState                  *ics,
                                                                         GskHueInterpolation             hue_interp,
                                                                         GskRepeat                       repeat,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *start_center,
                                                                         const graphene_point_t         *start_radius,
                                                                         const graphene_point_t         *end_center,
                                                                         const graphene_point_t         *end_radius,
                                                                         const GskGradientStop          *stops,
                                                                         gsize                           n_stops);


G_END_DECLS

