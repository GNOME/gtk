#include "config.h"

#include "gskgpucachedtileprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpuimageprivate.h"

typedef struct _GskGpuCachedTile GskGpuCachedTile;

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

  GskGpuImage *image;
  GdkColorState *color_state;
};

static void
gsk_gpu_cached_tile_finalize (GskGpuCached *cached)
{
  GskGpuCachedTile *self = (GskGpuCachedTile *) cached;
  GskGpuCache *cache = cached->cache;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  gpointer key, value;

  g_clear_object (&self->image);
  g_clear_pointer (&self->color_state, gdk_color_state_unref);

  if (g_hash_table_steal_extended (priv->tile_cache, self, &key, &value))
    {
      /* If the texture has been reused already, we put the entry back */
      if ((GskGpuCached *) value != cached)
        g_hash_table_insert (priv->tile_cache, key, value);
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
  gsk_gpu_cached_print_no_stats,
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
      gsk_gpu_cached_add_dead_pixels (((GskGpuCached *) self)->cache,
                                      1,
                                      ((GskGpuCached *) self)->pixels);
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
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskGpuCachedTile *self;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_TILE_CLASS);
  self->texture = texture;
  self->lod_level = lod_level;
  self->lod_linear = lod_linear;
  self->tile_id = tile_id;
  self->image = g_object_ref (image);
  self->color_state = gdk_color_state_ref (color_state);
  ((GskGpuCached *)self)->pixels = gsk_gpu_image_get_width (image) * gsk_gpu_image_get_height (image);
  self->use_count = 2;

  g_object_weak_ref (G_OBJECT (texture), (GWeakNotify) gsk_gpu_cached_tile_destroy_cb, self);
  if (priv->tile_cache == NULL)
    priv->tile_cache = g_hash_table_new (gsk_gpu_cached_tile_hash,
                                         gsk_gpu_cached_tile_equal);
  g_hash_table_add (priv->tile_cache, self);

  return self;
}

GskGpuImage *
gsk_gpu_cache_lookup_tile (GskGpuCache      *cache,
                           GdkTexture       *texture,
                           guint             lod_level,
                           GskScalingFilter  lod_filter,
                           gsize             tile_id,
                           GdkColorState   **out_color_state)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskGpuCachedTile *tile;
  GskGpuCachedTile lookup = {
    .texture = texture,
    .lod_level = lod_level,
    .lod_linear = lod_filter == GSK_SCALING_FILTER_TRILINEAR,
    .tile_id = tile_id
  };

  if (priv->tile_cache == NULL)
    return NULL;

  tile = g_hash_table_lookup (priv->tile_cache, &lookup);
  if (tile == NULL)
    return NULL;

  gsk_gpu_cached_use ((GskGpuCached *) tile);

  *out_color_state = tile->color_state;

  return g_object_ref (tile->image);
}

void
gsk_gpu_cache_cache_tile (GskGpuCache      *cache,
                          GdkTexture       *texture,
                          guint             lod_level,
                          GskScalingFilter  lod_filter,
                          gsize             tile_id,
                          GskGpuImage      *image,
                          GdkColorState    *color_state)
{
  GskGpuCachedTile *tile;

  tile = gsk_gpu_cached_tile_new (cache,
                                  texture,
                                  lod_level,
                                  lod_filter == GSK_SCALING_FILTER_TRILINEAR,
                                  tile_id,
                                  image,
                                  color_state);

  gsk_gpu_cached_use ((GskGpuCached *) tile);
}

void
gsk_gpu_cached_tile_init_cache (GskGpuCache *cache)
{
}

void
gsk_gpu_cached_tile_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_clear_pointer (&priv->tile_cache, g_hash_table_unref);
}
