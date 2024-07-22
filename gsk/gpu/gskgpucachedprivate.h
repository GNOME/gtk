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

  void                  (* free)                        (GskGpuCache            *cache,
                                                         GskGpuCached           *cached);
  gboolean              (* should_collect)              (GskGpuCache            *cache,
                                                         GskGpuCached           *cached,
                                                         gint64                  cache_timeout,
                                                         gint64                  timestamp);
};

struct _GskGpuCached
{
  const GskGpuCachedClass *class;

  GskGpuCachedAtlas *atlas;
  GskGpuCached *next;
  GskGpuCached *prev;

  gint64 timestamp;
  gboolean stale;
  guint pixels;   /* For glyphs and textures, pixels. For atlases, alive pixels */
};

gpointer                gsk_gpu_cached_new                              (GskGpuCache            *cache,
                                                                         const GskGpuCachedClass *class);

G_END_DECLS
