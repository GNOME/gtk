#include "config.h"

#include "gskgpucacheprivate.h"

#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gdk/gdktextureprivate.h"
#include "gdk/gdkprofilerprivate.h"

#include "gsk/gskdebugprivate.h"
#include "gsk/gskprivate.h"

#define MAX_SLICES_PER_ATLAS 64

#define ATLAS_SIZE 1024

#define MAX_ATLAS_ITEM_SIZE 256

#define MIN_ALIVE_PIXELS (ATLAS_SIZE * ATLAS_SIZE / 2)

#define ATLAS_TIMEOUT_SCALE 4

G_STATIC_ASSERT (MAX_ATLAS_ITEM_SIZE < ATLAS_SIZE);
G_STATIC_ASSERT (MIN_ALIVE_PIXELS < ATLAS_SIZE * ATLAS_SIZE);

typedef struct _GskGpuCached GskGpuCached;
typedef struct _GskGpuCachedClass GskGpuCachedClass;
typedef struct _GskGpuCachedAtlas GskGpuCachedAtlas;
typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;
typedef struct _GskGpuCachedTexture GskGpuCachedTexture;

struct _GskGpuCache
{
  GObject parent_instance;

  GskGpuDevice *device;

  GskGpuCached *first_cached;
  GskGpuCached *last_cached;

  GHashTable *texture_cache;
  GHashTable *glyph_cache;

  GskGpuCachedAtlas *current_atlas;

  /* atomic */ gsize dead_texture_pixels;
};

G_DEFINE_TYPE (GskGpuCache, gsk_gpu_cache, G_TYPE_OBJECT)

/* {{{ Cached base class */

