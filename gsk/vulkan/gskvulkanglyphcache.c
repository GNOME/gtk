#include "config.h"

#include "gskvulkanglyphcacheprivate.h"

#include "gskvulkanimageprivate.h"
#include "gskvulkanuploadopprivate.h"

#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"

#include <graphene.h>

/* Parameters for our cache eviction strategy.
 *
 * Each cached glyph has an age that gets reset every time a cached glyph gets used.
 * Glyphs that have not been used for the MAX_AGE frames are considered old. We keep
 * count of the pixels of each atlas that are taken up by old glyphs. We check the
 * fraction of old pixels every CHECK_INTERVAL frames, and if it is above MAX_OLD, then
 * we drop the atlas an all the glyphs contained in it from the cache.
 */

#define MAX_AGE 60
#define CHECK_INTERVAL 10
#define MAX_OLD 0.333

#define PADDING 1

typedef struct {
  GskVulkanImage *image;
  int width, height;
  int x, y, y0;
  guint old_pixels;
} Atlas;

struct _GskVulkanGlyphCache {
  GObject parent_instance;

  GdkVulkanContext *vulkan;
  GskRenderer *renderer;

  GHashTable *hash_table;
  GPtrArray *atlases;

  guint64 timestamp;
};

struct _GskVulkanGlyphCacheClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GskVulkanGlyphCache, gsk_vulkan_glyph_cache, G_TYPE_OBJECT)

static guint    glyph_cache_hash       (gconstpointer v);
static gboolean glyph_cache_equal      (gconstpointer v1,
                                        gconstpointer v2);
static void     glyph_cache_key_free   (gpointer      v);
static void     glyph_cache_value_free (gpointer      v);

static Atlas *
create_atlas (GskVulkanGlyphCache *cache)
{
  Atlas *atlas;

  atlas = g_new0 (Atlas, 1);
  atlas->width = 512;
  atlas->height = 512;
  atlas->y0 = 0;
  atlas->y = 0;
  atlas->x = 0;
  atlas->image = NULL;

  atlas->image = gsk_vulkan_image_new_for_atlas (cache->vulkan, atlas->width, atlas->height);

  return atlas;
}

static void
free_atlas (gpointer v)
{
  Atlas *atlas = v;

  g_clear_object (&atlas->image);
  g_free (atlas);
}

static void
gsk_vulkan_glyph_cache_init (GskVulkanGlyphCache *cache)
{
  cache->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                             glyph_cache_key_free, glyph_cache_value_free);
  cache->atlases = g_ptr_array_new_with_free_func (free_atlas);
}

static void
gsk_vulkan_glyph_cache_finalize (GObject *object)
{
  GskVulkanGlyphCache *cache = GSK_VULKAN_GLYPH_CACHE (object);

  g_ptr_array_unref (cache->atlases);
  g_hash_table_unref (cache->hash_table);

  G_OBJECT_CLASS (gsk_vulkan_glyph_cache_parent_class)->finalize (object);
}

static void
gsk_vulkan_glyph_cache_class_init (GskVulkanGlyphCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_vulkan_glyph_cache_finalize;
}

typedef struct {
  PangoFont *font;
  PangoGlyph glyph;
  guint xshift;
  guint yshift;
  guint scale; /* times 1024 */
} GlyphCacheKey;

static gboolean
glyph_cache_equal (gconstpointer v1, gconstpointer v2)
{
  const GlyphCacheKey *key1 = v1;
  const GlyphCacheKey *key2 = v2;

  return key1->font == key2->font &&
         key1->glyph == key2->glyph &&
         key1->xshift == key2->xshift &&
         key1->yshift == key2->yshift &&
         key1->scale == key2->scale;
}

static guint
glyph_cache_hash (gconstpointer v)
{
  const GlyphCacheKey *key = v;

  return GPOINTER_TO_UINT (key->font) ^ key->glyph ^ (key->xshift << 24) ^ (key->yshift << 26) ^ key->scale;
}

static void
glyph_cache_key_free (gpointer v)
{
  GlyphCacheKey *f = v;

  g_object_unref (f->font);
  g_free (f);
}

static void
glyph_cache_value_free (gpointer v)
{
  g_free (v);
}

static void
add_to_cache (GskVulkanGlyphCache  *cache,
              GskVulkanRender      *render,
              GlyphCacheKey        *key,
              GskVulkanCachedGlyph *value)
{
  Atlas *atlas;
  int i;
  int width = ceil (value->draw_width * key->scale / 1024.0);
  int height = ceil (value->draw_height * key->scale / 1024.0);
  int width_with_padding = width + 2 * PADDING;
  int height_with_padding = height + 2 * PADDING;

  for (i = 0; i < cache->atlases->len; i++)
    {
      int x, y, y0;

      atlas = g_ptr_array_index (cache->atlases, i);
      x = atlas->x;
      y = atlas->y;
      y0 = atlas->y0;

      if (atlas->x + width_with_padding >= atlas->width)
        {
          /* start a new row */
          y0 = y + PADDING;
          x = PADDING;
        }

      if (y0 + height_with_padding >= atlas->height)
        continue;

      atlas->y0 = y0;
      atlas->x = x;
      atlas->y = y;
      break;
    }

  if (i == cache->atlases->len)
    {
      atlas = create_atlas (cache);
      g_ptr_array_add (cache->atlases, atlas);
    }

  value->atlas_image = atlas->image;
  value->atlas_x = atlas->x;
  value->atlas_y = atlas->y0;

  value->tx = (float)(atlas->x + PADDING) / atlas->width;
  value->ty = (float)(atlas->y0 + PADDING) / atlas->height;
  value->tw = (float)width / atlas->width;
  value->th = (float)height / atlas->height;

  atlas->x = atlas->x + width_with_padding;
  atlas->y = MAX (atlas->y, atlas->y0 + height_with_padding);

  gsk_vulkan_upload_glyph_op (render,
                              atlas->image,
                              &(cairo_rectangle_int_t) {
                                  .x = value->atlas_x,
                                  .y = value->atlas_y,
                                  .width = width_with_padding,
                                  .height = height_with_padding
                              },
                              key->font,
                              &(PangoGlyphInfo) {
                                  .glyph = key->glyph,
                                  .geometry.width = value->draw_width * PANGO_SCALE,
                                  .geometry.x_offset = (0.25 * key->xshift - value->draw_x) * PANGO_SCALE,
                                  .geometry.y_offset = (0.25 * key->yshift - value->draw_y) * PANGO_SCALE
                              },
                              (float) key->scale / PANGO_SCALE);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tAtlas %d (%dx%d): %.2g%% old pixels, filled to %d, %d / %d\n",
                   i, atlas->width, atlas->height,
                   100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height),
                   atlas->x, atlas->y0, atlas->y);
        }
    }
