#pragma once

#include "gskgpucachedprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

void                    gsk_gpu_cached_atlas_init_cache                 (GskGpuCache                    *cache);
void                    gsk_gpu_cached_atlas_finish_cache               (GskGpuCache                    *cache);

GskGpuCachedAtlas *     gsk_gpu_cached_atlas_new                        (GskGpuCache                    *cache,
                                                                         gsize                           width,
                                                                         gsize                           height);

gpointer                gsk_gpu_cached_atlas_create                     (GskGpuCachedAtlas              *self,
                                                                         const GskGpuCachedClass        *class,
                                                                         gsize                           width,
                                                                         gsize                           height);
void                    gsk_gpu_cached_atlas_deallocate                 (GskGpuCachedAtlas              *self,
                                                                         GskGpuCached                   *cached);

void                    gsk_gpu_cached_atlas_set_item_stale             (GskGpuCachedAtlas              *self,
                                                                         GskGpuCached                   *item,
                                                                         gboolean                        stale);

G_END_DECLS
