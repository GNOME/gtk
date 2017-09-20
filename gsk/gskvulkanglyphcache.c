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
  int num_glyphs;
  GList *dirty_glyphs;
} Atlas;

struct _GskVulkanGlyphCache {
  GObject parent_instance;

  GdkVulkanContext *vulkan;

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
create_atlas (GskVulkanGlyphCache *cache)
{
  Atlas *atlas;

  atlas = g_new (Atlas, 1);
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

  if (atlas->surface)
    cairo_surface_destroy (atlas->surface);
  g_clear_object (&atlas->image);
  g_list_free_full (atlas->dirty_glyphs, g_free);
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

typedef struct {
  GlyphCacheKey *key;
  GskVulkanCachedGlyph *value;
} DirtyGlyph;

static void
add_to_cache (GskVulkanGlyphCache  *cache,
              GlyphCacheKey        *key,
              GskVulkanCachedGlyph *value)
{
  Atlas *atlas;
  int i;
  DirtyGlyph *dirty;

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
      atlas = create_atlas (cache);
      g_ptr_array_add (cache->atlases, atlas);
    }

  value->tx = (float)atlas->x / atlas->width;
  value->ty = (float)atlas->y0 / atlas->height;
  value->tw = (float)value->draw_width / atlas->width;
  value->th = (float)value->draw_height / atlas->height;

  value->texture_index = i;

  dirty = g_new (DirtyGlyph, 1);
  dirty->key = key;
  dirty->value = value;
  atlas->dirty_glyphs = g_list_prepend (atlas->dirty_glyphs, dirty);

  atlas->x = atlas->x + value->draw_width + 1;
  atlas->y = MAX (atlas->y, atlas->y0 + value->draw_height + 1);

  atlas->num_glyphs++;

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK(GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tAtlas %d (%dx%d): %d glyphs (%d dirty), filled to %d, %d / %d\n",
                   i, atlas->width, atlas->height,
                   atlas->num_glyphs, g_list_length (atlas->dirty_glyphs),
                   atlas->x, atlas->y0, atlas->y);
        }
    }
#endif
}

static void
upload_glyph (Atlas                *atlas,
              GskVulkanUploader    *uploader,
              GlyphCacheKey        *key,
              GskVulkanCachedGlyph *value)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cg;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        value->draw_width,
                                        value->draw_height);

  cr = cairo_create (surface);

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cg.index = key->glyph;
  cg.x = - value->draw_x;
  cg.y = - value->draw_y;

  cairo_show_glyphs (cr, &cg, 1);

  cairo_destroy (cr);

  gsk_vulkan_image_upload_region (atlas->image,
                                  uploader,
                                  cairo_image_surface_get_data (surface),
                                  cairo_image_surface_get_width (surface),
                                  cairo_image_surface_get_height (surface),
                                  cairo_image_surface_get_stride (surface),
                                  (gsize)(value->tx * atlas->width),
                                  (gsize)(value->ty * atlas->height));

  cairo_surface_destroy (surface);
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

      key = g_new (GlyphCacheKey, 1);
      value = g_new0 (GskVulkanCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;

      key->font = g_object_ref (font);
      key->glyph = glyph;

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
  GList *l;

  g_return_val_if_fail (index < cache->atlases->len, NULL);

  atlas = g_ptr_array_index (cache->atlases, index);

  if (atlas->image == NULL)
    {
      cairo_surface_t *surface;

      /* FIXME: create the image without uploading data pointlessly */
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, atlas->width, atlas->height);
      atlas->image = gsk_vulkan_image_new_from_data (uploader,
                                                     cairo_image_surface_get_data (surface),
                                                     cairo_image_surface_get_width (surface),
                                                     cairo_image_surface_get_height (surface),
                                                     cairo_image_surface_get_stride (surface));
      cairo_surface_destroy (surface);
    }

  for (l = atlas->dirty_glyphs; l; l = l->next)
    {
      DirtyGlyph *glyph = l->data;
      upload_glyph (atlas, uploader, glyph->key, glyph->value);
    }
  g_list_free_full (atlas->dirty_glyphs, g_free);
  atlas->dirty_glyphs = NULL;

  return atlas->image;
}
