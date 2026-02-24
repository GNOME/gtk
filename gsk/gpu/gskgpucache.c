#include "config.h"

#include "gskgpucacheprivate.h"

#include "gskgpucachedatlasprivate.h"
#include "gskgpucachedglyphprivate.h"
#include "gskgpucachedfillprivate.h"
#include "gskgpucachedstrokeprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanycbcrprivate.h"
#endif

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdktextureprivate.h"

#include "gsk/gskdebugprivate.h"

#define ATLAS_SIZE 1024

#define MAX_ATLAS_ITEM_SIZE 256

G_STATIC_ASSERT (MAX_ATLAS_ITEM_SIZE < ATLAS_SIZE);

typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;
typedef struct _GskGpuCachedTexture GskGpuCachedTexture;
typedef struct _GskGpuCachedTile GskGpuCachedTile;

struct _GskGpuCache
{
  GObject parent_instance;

  GskGpuDevice *device;
  gint64 timestamp;

  GskGpuCached *first_cached;
  GskGpuCached *last_cached;

  GHashTable *texture_cache;
  GHashTable *ccs_texture_caches[GDK_COLOR_STATE_N_IDS];
  GHashTable *tile_cache;

  /* atomic */ gsize dead_textures;
  /* atomic */ gsize dead_texture_pixels;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuCache, gsk_gpu_cache, G_TYPE_OBJECT)

/* {{{ Cached base class */

void
gsk_gpu_cached_free (GskGpuCached *cached)
{
  GskGpuCache *self = cached->cache;
  const GskGpuCachedClass *class = cached->class;

  if (cached->next)
    cached->next->prev = cached->prev;
  else
    self->last_cached = cached->prev;
  if (cached->prev)
    cached->prev->next = cached->next;
  else
    self->first_cached = cached->next;

  gsk_gpu_cached_set_stale (cached, TRUE);

  class->finalize (cached);

  if (!class->frees_memory_itself)
    {
      if (cached->atlas)
        gsk_gpu_cached_atlas_deallocate (cached->atlas, cached);

      g_free (cached);
    }
}

static gboolean
gsk_gpu_cached_should_collect (GskGpuCached *cached,
                               gint64        cache_timeout,
                               gint64        timestamp)
{
  return cached->class->should_collect (cached, cache_timeout, timestamp);
}

gpointer
gsk_gpu_cached_new (GskGpuCache             *cache,
                    const GskGpuCachedClass *class)
{
  GskGpuCached *cached;

  cached = g_malloc0 (class->size);

  cached->cache = cache;
  cached->class = class;

  cached->prev = cache->last_cached;
  cache->last_cached = cached;
  if (cached->prev)
    cached->prev->next = cached;
  else
    cache->first_cached = cached;

  return cached;
}

void
gsk_gpu_cached_use (GskGpuCached *cached)
{
  cached->timestamp = cached->cache->timestamp;
  gsk_gpu_cached_set_stale (cached, FALSE);
}

gpointer
gsk_gpu_cached_new_from_atlas (GskGpuCache             *self,
                               const GskGpuCachedClass *class,
                               gsize                    width,
                               gsize                    height)
{
  GskGpuCachePrivate *priv;
  GskGpuCachedAtlas *atlas;
  GskGpuCached *cached;

  if (width > MAX_ATLAS_ITEM_SIZE || height > MAX_ATLAS_ITEM_SIZE)
    return NULL;

  priv = gsk_gpu_cache_get_private (self);

  atlas = g_queue_peek_head (&priv->atlas_queue);

  if (atlas)
    {
      /* 1. Try the current atlas */
      cached = gsk_gpu_cached_atlas_create (atlas, class, width, height);
      if (cached)
        return cached;
      
      /* 2. It's full, try to see if the oldest atlas
       * has space again */
      atlas = g_queue_peek_tail (&priv->atlas_queue);
      cached = gsk_gpu_cached_atlas_create (atlas, class, width, height);
      if (cached)
        {
          /* It worked, use it as default by moving it to the front of the queue */
          atlas = g_queue_pop_tail (&priv->atlas_queue);
          g_queue_push_head (&priv->atlas_queue, atlas);

          return cached;
        }
    }

  /* 3. Nothing worked so far, try a new atlas
   * Note: It puts itself into the atlas queue */
  atlas = gsk_gpu_cached_atlas_new (self, ATLAS_SIZE, ATLAS_SIZE);
  return gsk_gpu_cached_atlas_create (atlas, class, width, height);
}

