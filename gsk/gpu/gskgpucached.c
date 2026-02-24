#include "config.h"

#include "gskgpucachedprivate.h"

#include "gskgpucachedatlasprivate.h"

gboolean
gsk_gpu_cached_is_old (GskGpuCached *cached,
                       gint64        cache_timeout,
                       gint64        timestamp)
{
  if (cache_timeout < 0)
    return -1;
  else
    return timestamp - cached->timestamp > cache_timeout;
}

void
gsk_gpu_cached_set_stale (GskGpuCached *cached,
                          gboolean      stale)
{
  if (cached->stale == stale)
    return;

  cached->stale = stale;

  if (cached->atlas)
    gsk_gpu_cached_atlas_set_item_stale (cached->atlas, cached, stale);
}

void
gsk_gpu_cached_print_no_stats (GskGpuCache *cache,
                               GString     *string)
{
}

