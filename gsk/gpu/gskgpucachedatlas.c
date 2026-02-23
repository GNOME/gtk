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
};

static void
gsk_gpu_cached_atlas_free (GskGpuCached *cached)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;
#if 0
  GskGpuCache *cache = cached->cache;
  GskGpuCached *c, *next;

  /* Free all remaining glyphs on this atlas */
  for (c = cache->first_cached; c != NULL; c = next)
    {
      next = c->next;
      if (c->atlas == self)
        gsk_gpu_cached_free (c);
    }

  if (cache->current_atlas == self)
    cache->current_atlas = NULL;
#endif

  gsk_atlas_allocator_free (self->allocator);
  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_atlas_should_collect (GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  //GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;

  return FALSE;
}

static const GskGpuCachedClass GSK_GPU_CACHED_ATLAS_CLASS =
{
  sizeof (GskGpuCachedAtlas),
  "Atlas",
  gsk_gpu_cached_atlas_free,
  gsk_gpu_cached_atlas_should_collect
};

GskGpuCachedAtlas *
gsk_gpu_cached_atlas_new (GskGpuCache *cache,
                          gsize        width,
                          gsize        height)
{
  GskGpuCachedAtlas *self;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_ATLAS_CLASS);
  self->allocator = gsk_atlas_allocator_new (width, height);
  self->image = gsk_gpu_device_create_atlas_image (gsk_gpu_cache_get_device (cache), width, height);

  return self;
}

static void
gsk_gpu_cached_atlas_purge_stale (GskGpuCachedAtlas *self)
{
  /*
  gsize i;

  for (i = 0; i < gsk_gpu_atlas_slots_get_size (&self->slots); i++)
    {
      GskGpuAtlasSlot *slot = gsk_gpu_atlas_slots_get (&self->slots, i);

      if (slot->type == GSK_GPU_ATLAS_USED &&
          slot->owner->stale)
        {
          gsk_gpu_cached_free (slot->owner);
        }
    }
    */
}

void
gsk_gpu_cached_atlas_deallocate (GskGpuCachedAtlas *self,
                                 GskGpuCached      *cached)
{
  gsk_atlas_allocator_deallocate (self->allocator, cached->atlas_slot);
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