/* }}} */
/* {{{ CachedTexture */

struct _GskGpuCachedTexture
{
  GskGpuCached parent;

  /* atomic */ int use_count; /* We count the use by the cache (via the linked
                               * list) and by the texture (via render data or
                               * weak ref.
                               */

  gsize *dead_textures_counter;
  gsize *dead_pixels_counter;

  GdkTexture *texture;
  GskGpuImage *image;
  GdkColorState *color_state;  /* no ref because global. May be NULL */
};

static GHashTable *
gsk_gpu_cache_get_texture_hash_table (GskGpuCache   *cache,
                                      GdkColorState *color_state)
{
  if (color_state == NULL)
    {
      return cache->texture_cache;
    }
  else if (GDK_IS_DEFAULT_COLOR_STATE (color_state))
    {
      GdkColorStateId id = GDK_DEFAULT_COLOR_STATE_ID (color_state);

      if (cache->ccs_texture_caches[id] == NULL)
        cache->ccs_texture_caches[id] = g_hash_table_new (g_direct_hash,
                                                          g_direct_equal);
      return cache->ccs_texture_caches[id];
    }
  else
    {
      return NULL;
    }
}

static void
gsk_gpu_cached_texture_finalize (GskGpuCached *cached)
{
  GskGpuCachedTexture *self = (GskGpuCachedTexture *) cached;
  GskGpuCache *cache = cached->cache;
  GHashTable *texture_cache;
  gpointer key, value;

  g_clear_object (&self->image);

  texture_cache = gsk_gpu_cache_get_texture_hash_table (cache, self->color_state);

  if (g_hash_table_steal_extended (texture_cache, self->texture, &key, &value))
    {
      /* If the texture has been reused already, we put the entry back */
      if ((GskGpuCached *) value != cached)
        g_hash_table_insert (texture_cache, key, value);
    }

  /* If the cached item itself is still in use by the texture, we leave
   * it to the weak ref or render data to free it.
   */
  if (g_atomic_int_dec_and_test (&self->use_count))
    {
      g_free (self);
      return;
    }
}

static inline gboolean
gsk_gpu_cached_texture_is_invalid (GskGpuCachedTexture *self)
{
  /* If the use count is less than 2, the orignal texture has died,
   * and the memory may have been reused for a new texture, so we
   * can't hand out the image that is for the original texture.
   */
  return g_atomic_int_get (&self->use_count) < 2;
}

static gboolean
gsk_gpu_cached_texture_should_collect (GskGpuCached *cached,
                                       gint64        cache_timeout,
                                       gint64        timestamp)
{
  GskGpuCachedTexture *self = (GskGpuCachedTexture *) cached;

  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp) ||
         gsk_gpu_cached_texture_is_invalid (self);
}

static const GskGpuCachedClass GSK_GPU_CACHED_TEXTURE_CLASS =
{
  sizeof (GskGpuCachedTexture),
  "Texture",
  TRUE,
  gsk_gpu_cached_texture_finalize,
  gsk_gpu_cached_texture_should_collect
};

/* Note: this function can run in an arbitrary thread, so it can
 * only access things atomically
 */
static void
gsk_gpu_cached_texture_destroy_cb (gpointer data)
{
  GskGpuCachedTexture *self = data;

  if (!gsk_gpu_cached_texture_is_invalid (self))
    {
      g_atomic_pointer_add (self->dead_textures_counter, 1);
      g_atomic_pointer_add (self->dead_pixels_counter, ((GskGpuCached *) self)->pixels);
    }

  if (g_atomic_int_dec_and_test (&self->use_count))
    g_free (self);
}

