#pragma once

#include "gskgpushaderopprivate.h"
#include "gskpath.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_fill_op                                 (GskGpuFrame                    *frame,
                                                                         GskGpuShaderClip                clip,
                                                                         GdkColorState                  *ccs,
                                                                         float                           opacity,
                                                                         const graphene_point_t         *offset,
                                                                         const graphene_rect_t          *rect,
                                                                         GskPath                        *path,
                                                                         GskFillRule                     fill_rule,
                                                                         const GdkColor                 *color);


G_END_DECLS

