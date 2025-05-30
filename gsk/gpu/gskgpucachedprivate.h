#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

typedef struct _GskGpuCached GskGpuCached;
typedef struct _GskGpuCachedClass GskGpuCachedClass;
typedef struct _GskGpuCachedAtlas GskGpuCachedAtlas;

struct _GskGpuCachedClass
{
  gsize size;
  const char *name;

  void                  (* free)                        (GskGpuCached           *cached);
  gboolean              (* should_collect)              (GskGpuCached           *cached,
                                                         gint64                  cache_timeout,
                                                         gint64                  timestamp);
};

struct _GskGpuCached
{
  const GskGpuCachedClass *class;

  GskGpuCache *cache;
  GskGpuCachedAtlas *atlas;
  GskGpuCached *next;
  GskGpuCached *prev;

  gint64 timestamp;
  gboolean stale;
  guint pixels;   /* For glyphs and textures, pixels. For atlases, alive pixels */
};

struct _GskGpuCachePrivate
{
  GHashTable *glyph_cache;
  GHashTable *fill_cache;
  GHashTable *stroke_cache;

  /* Vulkan-specific */
  GHashTable *ycbcr_cache;
};

gpointer                gsk_gpu_cached_new                              (GskGpuCache                    *cache,
                                                                         const GskGpuCachedClass        *class);
gpointer                gsk_gpu_cached_new_from_current_atlas           (GskGpuCache                    *cache,
                                                                         const GskGpuCachedClass        *class);

void                    gsk_gpu_cached_use                              (GskGpuCached                   *cached);

static inline gboolean
gsk_gpu_cached_is_old (GskGpuCached *cached,
                       gint64        cache_timeout,
                       gint64        timestamp)
{
  if (cache_timeout < 0)
    return -1;
  else
    return timestamp - cached->timestamp > cache_timeout;
}

static inline void
gsk_gpu_cached_set_stale (GskGpuCached *cached,
                          gboolean      stale)
{
  if (cached->stale == stale)
    return;

  cached->stale = stale;

  if (cached->atlas)
    {
      if (stale)
        ((GskGpuCached *) cached->atlas)->pixels -= cached->pixels;
      else
        ((GskGpuCached *) cached->atlas)->pixels += cached->pixels;
    }
}


G_END_DECLS
