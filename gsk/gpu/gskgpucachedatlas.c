#include "config.h"

#include "gskgpucachedatlasprivate.h"

#include "gskatlasallocatorprivate.h"
#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"

struct _GskGpuCachedAtlas
{
  GskGpuCached parent;

  GskAtlasAllocator *allocator;
  GskGpuImage *image;

  gsize used_pixels;
  gsize stale_pixels;
};

static void
gsk_gpu_cached_atlas_print_stats (GskGpuCache *cache,
                                  GString     *string)
{
  GskGpuCachePrivate *priv;
  GList *l;
  gboolean first;

  priv = gsk_gpu_cache_get_private (cache);

  g_string_append (string, "filled: ");
  for (l = g_queue_peek_head_link (&priv->atlas_queue); l; l = l->next)
    {
      GskGpuCachedAtlas *self = l->data;

      if (!first)
        g_string_append (string, ", ");
      g_string_append_printf (string, "%zu%%",
                              (self->used_pixels - self->stale_pixels) * 100 /
                              ((GskGpuCached *) self)->pixels);
      first = FALSE;
    }
}

static void
gsk_gpu_cached_atlas_finalize (GskGpuCached *cached)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;
  GskAtlasAllocatorIter iter;
  GskGpuCachePrivate *priv;
  gsize pos;

  priv = gsk_gpu_cache_get_private (cached->cache);
  g_queue_remove (&priv->atlas_queue, self);

  gsk_atlas_allocator_iter_init (self->allocator, &iter);
  for (pos = gsk_atlas_allocator_iter_next (self->allocator, &iter);
       pos != G_MAXSIZE;
       pos = gsk_atlas_allocator_iter_next (self->allocator, &iter))
    {
      GskGpuCached *item;

      item = gsk_atlas_allocator_get_user_data (self->allocator, pos);
      gsk_gpu_cached_free (item);
    }

  g_assert (self->used_pixels == 0);

  gsk_atlas_allocator_free (self->allocator);
  g_object_unref (self->image);
}

static gboolean
gsk_gpu_cached_atlas_should_collect (GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;

  if (gsk_gpu_cached_is_old (cached, cache_timeout, timestamp) &&
      self->used_pixels == self->stale_pixels)
    return TRUE;

  return FALSE;
}

static const GskGpuCachedClass GSK_GPU_CACHED_ATLAS_CLASS =
{
  sizeof (GskGpuCachedAtlas),
  "Atlas",
  FALSE,
  gsk_gpu_cached_atlas_print_stats,
  gsk_gpu_cached_atlas_finalize,
  gsk_gpu_cached_atlas_should_collect
};

GskGpuCachedAtlas *
gsk_gpu_cached_atlas_new (GskGpuCache *cache,
                          gsize        width,
                          gsize        height)
{
  GskGpuCachedAtlas *self;
  GskGpuCached *cached;
  GskGpuCachePrivate *priv;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_ATLAS_CLASS);
  cached = (GskGpuCached *) self;

  self->allocator = gsk_atlas_allocator_new (width, height);
  self->image = gsk_gpu_device_create_atlas_image (gsk_gpu_cache_get_device (cache), width, height);

  cached->pixels = width * height;

  priv = gsk_gpu_cache_get_private (cache);
  g_queue_push_head (&priv->atlas_queue, self);

  return self;
}

static void
gsk_gpu_cached_atlas_purge_stale (GskGpuCachedAtlas *self)
{
  GskAtlasAllocatorIter iter;
  gsize pos;

  gsk_atlas_allocator_iter_init (self->allocator, &iter);
  for (pos = gsk_atlas_allocator_iter_next (self->allocator, &iter);
       pos != G_MAXSIZE;
       pos = gsk_atlas_allocator_iter_next (self->allocator, &iter))
    {
      GskGpuCached *item;

      item = gsk_atlas_allocator_get_user_data (self->allocator, pos);
      if (item->stale)
        gsk_gpu_cached_free (item);
    }
}

static gsize 
gsk_gpu_cached_atlas_get_item_pixels (GskGpuCachedAtlas *self,
                                      GskGpuCached      *item)
{
  const cairo_rectangle_int_t *area;

  /* We use the area over cached->pixels so that the cached can rewrite
   * the pixels for whatever */
  area = gsk_atlas_allocator_get_area (self->allocator, item->atlas_slot);
  return area->width * area->height;
}

void
gsk_gpu_cached_atlas_deallocate (GskGpuCachedAtlas *self,
                                 GskGpuCached      *cached)
{
  gsize pixels;

  pixels = gsk_gpu_cached_atlas_get_item_pixels (self, cached);
  self->used_pixels -= pixels;
  if (cached->stale)
    self->stale_pixels -= pixels;

  if (!cached->stale)
    gsk_gpu_cached_use ((GskGpuCached *) self);

  gsk_atlas_allocator_deallocate (self->allocator, cached->atlas_slot);

  cached->atlas = NULL;
  cached->atlas_slot = 0;
}

gpointer
gsk_gpu_cached_atlas_create (GskGpuCachedAtlas       *self,
                             const GskGpuCachedClass *class, 
                             gsize                    width,
                             gsize                    height)
{
  GskGpuCached *cached;
  gsize pos;

  pos = gsk_atlas_allocator_allocate (self->allocator, width, height);
  if (pos == G_MAXSIZE)
    {
      gsk_gpu_cached_atlas_purge_stale (self);
      pos = gsk_atlas_allocator_allocate (self->allocator, width, height);
      if (pos == G_MAXSIZE)
        return NULL;
    }

  cached = gsk_gpu_cached_new (((GskGpuCached *) self)->cache, class);
  cached->atlas = self;
  cached->atlas_slot = pos;

  gsk_atlas_allocator_set_user_data (self->allocator, pos, cached);

  self->used_pixels += width * height;
  gsk_gpu_cached_use ((GskGpuCached *) self);

  return cached;
}

GskGpuImage *
gsk_gpu_cached_get_atlas_image (GskGpuCached *cached)
{
  if (cached->atlas == NULL)
    return NULL;

  return cached->atlas->image;
}

const cairo_rectangle_int_t *
gsk_gpu_cached_get_atlas_area (GskGpuCached *cached)
{
  if (cached->atlas == NULL)
    return NULL;

  return gsk_atlas_allocator_get_area (cached->atlas->allocator, cached->atlas_slot);
}

void
gsk_gpu_cached_atlas_set_item_stale (GskGpuCachedAtlas *self,
                                     GskGpuCached      *item,
                                     gboolean           stale)
{
  gsize pixels;

  pixels = gsk_gpu_cached_atlas_get_item_pixels (self, item);

  if (stale)
    {
      self->stale_pixels += pixels;
    }
  else
    {
      self->stale_pixels -= pixels;
    }

  gsk_gpu_cached_use ((GskGpuCached *) self);
}

void
gsk_gpu_cached_atlas_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_queue_init (&priv->atlas_queue);
}

void
gsk_gpu_cached_atlas_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_queue_clear (&priv->atlas_queue);
}