struct _GskGpuCachedClass
{
  gsize size;

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

static inline void
mark_as_stale (GskGpuCached *cached,
               gboolean      stale)
{
  if (cached->stale != stale)
    {
      cached->stale = stale;

      if (cached->atlas)
        {
          if (stale)
            ((GskGpuCached *) cached->atlas)->pixels -= cached->pixels;
          else
            ((GskGpuCached *) cached->atlas)->pixels += cached->pixels;
        }
    }
}

static void
gsk_gpu_cached_free (GskGpuCache  *self,
                     GskGpuCached *cached)
{
  if (cached->next)
    cached->next->prev = cached->prev;
  else
    self->last_cached = cached->prev;
  if (cached->prev)
    cached->prev->next = cached->next;
  else
    self->first_cached = cached->next;

  mark_as_stale (cached, TRUE);

  cached->class->free (self, cached);
}

static gboolean
gsk_gpu_cached_should_collect (GskGpuCache  *cache,
                               GskGpuCached *cached,
                               gint64        cache_timeout,
                               gint64        timestamp)
{
  return cached->class->should_collect (cache, cached, cache_timeout, timestamp);
}

static gpointer
gsk_gpu_cached_new (GskGpuCache             *cache,
                    const GskGpuCachedClass *class,
                    GskGpuCachedAtlas       *atlas)
{
  GskGpuCached *cached;

  cached = g_malloc0 (class->size);

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

static void
gsk_gpu_cached_use (GskGpuCache  *self,
                    GskGpuCached *cached,
                    gint64        timestamp)
{
  cached->timestamp = timestamp;
  mark_as_stale (cached, FALSE);
}

static inline gboolean
gsk_gpu_cached_is_old (GskGpuCache  *self,
                       GskGpuCached *cached,
                       gint64        cache_timeout,
                       gint64        timestamp)
{
  if (cache_timeout < 0)
    return -1;
  else
    return timestamp - cached->timestamp > cache_timeout;
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
gsk_gpu_cached_atlas_free (GskGpuCache  *cache,
                           GskGpuCached *cached)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;
  GskGpuCached *c, *next;

  /* Free all remaining glyphs on this atlas */
  for (c = cache->first_cached; c != NULL; c = next)
    {
      next = c->next;
      if (c->atlas == self)
        gsk_gpu_cached_free (cache, c);
    }

  if (cache->current_atlas == self)
    cache->current_atlas = NULL;

  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_atlas_should_collect (GskGpuCache  *cache,
                                     GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  GskGpuCachedAtlas *self = (GskGpuCachedAtlas *) cached;

  if (cache->current_atlas == self &&
      gsk_gpu_cached_is_old (cache, cached, cache_timeout * ATLAS_TIMEOUT_SCALE, timestamp) &&
      cached->pixels == 0)
    return TRUE;

  return cached->pixels + self->remaining_pixels < MIN_ALIVE_PIXELS;
}

static const GskGpuCachedClass GSK_GPU_CACHED_ATLAS_CLASS =
{
  sizeof (GskGpuCachedAtlas),
  gsk_gpu_cached_atlas_free,
  gsk_gpu_cached_atlas_should_collect
};

static GskGpuCachedAtlas *
gsk_gpu_cached_atlas_new (GskGpuCache *cache)
{
  GskGpuCachedAtlas *self;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_ATLAS_CLASS, NULL);
  self->image = gsk_gpu_device_create_atlas_image (cache->device, ATLAS_SIZE, ATLAS_SIZE);
  self->remaining_pixels = gsk_gpu_image_get_width (self->image) * gsk_gpu_image_get_height (self->image);

  return self;
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

  gsize *dead_pixels_counter;

  GdkTexture *texture;
  GskGpuImage *image;
};

static void
gsk_gpu_cached_texture_free (GskGpuCache  *cache,
                             GskGpuCached *cached)
{
  GskGpuCachedTexture *self = (GskGpuCachedTexture *) cached;
  gpointer key, value;

  g_clear_object (&self->image);

  if (g_hash_table_steal_extended (cache->texture_cache, self->texture, &key, &value))
    {
      /* If the texture has been reused already, we put the entry back */
      if ((GskGpuCached *) value != cached)
        g_hash_table_insert (cache->texture_cache, key, value);
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
gsk_gpu_cached_texture_should_collect (GskGpuCache *cache,
                                       GskGpuCached *cached,
                                       gint64        cache_timeout,
                                       gint64        timestamp)
{
  GskGpuCachedTexture *self = (GskGpuCachedTexture *) cached;

  return gsk_gpu_cached_is_old (cache, cached, cache_timeout, timestamp) ||
         gsk_gpu_cached_texture_is_invalid (self);
}

static const GskGpuCachedClass GSK_GPU_CACHED_TEXTURE_CLASS =
{
  sizeof (GskGpuCachedTexture),
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
    g_atomic_pointer_add (self->dead_pixels_counter, ((GskGpuCached *) self)->pixels);

  if (g_atomic_int_dec_and_test (&self->use_count))
    g_free (self);
}

static GskGpuCachedTexture *
gsk_gpu_cached_texture_new (GskGpuCache *cache,
                            GdkTexture   *texture,
                            GskGpuImage  *image)
{
  GskGpuCachedTexture *self;

  if (gdk_texture_get_render_data (texture, cache))
    gdk_texture_clear_render_data (texture);
  else if ((self = g_hash_table_lookup (cache->texture_cache, texture)))
    g_hash_table_remove (cache->texture_cache, texture);

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_TEXTURE_CLASS, NULL);
  self->texture = texture;
  self->image = g_object_ref (image);
  ((GskGpuCached *)self)->pixels = gsk_gpu_image_get_width (image) * gsk_gpu_image_get_height (image);
  self->dead_pixels_counter = &cache->dead_texture_pixels;
  self->use_count = 2;

  if (!gdk_texture_set_render_data (texture, cache, self, gsk_gpu_cached_texture_destroy_cb))
    {
      g_object_weak_ref (G_OBJECT (texture), (GWeakNotify) gsk_gpu_cached_texture_destroy_cb, self);

      g_hash_table_insert (cache->texture_cache, texture, self);
    }

  return self;
}

/* }}} */
/* {{{ CachedGlyph */

struct _GskGpuCachedGlyph
{
  GskGpuCached parent;

  PangoFont *font;
  PangoGlyph glyph;
  GskGpuGlyphLookupFlags flags;
  float scale;

  GskGpuImage *image;
  graphene_rect_t bounds;
  graphene_point_t origin;
};

static void
gsk_gpu_cached_glyph_free (GskGpuCache  *cache,
                           GskGpuCached *cached)
{
  GskGpuCachedGlyph *self = (GskGpuCachedGlyph *) cached;

  g_hash_table_remove (cache->glyph_cache, self);

  g_object_unref (self->font);
  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_glyph_should_collect (GskGpuCache  *cache,
                                     GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  if (gsk_gpu_cached_is_old (cache, cached, cache_timeout, timestamp))
    {
      if (cached->atlas)
        mark_as_stale (cached, TRUE);
      else
        return TRUE;
    }

  /* Glyphs are only collected when their atlas is freed */
  return FALSE;
}

static guint
gsk_gpu_cached_glyph_hash (gconstpointer data)
{
  const GskGpuCachedGlyph *glyph = data;

  return GPOINTER_TO_UINT (glyph->font) ^
         glyph->glyph ^
         (glyph->flags << 24) ^
         ((guint) glyph->scale * PANGO_SCALE);
}

static gboolean
gsk_gpu_cached_glyph_equal (gconstpointer v1,
                            gconstpointer v2)
{
  const GskGpuCachedGlyph *glyph1 = v1;
  const GskGpuCachedGlyph *glyph2 = v2;

  return glyph1->font == glyph2->font
      && glyph1->glyph == glyph2->glyph
      && glyph1->flags == glyph2->flags
      && glyph1->scale == glyph2->scale;
}

static const GskGpuCachedClass GSK_GPU_CACHED_GLYPH_CLASS =
{
  sizeof (GskGpuCachedGlyph),
  gsk_gpu_cached_glyph_free,
  gsk_gpu_cached_glyph_should_collect
};

/* }}} */
/* {{{ GskGpuCache */

static void
print_cache_stats (GskGpuCache *self)
{
  GskGpuCached *cached;
  guint glyphs = 0;
  guint stale_glyphs = 0;
  guint textures = 0;
  guint atlases = 0;
  GString *ratios = g_string_new ("");

  for (cached = self->first_cached; cached != NULL; cached = cached->next)
    {
      if (cached->class == &GSK_GPU_CACHED_GLYPH_CLASS)
        {
          glyphs++;
          if (cached->stale)
            stale_glyphs++;
        }
      else if (cached->class == &GSK_GPU_CACHED_TEXTURE_CLASS)
        {
          textures++;
        }
      else if (cached->class == &GSK_GPU_CACHED_ATLAS_CLASS)
        {
          double ratio;

          atlases++;

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

  gdk_debug_message ("Cached items\n"
                     "  glyphs:   %5u (%u stale)\n"
                     "  textures: %5u (%u in hash)\n"
                     "  atlases:  %5u%s",
                     glyphs, stale_glyphs,
                     textures, g_hash_table_size (self->texture_cache),
                     atlases, ratios->str);

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
      if (gsk_gpu_cached_should_collect (self, cached, cache_timeout, timestamp))
        gsk_gpu_cached_free (self, cached);
      else
        is_empty &= cached->stale;
    }

  g_atomic_pointer_set (&self->dead_texture_pixels, 0);

  if (GSK_DEBUG_CHECK (CACHE))
    print_cache_stats (self);

  gdk_profiler_end_mark (before, "Glyph cache GC", NULL);

  return is_empty;
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
    gsk_gpu_cached_free (self, self->last_cached);

  g_assert (self->last_cached == NULL);
}

static void
gsk_gpu_cache_dispose (GObject *object)
{
  GskGpuCache *self = GSK_GPU_CACHE (object);

  gsk_gpu_cache_clear_cache (self);
  g_hash_table_unref (self->glyph_cache);
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
  self->glyph_cache = g_hash_table_new (gsk_gpu_cached_glyph_hash,
                                        gsk_gpu_cached_glyph_equal);
  self->texture_cache = g_hash_table_new (g_direct_hash,
                                          g_direct_equal);
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

static GskGpuImage *
gsk_gpu_cache_add_atlas_image (GskGpuCache      *self,
                               gint64            timestamp,
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
      gsk_gpu_cached_use (self, (GskGpuCached *) self->current_atlas, timestamp);
      return self->current_atlas->image;
    }

  gsk_gpu_cache_ensure_atlas (self, TRUE);

  if (gsk_gpu_cached_atlas_allocate (self->current_atlas, width, height, out_x, out_y))
    {
      gsk_gpu_cached_use (self, (GskGpuCached *) self->current_atlas, timestamp);
      return self->current_atlas->image;
    }

  return NULL;
}

GskGpuImage *
gsk_gpu_cache_lookup_texture_image (GskGpuCache *self,
                                    GdkTexture  *texture,
                                    gint64       timestamp)
{
  GskGpuCachedTexture *cache;

  cache = gdk_texture_get_render_data (texture, self);
  if (cache == NULL)
    cache = g_hash_table_lookup (self->texture_cache, texture);

  if (!cache || !cache->image || gsk_gpu_cached_texture_is_invalid (cache))
    return NULL;

  gsk_gpu_cached_use (self, (GskGpuCached *) cache, timestamp);

  return g_object_ref (cache->image);
}

void
gsk_gpu_cache_cache_texture_image (GskGpuCache *self,
                                   GdkTexture  *texture,
                                   gint64       timestamp,
                                   GskGpuImage *image)
{
  GskGpuCachedTexture *cache;

  cache = gsk_gpu_cached_texture_new (self, texture, image);

  gsk_gpu_cached_use (self, (GskGpuCached *) cache, timestamp);
}

GskGpuImage *
gsk_gpu_cache_lookup_glyph_image (GskGpuCache            *self,
                                  GskGpuFrame            *frame,
                                  PangoFont              *font,
                                  PangoGlyph              glyph,
                                  GskGpuGlyphLookupFlags  flags,
                                  float                   scale,
                                  graphene_rect_t        *out_bounds,
                                  graphene_point_t       *out_origin)
{
  GskGpuCachedGlyph lookup = {
    .font = font,
    .glyph = glyph,
    .flags = flags,
    .scale = scale
  };
  GskGpuCachedGlyph *cache;
  PangoRectangle ink_rect;
  graphene_rect_t rect;
  graphene_point_t origin;
  GskGpuImage *image;
  gsize atlas_x, atlas_y, padding;
  float subpixel_x, subpixel_y;
  PangoFont *scaled_font;
  cairo_hint_metrics_t hint_metrics;

  cache = g_hash_table_lookup (self->glyph_cache, &lookup);
  if (cache)
    {
      gsk_gpu_cached_use (self, (GskGpuCached *) cache, gsk_gpu_frame_get_timestamp (frame));

      *out_bounds = cache->bounds;
      *out_origin = cache->origin;
      return cache->image;
    }

  /* The combination of hint-style != none and hint-metrics == off
   * leads to broken rendering with some fonts.
   */
  if (gsk_font_get_hint_style (font) != CAIRO_HINT_STYLE_NONE)
    hint_metrics = CAIRO_HINT_METRICS_ON;
  else
    hint_metrics = CAIRO_HINT_METRICS_DEFAULT;

  scaled_font = gsk_reload_font (font, scale, hint_metrics, CAIRO_HINT_STYLE_DEFAULT, CAIRO_ANTIALIAS_DEFAULT);

  subpixel_x = (flags & 3) / 4.f;
  subpixel_y = ((flags >> 2) & 3) / 4.f;
  pango_font_get_glyph_extents (scaled_font, glyph, &ink_rect, NULL);
  origin.x = floor (ink_rect.x * 1.0 / PANGO_SCALE + subpixel_x);
  origin.y = floor (ink_rect.y * 1.0 / PANGO_SCALE + subpixel_y);
  rect.size.width = ceil ((ink_rect.x + ink_rect.width) * 1.0 / PANGO_SCALE + subpixel_x) - origin.x;
  rect.size.height = ceil ((ink_rect.y + ink_rect.height) * 1.0 / PANGO_SCALE + subpixel_y) - origin.y;
  padding = 1;

  image = gsk_gpu_cache_add_atlas_image (self,
                                         gsk_gpu_frame_get_timestamp (frame),
                                         rect.size.width + 2 * padding, rect.size.height + 2 * padding,
                                         &atlas_x, &atlas_y);
  if (image)
    {
      g_object_ref (image);
      rect.origin.x = atlas_x + padding;
      rect.origin.y = atlas_y + padding;
      cache = gsk_gpu_cached_new (self, &GSK_GPU_CACHED_GLYPH_CLASS, self->current_atlas);
    }
  else
    {
      image = gsk_gpu_device_create_upload_image (self->device, GDK_MEMORY_DEFAULT, FALSE, rect.size.width, rect.size.height),
      rect.origin.x = 0;
      rect.origin.y = 0;
      padding = 0;
      cache = gsk_gpu_cached_new (self, &GSK_GPU_CACHED_GLYPH_CLASS, NULL);
    }

  cache->font = g_object_ref (font);
  cache->glyph = glyph;
  cache->flags = flags;
  cache->scale = scale;
  cache->bounds = rect;
  cache->image = image;
  cache->origin = GRAPHENE_POINT_INIT (- origin.x + subpixel_x,
                                       - origin.y + subpixel_y);
  ((GskGpuCached *) cache)->pixels = (rect.size.width + 2 * padding) * (rect.size.height + 2 * padding);

  gsk_gpu_upload_glyph_op (frame,
                           cache->image,
                           scaled_font,
                           glyph,
                           &(cairo_rectangle_int_t) {
                               .x = rect.origin.x - padding,
                               .y = rect.origin.y - padding,
                               .width = rect.size.width + 2 * padding,
                               .height = rect.size.height + 2 * padding,
                           },
                           &GRAPHENE_POINT_INIT (cache->origin.x + padding,
                                                 cache->origin.y + padding));

  g_hash_table_insert (self->glyph_cache, cache, cache);
  gsk_gpu_cached_use (self, (GskGpuCached *) cache, gsk_gpu_frame_get_timestamp (frame));

  *out_bounds = cache->bounds;
  *out_origin = cache->origin;

  g_object_unref (scaled_font);

  return cache->image;
}

GskGpuCache *
gsk_gpu_cache_new (GskGpuDevice *device)
{
  GskGpuCache *self;

  self = g_object_new (GSK_TYPE_GPU_CACHE, NULL);
  self->device = g_object_ref (device);

  return self;
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
