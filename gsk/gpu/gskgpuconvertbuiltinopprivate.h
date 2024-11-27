#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_convert_from_builtin_op                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         GdkColorState                  *builtin,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image);

void                    gsk_gpu_convert_to_builtin_op                   (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *builtin,
                                                                         gboolean                        builtin_premultiplied,
                                                                         GdkColorState                  *source_cs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const GskGpuShaderImage        *image);


G_END_DECLS
