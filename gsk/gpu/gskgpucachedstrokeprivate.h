#pragma once

#include "gskgpucachedprivate.h"

#include "gsk/gskpath.h"
#include "gsk/gskstroke.h"
#include "gsk/gsktransform.h"

#include <graphene.h>

#include "gdk/gdkcolorprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_cached_stroke_init_cache                (GskGpuCache            *cache);
void                    gsk_gpu_cached_stroke_finish_cache              (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cached_stroke_lookup                    (GskGpuCache            *self,
                                                                         GskGpuFrame            *frame,
                                                                         const graphene_size_t  *scale,
                                                                         const graphene_rect_t  *bounds,
                                                                         GskTransform           *modelview,
                                                                         GskPath                *path,
                                                                         const GskStroke        *stroke,
                                                                         const GdkColor         *color_hint,
                                                                         graphene_rect_t        *out_rect,
                                                                         GdkColor               *out_color);


G_END_DECLS
