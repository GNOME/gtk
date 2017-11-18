#include "config.h"

#include "gskglglyphcacheprivate.h"
#include "gskgldriverprivate.h"
#include "gskdebugprivate.h"
#include "gskprivate.h"

#include <graphene.h>
#include <cairo/cairo.h>
#include <epoxy/gl.h>

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

#define ATLAS_SIZE 512

typedef struct
{
  PangoFont *font;
  PangoGlyph glyph;
  guint scale; /* times 1024 */
} GlyphCacheKey;

typedef struct
{
  GlyphCacheKey *key;
  GskGLCachedGlyph *value;
  cairo_surface_t *surface;
} DirtyGlyph;


static guint    glyph_cache_hash       (gconstpointer v);
static gboolean glyph_cache_equal      (gconstpointer v1,
                                        gconstpointer v2);
static void     glyph_cache_key_free   (gpointer      v);
static void     glyph_cache_value_free (gpointer      v);
static void     dirty_glyph_free       (gpointer      v);

static GskGLGlyphAtlas *
create_atlas (GskGLGlyphCache *cache)
{
  GskGLGlyphAtlas *atlas;

  atlas = g_new0 (GskGLGlyphAtlas, 1);
  atlas->width = ATLAS_SIZE;
  atlas->height = ATLAS_SIZE;
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
  GskGLGlyphAtlas *atlas = v;

  if (atlas->image)
    {
      g_assert (atlas->image->texture_id == 0);
      g_free (atlas->image);
    }
  g_list_free_full (atlas->dirty_glyphs, dirty_glyph_free);
  g_free (atlas);
}

void
gsk_gl_glyph_cache_init (GskGLGlyphCache *self,
                         GskGLDriver     *gl_driver)
{
  self->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                            glyph_cache_key_free, glyph_cache_value_free);
  self->atlases = g_ptr_array_new_with_free_func (free_atlas);
  g_ptr_array_add (self->atlases, create_atlas (self));

  self->gl_driver = gl_driver;
}

void
gsk_gl_glyph_cache_free (GskGLGlyphCache *self)
{
  guint i;

  for (i = 0; i < self->atlases->len; i ++)
    {
      GskGLGlyphAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (atlas->image)
        {
          gsk_gl_image_destroy (atlas->image, self->gl_driver);
          atlas->image->texture_id = 0;
        }
    }

  g_ptr_array_unref (self->atlases);
  g_hash_table_unref (self->hash_table);
}

static gboolean
glyph_cache_equal (gconstpointer v1, gconstpointer v2)
{
  const GlyphCacheKey *key1 = v1;
  const GlyphCacheKey *key2 = v2;

  return key1->font == key2->font &&
         key1->glyph == key2->glyph &&
         key1->scale == key2->scale;
}

static guint
glyph_cache_hash (gconstpointer v)
{
  const GlyphCacheKey *key = v;

  return GPOINTER_TO_UINT (key->font) ^ key->glyph ^ key->scale;
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
dirty_glyph_free (gpointer v)
{
  DirtyGlyph *glyph = v;

  if (glyph->surface)
    cairo_surface_destroy (glyph->surface);
  g_free (glyph);
}

static void
add_to_cache (GskGLGlyphCache  *cache,
              GlyphCacheKey        *key,
              GskGLCachedGlyph *value)
{
  GskGLGlyphAtlas *atlas;
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

  value->atlas = atlas;

  dirty = g_new0 (DirtyGlyph, 1);
  dirty->key = key;
  dirty->value = value;
  atlas->dirty_glyphs = g_list_prepend (atlas->dirty_glyphs, dirty);

  atlas->x = atlas->x + width + 1;
  atlas->y = MAX (atlas->y, atlas->y0 + height + 1);

  atlas->num_glyphs++;

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK(GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tGskGLGlyphAtlas %d (%dx%d): %d glyphs (%d dirty), %.2g%% old pixels, filled to %d, %d / %d\n",
                   i, atlas->width, atlas->height,
                   atlas->num_glyphs, g_list_length (atlas->dirty_glyphs),
                   100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height),
                   atlas->x, atlas->y0, atlas->y);
        }
    }
#endif
}

