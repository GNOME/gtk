#include "config.h"

#include "gskgpucacheprivate.h"

#include "gskgpucachedglyphprivate.h"
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

#define MAX_SLICES_PER_ATLAS 64

#define ATLAS_SIZE 1024

#define MAX_ATLAS_ITEM_SIZE 256

#define MIN_ALIVE_PIXELS (ATLAS_SIZE * ATLAS_SIZE / 2)

#define ATLAS_TIMEOUT_SCALE 4

G_STATIC_ASSERT (MAX_ATLAS_ITEM_SIZE < ATLAS_SIZE);
G_STATIC_ASSERT (MIN_ALIVE_PIXELS < ATLAS_SIZE * ATLAS_SIZE);

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

  GskGpuCachedAtlas *current_atlas;

  /* atomic */ gsize dead_textures;
  /* atomic */ gsize dead_texture_pixels;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuCache, gsk_gpu_cache, G_TYPE_OBJECT)

/* {{{ Cached base class */

static void
gsk_gpu_cached_free (GskGpuCached *cached)
{
  GskGpuCache *self = cached->cache;

  if (cached->next)
    cached->next->prev = cached->prev;
  else
    self->last_cached = cached->prev;
  if (cached->prev)
    cached->prev->next = cached->next;
  else
    self->first_cached = cached->next;

  gsk_gpu_cached_set_stale (cached, TRUE);

  cached->class->free (cached);
}

static gboolean
gsk_gpu_cached_should_collect (GskGpuCached *cached,
                               gint64        cache_timeout,
                               gint64        timestamp)
{
  return cached->class->should_collect (cached, cache_timeout, timestamp);
}

static gpointer
gsk_gpu_cached_new_from_atlas (GskGpuCache             *cache,
                               const GskGpuCachedClass *class,
                               GskGpuCachedAtlas       *atlas)
{
  GskGpuCached *cached;

  cached = g_malloc0 (class->size);

  cached->cache = cache;
  cached->class = class;
  cached->atlas = atlas;

  cached->prev = cache->last_cached;
  cache->last_cached = cached;
  if (cached->prev)
    cached->prev->next = cached;
  else
    cache->first_cached = cached;

  return cached;
}

gpointer
gsk_gpu_cached_new_from_current_atlas (GskGpuCache             *cache,
                                       const GskGpuCachedClass *class)
{
  return gsk_gpu_cached_new_from_atlas (cache,
                                        class,
                                        cache->current_atlas);
}

gpointer
gsk_gpu_cached_new (GskGpuCache             *cache,
                    const GskGpuCachedClass *class)
{
  return gsk_gpu_cached_new_from_atlas (cache, class, NULL);
}

void
gsk_gpu_cached_use (GskGpuCached *cached)
{
  cached->timestamp = cached->cache->timestamp;
  gsk_gpu_cached_set_stale (cached, FALSE);
}

/* }}} */
/* {{{ CachedAtlas */

struct _GskGpuCachedAtlas
{
  GskGpuCached parent;

  GskGpuImage *image;

  gsize remaining_pixels;
  gsize n_slices;
  struct {
    gsize width;
    gsize height;
  } slices[MAX_SLICES_PER_ATLAS];
};

static void
gsk_gpu_cached_atlas_free (GskGpuCached *cached)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;
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

  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_atlas_should_collect (GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;

  if (cached->cache->current_atlas == self &&
      gsk_gpu_cached_is_old (cached, cache_timeout * ATLAS_TIMEOUT_SCALE, timestamp) &&
      cached->pixels == 0)
    return TRUE;

  return cached->pixels + self->remaining_pixels < MIN_ALIVE_PIXELS;
}

static const GskGpuCachedClass GSK_GPU_CACHED_ATLAS_CLASS =
{
  sizeof (GskGpuCachedAtlas),
  "Atlas",
  gsk_gpu_cached_atlas_free,
  gsk_gpu_cached_atlas_should_collect
};

static GskGpuCachedAtlas *
gsk_gpu_cached_atlas_new (GskGpuCache *cache)
{
  GskGpuCachedAtlas *self;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_ATLAS_CLASS);
  self->image = gsk_gpu_device_create_atlas_image (cache->device, ATLAS_SIZE, ATLAS_SIZE);
  self->remaining_pixels = gsk_gpu_image_get_width (self->image) * gsk_gpu_image_get_height (self->image);

  return self;
}

/* This rounds up to the next number that has <= 2 bits set:
 * 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, ...
 * That is roughly sqrt(2), so it should limit waste
 */
static gsize
round_up_atlas_size (gsize num)
{
  gsize storage = g_bit_storage (num);

  num = num + (((1 << storage) - 1) >> 2);
  num &= (((gsize) 7) << storage) >> 2;

  return num;
}