#endif
}

GskVulkanGlyphCache *
gsk_vulkan_glyph_cache_new (GdkVulkanContext *vulkan)
{
  GskVulkanGlyphCache *cache;

  cache = GSK_VULKAN_GLYPH_CACHE (g_object_new (GSK_TYPE_VULKAN_GLYPH_CACHE, NULL));
  cache->vulkan = vulkan;
  g_ptr_array_add (cache->atlases, create_atlas (cache));

  return cache;
}

#define PHASE(x) ((x % PANGO_SCALE) * 4 / PANGO_SCALE)

GskVulkanCachedGlyph *
gsk_vulkan_glyph_cache_lookup (GskVulkanGlyphCache *cache,
                               GskVulkanRender     *render,
                               PangoFont           *font,
                               PangoGlyph           glyph,
                               int                  x,
                               int                  y,
                               float                scale)
{
  GlyphCacheKey lookup_key;
  GskVulkanCachedGlyph *value;
  guint xshift;
  guint yshift;

  xshift = PHASE (x);
  yshift = PHASE (y);

  lookup_key.font = font;
  lookup_key.glyph = glyph;
  lookup_key.xshift = xshift;
  lookup_key.yshift = yshift;
  lookup_key.scale = (guint)(scale * 1024);

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (value)
    {
      if (cache->timestamp - value->timestamp >= MAX_AGE)
        {
          Atlas *atlas = g_ptr_array_index (cache->atlases, value->texture_index);

          atlas->old_pixels -= value->draw_width * value->draw_height;
          value->timestamp = cache->timestamp;
        }
    }

  if (value == NULL)
    {
      GlyphCacheKey *key;
      PangoRectangle ink_rect;

      key = g_new (GlyphCacheKey, 1);
      value = g_new0 (GskVulkanCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      ink_rect.x -= 1;
      ink_rect.y -= 1;
      ink_rect.width += 2;
      ink_rect.height += 2;

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;
      value->timestamp = cache->timestamp;

      key->font = g_object_ref (font);
      key->glyph = glyph;
      key->xshift = xshift;
      key->yshift = yshift;
      key->scale = (guint)(scale * 1024);

      if (ink_rect.width > 0 && ink_rect.height > 0)
        add_to_cache (cache, render, key, value);

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

void
gsk_vulkan_glyph_cache_begin_frame (GskVulkanGlyphCache *cache)
{
  int i, j;
  guint *drops;
  guint *shifts;
  guint len;
  GHashTableIter iter;
  GlyphCacheKey *key;
  GskVulkanCachedGlyph *value;
  G_GNUC_UNUSED guint dropped = 0;

  cache->timestamp++;

  if (cache->timestamp % CHECK_INTERVAL != 0)
    return;

  len = cache->atlases->len;

  /* look for glyphs that have grown old since last time */
  g_hash_table_iter_init (&iter, cache->hash_table);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      guint age;

      age = cache->timestamp - value->timestamp;
      if (MAX_AGE <= age && age < MAX_AGE + CHECK_INTERVAL)
        {
          Atlas *atlas = g_ptr_array_index (cache->atlases, value->texture_index);
          atlas->old_pixels += value->draw_width * value->draw_height;
        }
    }

  drops = g_alloca (sizeof (guint) * len);
  shifts = g_alloca (sizeof (guint) * len);

  for (i = 0; i < len; i++)
    {
      drops[i] = 0;
      shifts[i] = i;
    }

  /* look for atlases to drop, and create a mapping of updated texture indices */
  for (i = cache->atlases->len - 1; i >= 0; i--)
    {
      Atlas *atlas = g_ptr_array_index (cache->atlases, i);

      if (atlas->old_pixels > MAX_OLD * atlas->width * atlas->height)
        {
          GSK_DEBUG (GLYPH_CACHE,
                     "Dropping atlas %d (%g.2%% old)",
                     i, 100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height));
          g_ptr_array_remove_index (cache->atlases, i);

          drops[i] = 1;
          for (j = i; j + 1 < len; j++)
            shifts[j + 1] = shifts[j];
        }
    }

  /* no atlas dropped, we're done */
  if (len == cache->atlases->len)
    return;

  /* purge glyphs and update texture indices */
  g_hash_table_iter_init (&iter, cache->hash_table);

  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      if (drops[value->texture_index])
        {
          dropped++;
          g_hash_table_iter_remove (&iter);
        }
      else
        {
          value->texture_index = shifts[value->texture_index];
        }
    }

  GSK_DEBUG (GLYPH_CACHE, "Dropped %d glyphs", dropped);
}
