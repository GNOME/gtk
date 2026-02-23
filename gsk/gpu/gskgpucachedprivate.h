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
  gsize atlas_slot;
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
gpointer                gsk_gpu_cached_new_from_atlas                   (GskGpuCache                    *cache,
                                                                         const GskGpuCachedClass        *class,
                                                                         gsize                           width,
                                                                         gsize                           height);
void                    gsk_gpu_cached_free                             (GskGpuCached                   *cached);

void                    gsk_gpu_cached_use                              (GskGpuCached                   *cached);
gboolean                gsk_gpu_cached_is_old                           (GskGpuCached                   *cached,
                                                                         gint64                          cache_timeout,
                                                                         gint64                          timestamp);

GskGpuImage *           gsk_gpu_cached_get_atlas_image                  (GskGpuCached                   *cached);
const cairo_rectangle_int_t *
                        gsk_gpu_cached_get_atlas_area                   (GskGpuCached                   *cached);

void                    gsk_gpu_cached_set_stale                        (GskGpuCached                   *cached,
                                                                         gboolean                        stale);

G_END_DECLS
