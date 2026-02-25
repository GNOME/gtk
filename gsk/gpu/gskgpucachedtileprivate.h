#pragma once

#include "gskgpucachedprivate.h"

G_BEGIN_DECLS

void                    gsk_gpu_cached_tile_init_cache                  (GskGpuCache            *cache);
void                    gsk_gpu_cached_tile_finish_cache                (GskGpuCache            *cache);

GskGpuImage *           gsk_gpu_cache_lookup_tile                       (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         guint                   lod_level,
                                                                         GskScalingFilter        lod_filter,
                                                                         gsize                   tile_id,
                                                                         GdkColorState         **out_color_state);
void                    gsk_gpu_cache_cache_tile                        (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         guint                   lod_level,
                                                                         GskScalingFilter        lod_filter,
                                                                         gsize                   tile_id,
                                                                         GskGpuImage            *image,
                                                                         GdkColorState          *color_state);


G_END_DECLS
