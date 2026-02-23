#pragma once

#include "gskgpucachedprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

GskGpuCachedAtlas *     gsk_gpu_cached_atlas_new                        (GskGpuCache                    *cache,
                                                                         gsize                           width,
                                                                         gsize                           height);

gpointer                gsk_gpu_cached_atlas_create                     (GskGpuCachedAtlas              *self,
                                                                         const GskGpuCachedClass        *class,
                                                                         gsize                           width,
                                                                         gsize                           height);
void                    gsk_gpu_cached_atlas_deallocate                 (GskGpuCachedAtlas              *self,
                                                                         GskGpuCached                   *cached);

G_END_DECLS