static GskGpuCachedTexture *
gsk_gpu_cached_texture_new (GskGpuCache   *cache,
                            GdkTexture    *texture,
                            GskGpuImage   *image,
                            GdkColorState *color_state)
{
  GskGpuCachedTexture *self;
  GHashTable *texture_cache;

  texture_cache = gsk_gpu_cache_get_texture_hash_table (cache, color_state);
  if (texture_cache == NULL)
    return NULL;

  /* First, move any existing renderdata */
  self = gdk_texture_get_render_data (texture, cache);
  if (self)
    {
      if (gsk_gpu_cached_texture_is_invalid (self))
        {
          gdk_texture_clear_render_data (texture);
        }
      else
        {
          gdk_texture_steal_render_data (texture);
          g_object_weak_ref (G_OBJECT (texture), (GWeakNotify) gsk_gpu_cached_texture_destroy_cb, self);
          texture_cache = gsk_gpu_cache_get_texture_hash_table (cache, self->color_state);
          g_assert (texture_cache != NULL);
          g_hash_table_insert (texture_cache, texture, self);
        }
    }

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_TEXTURE_CLASS);
  self->texture = texture;
  self->image = g_object_ref (image);
  self->color_state = color_state;
  ((GskGpuCached *)self)->pixels = gsk_gpu_image_get_width (image) * gsk_gpu_image_get_height (image);
  self->dead_textures_counter = &cache->dead_textures;
  self->dead_pixels_counter = &cache->dead_texture_pixels;
  self->use_count = 2;

  if (!gdk_texture_set_render_data (texture, cache, self, gsk_gpu_cached_texture_destroy_cb))
    {
      g_object_weak_ref (G_OBJECT (texture), (GWeakNotify) gsk_gpu_cached_texture_destroy_cb, self);

      g_hash_table_insert (texture_cache, texture, self);
    }

  return self;
}

/* }}} */
/* {{{ CachedTile */

struct _GskGpuCachedTile
{
  GskGpuCached parent;

  GdkTexture *texture;
  guint lod_level;
  gboolean lod_linear;
  gsize tile_id;

  /* atomic */ int use_count; /* We count the use by the cache (via the linked
                               * list) and by the texture (via weak ref)
                               */

  gsize *dead_textures_counter;
  gsize *dead_pixels_counter;

  GskGpuImage *image;
  GdkColorState *color_state;
};

static void
gsk_gpu_cached_tile_finalize (GskGpuCached *cached)
{
  GskGpuCachedTile *self = (GskGpuCachedTile *) cached;
  GskGpuCache *cache = cached->cache;
  gpointer key, value;

  g_clear_object (&self->image);
  g_clear_pointer (&self->color_state, gdk_color_state_unref);

  if (g_hash_table_steal_extended (cache->tile_cache, self, &key, &value))
    {
      /* If the texture has been reused already, we put the entry back */
      if ((GskGpuCached *) value != cached)
        g_hash_table_insert (cache->tile_cache, key, value);
    }

  /* If the cached item itself is still in use by the texture, we leave
   * it to the weak ref or render data to free it.
   */
  if (g_atomic_int_dec_and_test (&self->use_count))
    {
      g_free (self);
      return;
    }
}

static inline gboolean
gsk_gpu_cached_tile_is_invalid (GskGpuCachedTile *self)
{
  /* If the use count is less than 2, the orignal texture has died,
   * and the memory may have been reused for a new texture, so we
   * can't hand out the image that is for the original texture.
   */
  return g_atomic_int_get (&self->use_count) < 2;
}

static gboolean
gsk_gpu_cached_tile_should_collect (GskGpuCached *cached,
                                    gint64        cache_timeout,
                                    gint64        timestamp)
{
  GskGpuCachedTile *self = (GskGpuCachedTile *) cached;

  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp) ||
         gsk_gpu_cached_tile_is_invalid (self);
}

static const GskGpuCachedClass GSK_GPU_CACHED_TILE_CLASS =
{
  sizeof (GskGpuCachedTile),
  "Tile",
  TRUE,
  gsk_gpu_cached_tile_finalize,
  gsk_gpu_cached_tile_should_collect
};

/* Note: this function can run in an arbitrary thread, so it can
 * only access things atomically
 */
