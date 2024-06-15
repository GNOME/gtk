#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_color_convert_op                        (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *from,
                                                                         GdkColorState                  *to,
                                                                         GskGpuDescriptors              *desc,
                                                                         guint32                         descriptor,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);

/* For lack of a better place */
guint gsk_gpu_color_conversion        (GdkColorState *from,
                                       GdkColorState *to);
guint gsk_gpu_color_conversion_triple (GdkColorState *from1,
                                       GdkColorState *from2,
                                       GdkColorState *to);

G_END_DECLS

