#include "config.h"

#include "gskvulkanglyphcacheprivate.h"

#include "gskvulkanimageprivate.h"
#include "gskdebugprivate.h"
#include "gskprivate.h"

#include <graphene.h>

struct _GskVulkanGlyphCache {
  GObject parent_instance;

  GHashTable *hash_table;

  cairo_surface_t *surface;
  int width, height;
  int x, y, y0;

  GskVulkanImage *image;
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

static void
gsk_vulkan_glyph_cache_init (GskVulkanGlyphCache *cache)
{
  cache->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                             glyph_cache_key_free, glyph_cache_value_free);
  cache->width = 1024;
  cache->height = 1024;
  cache->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, cache->width, cache->height);
  cache->y0 = 1;
  cache->y = 1;
  cache->x = 1;
}

static void
gsk_vulkan_glyph_cache_finalize (GObject *object)
{
  GskVulkanGlyphCache *cache = GSK_VULKAN_GLYPH_CACHE (object);

  g_hash_table_unref (cache->hash_table);
  cairo_surface_destroy (cache->surface);
  g_clear_object (&cache->image);

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
} GlyphCacheKey;

static gboolean
glyph_cache_equal (gconstpointer v1, gconstpointer v2)
{
  const GlyphCacheKey *key1 = v1;
  const GlyphCacheKey *key2 = v2;

  return key1->font == key2->font &&
         key1->glyph == key2->glyph;
}

static guint
glyph_cache_hash (gconstpointer v)
{
  const GlyphCacheKey *key = v;

  return GPOINTER_TO_UINT (key->font) ^ key->glyph;
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
              PangoFont            *font,
              PangoGlyph            glyph,
              GskVulkanCachedGlyph *value)
{
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cg;

  if (cache->x + value->draw_width + 1 >= cache->width)
    {
      /* start a new row */
      cache->y0 = cache->y + 1;
      cache->x = 1;
    }

  if (cache->y0 + value->draw_height + 1 >= cache->height)
    {
      g_critical ("Drats! Out of cache space. We should really handle this");
      return;
    }

  cr = cairo_create (cache->surface);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cg.index = glyph;
  cg.x = cache->x - value->draw_x;
  cg.y = cache->y0 - value->draw_y;

  cairo_show_glyphs (cr, &cg, 1);

  cairo_destroy (cr);

  cache->x = cache->x + value->draw_width + 1;
  cache->y = MAX (cache->y, cache->y0 + value->draw_height + 1);

  value->tx = (cg.x + value->draw_x) / cache->width;
  value->ty = (cg.y + value->draw_y) / cache->height;
  value->tw = (float)value->draw_width / cache->width;
  value->th = (float)value->draw_height / cache->height;

  value->texture_index = 0;
}

#if 0
static void
dump_glyph_cache_stats (GskVulkanGlyphCache *cache)
{
  static gint64 time;
  gint64 now;

  if (!cache->hash_table)
    return;

  now = g_get_monotonic_time ();
  if (now - time < 1000000)
    return;

  time = now;

  cairo_surface_write_to_png (cache->surface, "gsk-glyph-cache.png");
}
#endif

GskVulkanGlyphCache *
gsk_vulkan_glyph_cache_new (void)
{
  return GSK_VULKAN_GLYPH_CACHE (g_object_new (GSK_TYPE_VULKAN_GLYPH_CACHE, NULL));
}

GskVulkanCachedGlyph *
gsk_vulkan_glyph_cache_lookup (GskVulkanGlyphCache *cache,
                               gboolean             create,
                               PangoFont           *font,
                               PangoGlyph           glyph)
{
  GlyphCacheKey lookup_key;
  GskVulkanCachedGlyph *value;

  lookup_key.font = font;
  lookup_key.glyph = glyph;

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (create && value == NULL)
    {
      GlyphCacheKey *key;
      PangoRectangle ink_rect;

      value = g_new (GskVulkanCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      if (ink_rect.width > 0 && ink_rect.height > 0)
        {
          add_to_cache (cache, font, glyph, value);
          g_clear_object (&cache->image);
        }

      key = g_new (GlyphCacheKey, 1);
      key->font = g_object_ref (font);
      key->glyph = glyph;

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

GskVulkanImage *
gsk_vulkan_glyph_cache_get_glyph_image (GskVulkanGlyphCache *cache,
                                        GskVulkanUploader   *uploader,
                                        guint                index)
{
  if (cache->image == NULL)
    cache->image = gsk_vulkan_image_new_from_data (uploader,
                                                   cairo_image_surface_get_data (cache->surface),
                                                   cairo_image_surface_get_width (cache->surface),
                                                   cairo_image_surface_get_height (cache->surface),
                                                   cairo_image_surface_get_stride (cache->surface));

  return cache->image;
}
