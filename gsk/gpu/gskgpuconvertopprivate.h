#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_convert_op                              (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuColorStates               color_states,
                                                                         float                           opacity,
                                                                         GskGpuDescriptors              *desc,
                                                                         guint32                         descriptor,
                                                                         gboolean                        straight_alpha,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);


G_END_DECLS

