#pragma once

#include "gskgpushaderopprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_colorize_op                             (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GskGpuImage                    *image,
                                                                         const graphene_rect_t          *rect,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *tex_rect,
                                                                         const GdkRGBA                  *color);


G_END_DECLS