static gboolean
gsk_gpu_cached_atlas_allocate (GskGpuCachedAtlas *atlas,
                               gsize              width,
                               gsize              height,
                               gsize             *out_x,
                               gsize             *out_y)
{
  gsize i;
  gsize waste, slice_waste;
  gsize best_slice;
  gsize y, best_y;
  gboolean can_add_slice;

  best_y = 0;
  best_slice = G_MAXSIZE;
  can_add_slice = atlas->n_slices < MAX_SLICES_PER_ATLAS;
  if (can_add_slice)
    waste = height; /* Require less than 100% waste */
  else
    waste = G_MAXSIZE; /* Accept any slice, we can't make better ones */

  for (i = 0, y = 0; i < atlas->n_slices; y += atlas->slices[i].height, i++)
    {
      if (atlas->slices[i].height < height || ATLAS_SIZE - atlas->slices[i].width < width)
        continue;

      slice_waste = atlas->slices[i].height - height;
      if (slice_waste < waste)
        {
          waste = slice_waste;
          best_slice = i;
          best_y = y;
          if (waste == 0)
            break;
        }
    }

  if (best_slice >= i && i == atlas->n_slices)
    {
      gsize slice_height;

      if (!can_add_slice)
        return FALSE;

      slice_height = round_up_atlas_size (MAX (height, 4));
      if (slice_height > ATLAS_SIZE - y)
        return FALSE;

      atlas->n_slices++;
      if (atlas->n_slices == MAX_SLICES_PER_ATLAS)
        slice_height = ATLAS_SIZE - y;

      atlas->slices[i].width = 0;
      atlas->slices[i].height = slice_height;
      best_y = y;
      best_slice = i;
    }

  *out_x = atlas->slices[best_slice].width;
  *out_y = best_y;

  atlas->slices[best_slice].width += width;
  g_assert (atlas->slices[best_slice].width <= ATLAS_SIZE);

  atlas->remaining_pixels -= width * height;
  ((GskGpuCached *) atlas)->pixels += width * height;

  return TRUE;
}

static void
gsk_gpu_cache_ensure_atlas (GskGpuCache *self,
                            gboolean     recreate)
{
  if (self->current_atlas && !recreate)
    return;

  if (self->current_atlas)
    self->current_atlas->remaining_pixels = 0;

  self->current_atlas = gsk_gpu_cached_atlas_new (self);
}

GskGpuImage *
gsk_gpu_cache_get_atlas_image (GskGpuCache *self)
{
  gsk_gpu_cache_ensure_atlas (self, FALSE);

  return self->current_atlas->image;
}

GskGpuImage *
gsk_gpu_cache_add_atlas_image (GskGpuCache      *self,
                               gsize             width,
                               gsize             height,
                               gsize            *out_x,
                               gsize            *out_y)
{
  if (width > MAX_ATLAS_ITEM_SIZE || height > MAX_ATLAS_ITEM_SIZE)
    return NULL;

  gsk_gpu_cache_ensure_atlas (self, FALSE);

  if (gsk_gpu_cached_atlas_allocate (self->current_atlas, width, height, out_x, out_y))
    {
      gsk_gpu_cached_use ((GskGpuCached *) self->current_atlas);
      return self->current_atlas->image;
    }

  gsk_gpu_cache_ensure_atlas (self, TRUE);

  if (gsk_gpu_cached_atlas_allocate (self->current_atlas, width, height, out_x, out_y))
    {
      gsk_gpu_cached_use ((GskGpuCached *) self->current_atlas);
      return self->current_atlas->image;
    }

  return NULL;
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
gsk_gpu_cached_texture_free (GskGpuCached *cached)
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
  gsk_gpu_cached_texture_free,
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
gsk_gpu_cached_tile_free (GskGpuCached *cached)
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
  gsk_gpu_cached_tile_free,
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
  guint n_stale;
} GskGpuCacheData;

static void
print_cache_stats (GskGpuCache *self)
{
  GskGpuCached *cached;
  GString *message;
  GString *ratios = g_string_new ("");
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
      if (cached->stale)
        cache_data->n_stale++;

      if (cached->class == &GSK_GPU_CACHED_ATLAS_CLASS)
        {
          double ratio;

          ratio = (double) cached->pixels / (double) (ATLAS_SIZE * ATLAS_SIZE);

          if (ratios->len == 0)
            g_string_append (ratios, " (ratios ");
          else
            g_string_append (ratios, ", ");
          g_string_append_printf (ratios, "%.2f", ratio);
        }
    }

  if (ratios->len > 0)
    g_string_append (ratios, ")");

  message = g_string_new ("Cached items");
  g_hash_table_iter_init (&iter, classes);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const GskGpuCachedClass *class = key;
      const GskGpuCacheData *cache_data = value;

      g_string_append_printf (message, "\n  %s:%*s%5u (%u stale)", class->name, 12 - MIN (12, (int) strlen (class->name)), "", cache_data->n_items, cache_data->n_stale);

      if (class == &GSK_GPU_CACHED_ATLAS_CLASS)
        g_string_append_printf (message, "%s", ratios->str);
      else if (class == &GSK_GPU_CACHED_TEXTURE_CLASS)
        g_string_append_printf (message, " (%u in hash)", g_hash_table_size (self->texture_cache));
    }

  gdk_debug_message ("%s", message->str);
  g_string_free (message, TRUE);
  g_hash_table_unref (classes);
  g_string_free (ratios, TRUE);
}

/* Returns TRUE if everything was GC'ed */
gboolean
gsk_gpu_cache_gc (GskGpuCache *self,
                  gint64       cache_timeout,
                  gint64       timestamp)
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
      if (gsk_gpu_cached_should_collect (cached, cache_timeout, timestamp))
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

#ifdef GDK_RENDERING_VULKAN
  gsk_vulkan_ycbcr_finish_cache (self);
#endif
  gsk_gpu_cached_glyph_finish_cache (self);

  g_clear_pointer (&self->tile_cache, g_hash_table_unref);
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
  
  gsk_gpu_cached_glyph_init_cache (self);
#ifdef GDK_RENDERING_VULKAN
  gsk_vulkan_ycbcr_init_cache (self);
#endif
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

/* }}} */
/* vim:set foldmethod=marker: */
