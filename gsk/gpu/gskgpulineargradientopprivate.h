#pragma once

#include "gskgpushaderopprivate.h"

#include "gskrendernodeprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_linear_gradient_op                      (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         GdkColorState                  *ics,
                                                                         GskHueInterpolation             hue_interp,
                                                                         gboolean                        repeating,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *start,
                                                                         const graphene_point_t         *end,
                                                                         const GskColorStop2            *stops,
                                                                         gsize                           n_stops);


void gsk_adjust_hue (GdkColorState       *ics,
                     GskHueInterpolation  interp,
                     const float          color1[4],
                     float                color2[4]);

G_END_DECLS