static void
render_glyph (const GskGLGlyphAtlas *atlas,
              DirtyGlyph            *glyph,
              GskImageRegion        *region)
{
  GlyphCacheKey *key = glyph->key;
  GskGLCachedGlyph *value = glyph->value;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cg;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        value->draw_width * key->scale / 1024,
                                        value->draw_height * key->scale / 1024);
  cairo_surface_set_device_scale (surface, key->scale / 1024.0, key->scale / 1024.0);

  cr = cairo_create (surface);

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cg.index = key->glyph;
  cg.x = - value->draw_x;
  cg.y = - value->draw_y;

  cairo_show_glyphs (cr, &cg, 1);

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
upload_dirty_glyphs (GskGLGlyphCache *self,
                     GskGLGlyphAtlas *atlas)
{
  GList *l;
  guint num_regions;
  GskImageRegion *regions;
  int i;

  num_regions = g_list_length (atlas->dirty_glyphs);
  regions = alloca (sizeof (GskImageRegion) * num_regions);

  for (l = atlas->dirty_glyphs, i = 0; l; l = l->next, i++)
    render_glyph (atlas, (DirtyGlyph *)l->data, &regions[i]);

  GSK_NOTE (GLYPH_CACHE,
            g_print ("uploading %d glyphs to cache\n", num_regions));


  gsk_gl_image_upload_regions (atlas->image, self->gl_driver, num_regions, regions);

  g_list_free_full (atlas->dirty_glyphs, dirty_glyph_free);
  atlas->dirty_glyphs = NULL;
}

const GskGLCachedGlyph *
gsk_gl_glyph_cache_lookup (GskGLGlyphCache *cache,
                           gboolean         create,
                           PangoFont       *font,
                           PangoGlyph       glyph,
                           float            scale)
{
  GlyphCacheKey lookup_key;
  GskGLCachedGlyph *value;

  lookup_key.font = font;
  lookup_key.glyph = glyph;
  lookup_key.scale = (guint)(scale * 1024);

  value = g_hash_table_lookup (cache->hash_table, &lookup_key);

  if (value)
    {
      if (cache->timestamp - value->timestamp >= MAX_AGE)
        {
          GskGLGlyphAtlas *atlas = value->atlas;

          if (atlas)
            atlas->old_pixels -= value->draw_width * value->draw_height;

          value->timestamp = cache->timestamp;
        }
    }

  if (create && value == NULL)
    {
      GlyphCacheKey *key;
      PangoRectangle ink_rect;

      key = g_new0 (GlyphCacheKey, 1);
      value = g_new0 (GskGLCachedGlyph, 1);

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;
      value->timestamp = cache->timestamp;
      value->atlas = NULL; /* For now */

      key->font = g_object_ref (font);
      key->glyph = glyph;
      key->scale = (guint)(scale * 1024);

      if (ink_rect.width > 0 && ink_rect.height > 0)
        add_to_cache (cache, key, value);

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

GskGLImage *
gsk_gl_glyph_cache_get_glyph_image (GskGLGlyphCache *self,
                                    const GskGLCachedGlyph *glyph)
{
  GskGLGlyphAtlas *atlas = glyph->atlas;

  g_assert (atlas != NULL);


  if (atlas->image == NULL)
    {
      atlas->image = g_new0 (GskGLImage, 1);
      gsk_gl_image_create (atlas->image, self->gl_driver, atlas->width, atlas->height);
    }

  if (atlas->dirty_glyphs)
    upload_dirty_glyphs (self, atlas);

  return atlas->image;
}

void
gsk_gl_glyph_cache_begin_frame (GskGLGlyphCache *self)
{
  int i;
  GHashTableIter iter;
  GlyphCacheKey *key;
  GskGLCachedGlyph *value;
  guint dropped = 0;

  self->timestamp++;


  if (self->timestamp % CHECK_INTERVAL != 0)
    return;

  /* look for glyphs that have grown old since last time */
  g_hash_table_iter_init (&iter, self->hash_table);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      guint age;

      age = self->timestamp - value->timestamp;
      if (MAX_AGE <= age && age < MAX_AGE + CHECK_INTERVAL)
        {
          GskGLGlyphAtlas *atlas = value->atlas;

          if (atlas)
            atlas->old_pixels += value->draw_width * value->draw_height;
        }
    }

  /* look for atlases to drop, and create a mapping of updated texture indices */
  for (i = self->atlases->len - 1; i >= 0; i--)
    {
      GskGLGlyphAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (atlas->old_pixels > MAX_OLD * atlas->width * atlas->height)
        {
          GSK_NOTE(GLYPH_CACHE,
                   g_print ("Dropping atlas %d (%g.2%% old)\n",
                            i, 100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height)));

          if (atlas->image)
            {
              gsk_gl_image_destroy (atlas->image, self->gl_driver);
              atlas->image->texture_id = 0;
            }

          /* Remove all glyphs that point to this atlas */
          g_hash_table_iter_init (&iter, self->hash_table);
          while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
            {
              if (value->atlas == atlas)
                g_hash_table_iter_remove (&iter);
            }
          /* TODO: The above loop inside this other loop could be slow... */

          g_ptr_array_remove_index (self->atlases, i);
        }
    }

  GSK_NOTE(GLYPH_CACHE, g_print ("Dropped %d glyphs\n", dropped));
}
