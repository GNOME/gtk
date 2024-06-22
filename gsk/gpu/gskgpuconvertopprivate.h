#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_convert_op                              (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *from,
                                                                         gboolean                        from_premultiplied,
                                                                         GdkColorState                  *to,
                                                                         gboolean                        to_premultiplied,
                                                                         float                           opacity,
                                                                         GskGpuDescriptors              *desc,
                                                                         guint32                         descriptor,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect);


G_END_DECLS

