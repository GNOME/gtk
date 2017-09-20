#include "config.h"

#include "gskvulkanglyphcacheprivate.h"

#include "gskvulkanimageprivate.h"
#include "gskdebugprivate.h"
#include "gskprivate.h"

#include <graphene.h>

typedef struct {
  cairo_surface_t *surface;
  GskVulkanImage *image;
  int width, height;
  int x, y, y0;
} Atlas;

struct _GskVulkanGlyphCache {
  GObject parent_instance;

  GHashTable *hash_table;
  GPtrArray *atlases;
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
create_atlas (void)
{
  Atlas *atlas;

  atlas = g_new (Atlas, 1);
  atlas->width = 512;
  atlas->height = 512;
  atlas->y0 = 1;
  atlas->y = 1;
  atlas->x = 1;
  atlas->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, atlas->width, atlas->height);
  atlas->image = NULL;

  return atlas;
}

static void
free_atlas (gpointer v)
{
  Atlas *atlas = v;

  if (atlas->surface)
    cairo_surface_destroy (atlas->surface);
  g_clear_object (&atlas->image);
  g_free (atlas);
}

static void
gsk_vulkan_glyph_cache_init (GskVulkanGlyphCache *cache)
{
  cache->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                             glyph_cache_key_free, glyph_cache_value_free);
  cache->atlases = g_ptr_array_new_with_free_func (free_atlas);
  g_ptr_array_add (cache->atlases, create_atlas ());
}

static void
gsk_vulkan_glyph_cache_finalize (GObject *object)
{
  GskVulkanGlyphCache *cache = GSK_VULKAN_GLYPH_CACHE (object);

  g_hash_table_unref (cache->hash_table);
  g_ptr_array_unref (cache->atlases);

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
  Atlas *atlas;
  int i;

  for (i = 0; i < cache->atlases->len; i++)
    {
      int x, y, y0;

      atlas = g_ptr_array_index (cache->atlases, i);
      x = atlas->x;
      y = atlas->y;
      y0 = atlas->y0;

      if (atlas->x + value->draw_width + 1 >= atlas->width)
        {
          /* start a new row */
          y0 = y + 1;
          x = 1;
        }

      if (y0 + value->draw_height + 1 >= atlas->height)
        continue;

      atlas->y0 = y0;
      atlas->x = x;
      atlas->y = y;
      break;
    }

  if (i == cache->atlases->len)
    {
      atlas = create_atlas ();
      g_ptr_array_add (cache->atlases, atlas);
    }

  cr = cairo_create (atlas->surface);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cg.index = glyph;
  cg.x = atlas->x - value->draw_x;
  cg.y = atlas->y0 - value->draw_y;

  cairo_show_glyphs (cr, &cg, 1);

  cairo_destroy (cr);

  atlas->x = atlas->x + value->draw_width + 1;
  atlas->y = MAX (atlas->y, atlas->y0 + value->draw_height + 1);

  value->tx = (cg.x + value->draw_x) / atlas->width;
  value->ty = (cg.y + value->draw_y) / atlas->height;
  value->tw = (float)value->draw_width / atlas->width;
  value->th = (float)value->draw_height / atlas->height;

  value->texture_index = i;

  g_clear_object (&atlas->image); /* force re-upload */
}

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

      value = g_new0 (GskVulkanCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      if (ink_rect.width > 0 && ink_rect.height > 0)
        add_to_cache (cache, font, glyph, value);

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
  Atlas *atlas;

  g_return_val_if_fail (index < cache->atlases->len, NULL);

  atlas = g_ptr_array_index (cache->atlases, index);

  if (atlas->image == NULL)
    atlas->image = gsk_vulkan_image_new_from_data (uploader,
                                                   cairo_image_surface_get_data (atlas->surface),
                                                   cairo_image_surface_get_width (atlas->surface),
                                                   cairo_image_surface_get_height (atlas->surface),
                                                   cairo_image_surface_get_stride (atlas->surface));

  return atlas->image;
}
