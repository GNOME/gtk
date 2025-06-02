#pragma once

#include "gskgpucachedprivate.h"

#include "gsk/gskpath.h"
#include "gsk/gskstroke.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_cached_stroke_init_cache                (GskGpuCache            *cache);
void                    gsk_gpu_cached_stroke_finish_cache              (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cached_stroke_lookup                    (GskGpuCache            *self,
                                                                         GskGpuFrame            *frame,
                                                                         const graphene_vec2_t  *scale,
                                                                         const graphene_rect_t  *bounds,
                                                                         GskPath                *path,
                                                                         const GskStroke        *stroke,
                                                                         graphene_rect_t        *rect);


G_END_DECLS
