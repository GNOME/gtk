#pragma once

#include "gskgpucachedprivate.h"

#include "gsk/gskpath.h"
#include "gsk/gsktransform.h"

#include <graphene.h>

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_cached_fill_init_cache                  (GskGpuCache            *cache);
void                    gsk_gpu_cached_fill_finish_cache                (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cached_fill_lookup                      (GskGpuCache            *self,
                                                                         GskGpuFrame            *frame,
                                                                         const graphene_size_t  *scale,
                                                                         const graphene_rect_t  *bounds,
                                                                         GskTransform           *modelview,
                                                                         GskPath                *path,
                                                                         GskFillRule             fill_rule,
                                                                         const GdkColor         *color_hint,
                                                                         graphene_rect_t        *out_rect,
                                                                         GdkColor               *out_color);


G_END_DECLS
