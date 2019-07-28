#include "config.h"

#include "gskvulkanglyphcacheprivate.h"

#include "gskvulkanimageprivate.h"
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


typedef struct {
  GskVulkanImage *image;
  int width, height;
  int x, y, y0;
  int num_glyphs;
  GList *dirty_glyphs;
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
static void     dirty_glyph_free       (gpointer      v);

static Atlas *
create_atlas (GskVulkanGlyphCache *cache)
{
  Atlas *atlas;

  atlas = g_new0 (Atlas, 1);
  atlas->width = 512;
  atlas->height = 512;
  atlas->y0 = 1;
  atlas->y = 1;
  atlas->x = 1;
  atlas->image = NULL;
  atlas->num_glyphs = 0;
  atlas->dirty_glyphs = NULL;

  return atlas;
}

static void
free_atlas (gpointer v)
{
  Atlas *atlas = v;

  g_clear_object (&atlas->image);
  g_list_free_full (atlas->dirty_glyphs, dirty_glyph_free);
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

typedef struct {
  GlyphCacheKey *key;
  GskVulkanCachedGlyph *value;
  cairo_surface_t *surface;
} DirtyGlyph;

static void
dirty_glyph_free (gpointer v)
{
  DirtyGlyph *glyph = v;

  if (glyph->surface)
    cairo_surface_destroy (glyph->surface);
  g_free (glyph);
}

static void
add_to_cache (GskVulkanGlyphCache  *cache,
              GlyphCacheKey        *key,
              GskVulkanCachedGlyph *value)
{
  Atlas *atlas;
  int i;
  DirtyGlyph *dirty;
  int width = value->draw_width * key->scale / 1024;
  int height = value->draw_height * key->scale / 1024;

  for (i = 0; i < cache->atlases->len; i++)
    {
      int x, y, y0;

      atlas = g_ptr_array_index (cache->atlases, i);
      x = atlas->x;
      y = atlas->y;
      y0 = atlas->y0;

      if (atlas->x + width + 1 >= atlas->width)
        {
          /* start a new row */
          y0 = y + 1;
          x = 1;
        }

      if (y0 + height + 1 >= atlas->height)
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

  value->tx = (float)atlas->x / atlas->width;
  value->ty = (float)atlas->y0 / atlas->height;
  value->tw = (float)width / atlas->width;
  value->th = (float)height / atlas->height;

  value->texture_index = i;

  dirty = g_new (DirtyGlyph, 1);
  dirty->key = key;
  dirty->value = value;
  atlas->dirty_glyphs = g_list_prepend (atlas->dirty_glyphs, dirty);

  atlas->x = atlas->x + width + 1;
  atlas->y = MAX (atlas->y, atlas->y0 + height + 1);

  atlas->num_glyphs++;

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (cache->renderer, GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tAtlas %d (%dx%d): %d glyphs (%d dirty), %.2g%% old pixels, filled to %d, %d / %d\n",
                   i, atlas->width, atlas->height,
                   atlas->num_glyphs, g_list_length (atlas->dirty_glyphs),
                   100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height),
                   atlas->x, atlas->y0, atlas->y);
        }
    }
#endif
}

static void
render_glyph (Atlas          *atlas,
              DirtyGlyph     *glyph,
              GskImageRegion *region)
{
  GlyphCacheKey *key = glyph->key;
  GskVulkanCachedGlyph *value = glyph->value;
  cairo_surface_t *surface;
  cairo_t *cr;
  PangoGlyphString glyphs;
  PangoGlyphInfo gi;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        value->draw_width * key->scale / 1024,
                                        value->draw_height * key->scale / 1024);
  cairo_surface_set_device_scale (surface, key->scale / 1024.0, key->scale / 1024.0);

  cr = cairo_create (surface);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  gi.glyph = key->glyph;
  gi.geometry.width = value->draw_width * 1024;
  if (key->glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    gi.geometry.x_offset = key->xshift * 256;
  else
    gi.geometry.x_offset = key->xshift * 256 - value->draw_x * 1024;
  gi.geometry.y_offset = key->yshift * 256 - value->draw_y * 1024;

  glyphs.num_glyphs = 1;
  glyphs.glyphs = &gi;

  pango_cairo_show_glyph_string (cr, key->font, &glyphs);

  cairo_destroy (cr);

  glyph->surface = surface;

  region->data = cairo_image_surface_get_data (surface);
  region->width = cairo_image_surface_get_width (surface);
  region->height = cairo_image_surface_get_height (surface);
  region->stride = cairo_image_surface_get_stride (surface);
  region->x = (gsize)(value->tx * atlas->width);
  region->y = (gsize)(value->ty * atlas->height);
}

static void
upload_dirty_glyphs (GskVulkanGlyphCache *cache,
                     Atlas               *atlas,
                     GskVulkanUploader   *uploader)
{
  GList *l;
  guint num_regions;
  GskImageRegion *regions;
  int i;

  num_regions = g_list_length (atlas->dirty_glyphs);
  regions = alloca (sizeof (GskImageRegion) * num_regions);

  for (l = atlas->dirty_glyphs, i = 0; l; l = l->next, i++)
    render_glyph (atlas, (DirtyGlyph *)l->data, &regions[i]);

  GSK_RENDERER_NOTE (cache->renderer, GLYPH_CACHE,
            g_message ("uploading %d glyphs to cache", num_regions));

  gsk_vulkan_image_upload_regions (atlas->image, uploader, num_regions, regions);

  g_list_free_full (atlas->dirty_glyphs, dirty_glyph_free);
  atlas->dirty_glyphs = NULL;
}

GskVulkanGlyphCache *
gsk_vulkan_glyph_cache_new (GskRenderer      *renderer,
                            GdkVulkanContext *vulkan)
{
  GskVulkanGlyphCache *cache;

  cache = GSK_VULKAN_GLYPH_CACHE (g_object_new (GSK_TYPE_VULKAN_GLYPH_CACHE, NULL));
  cache->renderer = renderer;
  cache->vulkan = vulkan;
  g_ptr_array_add (cache->atlases, create_atlas (cache));

  return cache;
}

#define PHASE(x) ((x % PANGO_SCALE) * 4 / PANGO_SCALE)

GskVulkanCachedGlyph *
gsk_vulkan_glyph_cache_lookup (GskVulkanGlyphCache *cache,
                               gboolean             create,
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

  if (create && value == NULL)
    {
      GlyphCacheKey *key;
      PangoRectangle ink_rect;

      key = g_new (GlyphCacheKey, 1);
      value = g_new0 (GskVulkanCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

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
        add_to_cache (cache, key, value);

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

GskVulkanImage *
gsk_vulkan_glyph_cache_get_glyph_image (GskVulkanGlyphCache *cache,
                                        GskVulkanUploader   *uploader,
                                        guint                index)
{
  Atlas *atlas;

  g_return_val_if_fail (index < cache->atlases->len, NULL);

  atlas = g_ptr_array_index (cache->atlases, index);

  if (atlas->image == NULL)
    atlas->image = gsk_vulkan_image_new_for_atlas (cache->vulkan, atlas->width, atlas->height);

  if (atlas->dirty_glyphs)
    upload_dirty_glyphs (cache, atlas, uploader);

  return atlas->image;
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
  guint dropped = 0;

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
          GSK_RENDERER_NOTE(cache->renderer, GLYPH_CACHE,
                   g_message ("Dropping atlas %d (%g.2%% old)", i, 100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height)));
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

  GSK_RENDERER_NOTE(cache->renderer, GLYPH_CACHE, g_message ("Dropped %d glyphs", dropped));
}