static void
gsk_gpu_cached_tile_destroy_cb (gpointer data)
{
  GskGpuCachedTile *self = data;

  if (!gsk_gpu_cached_tile_is_invalid (self))
    {
      g_atomic_pointer_add (self->dead_textures_counter, 1);
      g_atomic_pointer_add (self->dead_pixels_counter, ((GskGpuCached *) self)->pixels);
    }

  if (g_atomic_int_dec_and_test (&self->use_count))
    g_free (self);
}

static guint
gsk_gpu_cached_tile_hash (gconstpointer data)
{
  const GskGpuCachedTile *self = data;

  return g_direct_hash (self->texture) ^
         self->tile_id ^
         (self->lod_level << 24) ^
         (self->lod_linear << 31);
}

static gboolean
gsk_gpu_cached_tile_equal (gconstpointer data_a,
                           gconstpointer data_b)
{
  const GskGpuCachedTile *a = data_a;
  const GskGpuCachedTile *b = data_b;

  return a->texture == b->texture &&
         a->lod_level == b->lod_level &&
         a->lod_linear == b->lod_linear &&
         a->tile_id == b->tile_id;
}

static GskGpuCachedTile *
gsk_gpu_cached_tile_new (GskGpuCache   *cache,
                         GdkTexture    *texture,
                         guint          lod_level,
                         gboolean       lod_linear,
                         guint          tile_id,
                         GskGpuImage   *image,
                         GdkColorState *color_state)
{
  GskGpuCachedTile *self;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_TILE_CLASS);
  self->texture = texture;
  self->lod_level = lod_level;
  self->lod_linear = lod_linear;
  self->tile_id = tile_id;
  self->image = g_object_ref (image);
  self->color_state = gdk_color_state_ref (color_state);
  ((GskGpuCached *)self)->pixels = gsk_gpu_image_get_width (image) * gsk_gpu_image_get_height (image);
  self->dead_textures_counter = &cache->dead_textures;
  self->dead_pixels_counter = &cache->dead_texture_pixels;
  self->use_count = 2;

  g_object_weak_ref (G_OBJECT (texture), (GWeakNotify) gsk_gpu_cached_tile_destroy_cb, self);
  if (cache->tile_cache == NULL)
    cache->tile_cache = g_hash_table_new (gsk_gpu_cached_tile_hash,
                                          gsk_gpu_cached_tile_equal);
  g_hash_table_add (cache->tile_cache, self);

  return self;
}

GskGpuImage *
gsk_gpu_cache_lookup_tile (GskGpuCache      *self,
                           GdkTexture       *texture,
                           guint             lod_level,
                           GskScalingFilter  lod_filter,
                           gsize             tile_id,
                           GdkColorState   **out_color_state)
{
  GskGpuCachedTile *tile;
  GskGpuCachedTile lookup = {
    .texture = texture,
    .lod_level = lod_level,
    .lod_linear = lod_filter == GSK_SCALING_FILTER_TRILINEAR,
    .tile_id = tile_id
  };

  if (self->tile_cache == NULL)
    return NULL;

  tile = g_hash_table_lookup (self->tile_cache, &lookup);
  if (tile == NULL)
    return NULL;

  gsk_gpu_cached_use ((GskGpuCached *) tile);

  *out_color_state = tile->color_state;

  return g_object_ref (tile->image);
}

void
gsk_gpu_cache_cache_tile (GskGpuCache      *self,
                          GdkTexture       *texture,
                          guint             lod_level,
                          GskScalingFilter  lod_filter,
                          gsize             tile_id,
                          GskGpuImage      *image,
                          GdkColorState    *color_state)
{
  GskGpuCachedTile *tile;

  tile = gsk_gpu_cached_tile_new (self,
                                  texture,
                                  lod_level,
                                  lod_filter == GSK_SCALING_FILTER_TRILINEAR,
                                  tile_id,
                                  image,
                                  color_state);

  gsk_gpu_cached_use ((GskGpuCached *) tile);
}

/* }}} */
/* {{{ GskGpuCache */

