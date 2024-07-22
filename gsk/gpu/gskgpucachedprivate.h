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

gpointer                gsk_gpu_cached_new                              (GskGpuCache                    *cache,
                                                                         const GskGpuCachedClass        *class);

void                    gsk_gpu_cached_use                              (GskGpuCached                   *cached);

G_END_DECLS
