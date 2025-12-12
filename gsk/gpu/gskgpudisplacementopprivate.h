#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_displacement_op                         (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         const graphene_point_t         *offset,
                                                                         float                           opacity,
                                                                         const graphene_rect_t          *bounds,
                                                                         const GskGpuShaderImage        *child,
                                                                         const GskGpuShaderImage        *displacement,
                                                                         const guint                     channels[2],
                                                                         const graphene_size_t          *max,
                                                                         const graphene_size_t          *scale,
                                                                         const graphene_point_t         *offset2);

G_END_DECLS