GskGpuDevice *
gsk_gpu_cache_get_device (GskGpuCache *self)
{
  return self->device;
}

/*
 * gsk_gpu_cache_set_time:
 * @self: a `GskGpuCache`
 * @timestamp: time in whatever the frameclock uses
 *
 * Sets the timestamp to use for all following operations.
 * Frames should set this when they start drawing.
 **/
void
gsk_gpu_cache_set_time (GskGpuCache *self,
                        gint64       timestamp)
{
  self->timestamp = timestamp;
}

typedef struct
{
  guint n_items;
  guint n_stale_items;
  gsize n_pixels;
  gsize n_stale_pixels;
} GskGpuCacheData;

static void
print_cache_stats (GskGpuCache *self)
{
  GskGpuCached *cached;
  GString *message;
  GHashTable *classes = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  GHashTableIter iter;
  gpointer key, value;

  for (cached = self->first_cached; cached != NULL; cached = cached->next)
    {
      GskGpuCacheData *cache_data = g_hash_table_lookup (classes, cached->class);
      if (cache_data == NULL)
        {
          cache_data = g_new0 (GskGpuCacheData, 1);
          g_hash_table_insert (classes, (gpointer) cached->class, cache_data);
        }
      cache_data->n_items++;
      cache_data->n_pixels += cached->pixels;
      if (cached->stale)
        {
          cache_data->n_stale_items++;
          cache_data->n_stale_pixels += cached->pixels;
        }
    }

  message = g_string_new ("Cached items:");
  if (g_hash_table_size (classes) == 0)
    g_string_append (message, "\n  none");
  else
    g_string_append (message, "\n  Class        Items Stale      Pixels       Stale");
  g_hash_table_iter_init (&iter, classes);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const GskGpuCachedClass *class = key;
      const GskGpuCacheData *cache_data = value;

      g_string_append_printf (message, "\n  %-12s %5u %5u %'11zu %'11zu %3zu%%", 
                              class->name,
                              cache_data->n_items, cache_data->n_stale_items,
                              cache_data->n_pixels, cache_data->n_stale_pixels,
                              cache_data->n_stale_pixels * 100 / cache_data->n_pixels);

      if (class == &GSK_GPU_CACHED_TEXTURE_CLASS)
        g_string_append_printf (message, " (%u in hash)", g_hash_table_size (self->texture_cache));
    }

  gdk_debug_message ("%s", message->str);
  g_string_free (message, TRUE);
  g_hash_table_unref (classes);
}

/* Returns TRUE if everything was GC'ed */
gboolean
gsk_gpu_cache_gc (GskGpuCache *self,
                  gint64       cache_timeout)
{
  GskGpuCached *cached, *prev;
  gint64 before G_GNUC_UNUSED = GDK_PROFILER_CURRENT_TIME;
  gboolean is_empty = TRUE;

  /* We walk the cache from the end so we don't end up with prev
   * being a leftover glyph on the atlas we are freeing
   */
  for (cached = self->last_cached; cached != NULL; cached = prev)
    {
      prev = cached->prev;
      if (gsk_gpu_cached_should_collect (cached, cache_timeout, self->timestamp))
        gsk_gpu_cached_free (cached);
      else
        is_empty &= cached->stale;
    }

  g_atomic_pointer_set (&self->dead_textures, 0);
  g_atomic_pointer_set (&self->dead_texture_pixels, 0);

  if (GSK_DEBUG_CHECK (CACHE))
    print_cache_stats (self);

  gdk_profiler_end_mark (before, "Glyph cache GC", NULL);

  return is_empty;
}

gsize
gsk_gpu_cache_get_dead_textures (GskGpuCache *self)
{
  return GPOINTER_TO_SIZE (g_atomic_pointer_get (&self->dead_textures));
}

gsize
gsk_gpu_cache_get_dead_texture_pixels (GskGpuCache *self)
{
  return GPOINTER_TO_SIZE (g_atomic_pointer_get (&self->dead_texture_pixels));
}

static void
gsk_gpu_cache_clear_cache (GskGpuCache *self)
{
  for (GskGpuCached *cached = self->first_cached; cached; cached = cached->next)
    {
      if (cached->prev == NULL)
        g_assert (self->first_cached == cached);
      else
        g_assert (cached->prev->next == cached);
      if (cached->next == NULL)
        g_assert (self->last_cached == cached);
      else
        g_assert (cached->next->prev == cached);
    }

  /* We clear the cache from the end so glyphs get freed before their atlas */
  while (self->last_cached)
    gsk_gpu_cached_free (self->last_cached);

  g_assert (self->last_cached == NULL);
}

static void
gsk_gpu_cache_dispose (GObject *object)
{
  GskGpuCache *self = GSK_GPU_CACHE (object);

  gsk_gpu_cache_clear_cache (self);

  gsk_gpu_cached_stroke_finish_cache (self);
  gsk_gpu_cached_fill_finish_cache (self);

#ifdef GDK_RENDERING_VULKAN
  gsk_vulkan_ycbcr_finish_cache (self);
#endif
  gsk_gpu_cached_glyph_finish_cache (self);
  gsk_gpu_cached_atlas_finish_cache (self);

  g_clear_pointer (&self->tile_cache, g_hash_table_unref);
  for (int i = 0; i < GDK_COLOR_STATE_N_IDS; i++)
    g_clear_pointer (&self->ccs_texture_caches[i], g_hash_table_unref);
  g_hash_table_unref (self->texture_cache);

  G_OBJECT_CLASS (gsk_gpu_cache_parent_class)->dispose (object);
}

static void
gsk_gpu_cache_finalize (GObject *object)
{
  GskGpuCache *self = GSK_GPU_CACHE (object);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_gpu_cache_parent_class)->finalize (object);
}

static void
gsk_gpu_cache_class_init (GskGpuCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gpu_cache_dispose;
  object_class->finalize = gsk_gpu_cache_finalize;
}

static void
gsk_gpu_cache_init (GskGpuCache *self)
{
  self->texture_cache = g_hash_table_new (g_direct_hash,
                                          g_direct_equal);
  
  gsk_gpu_cached_atlas_init_cache (self);
  gsk_gpu_cached_glyph_init_cache (self);
#ifdef GDK_RENDERING_VULKAN
  gsk_vulkan_ycbcr_init_cache (self);
#endif
  gsk_gpu_cached_fill_init_cache (self);
  gsk_gpu_cached_stroke_init_cache (self);
}

GskGpuImage *
gsk_gpu_cache_lookup_texture_image (GskGpuCache   *self,
                                    GdkTexture    *texture,
                                    GdkColorState *color_state)
{
  GskGpuCachedTexture *cache;
  GHashTable *texture_cache;

  texture_cache = gsk_gpu_cache_get_texture_hash_table (self, color_state);
  if (texture_cache == NULL)
    return NULL;

  cache = gdk_texture_get_render_data (texture, self);
  /* color_state_equal() isn't necessary and if we'd use it,
   * we'd need to check for NULLs before */
  if (cache == NULL || color_state != cache->color_state)
    cache = g_hash_table_lookup (texture_cache, texture);

  if (!cache || !cache->image || gsk_gpu_cached_texture_is_invalid (cache))
    return NULL;

  gsk_gpu_cached_use ((GskGpuCached *) cache);

  return g_object_ref (cache->image);
}

void
gsk_gpu_cache_cache_texture_image (GskGpuCache   *self,
                                   GdkTexture    *texture,
                                   GskGpuImage   *image,
                                   GdkColorState *color_state)
{
  GskGpuCachedTexture *cache;

  cache = gsk_gpu_cached_texture_new (self, texture, image, color_state);
  if (cache == NULL)
    return;

  gsk_gpu_cached_use ((GskGpuCached *) cache);
}

GskGpuCache *
gsk_gpu_cache_new (GskGpuDevice *device)
{
  GskGpuCache *self;

  self = g_object_new (GSK_TYPE_GPU_CACHE, NULL);
  self->device = g_object_ref (device);

  return self;
}

GskGpuCachePrivate *
gsk_gpu_cache_get_private (GskGpuCache *self)
{
  return gsk_gpu_cache_get_instance_private (self);
}

