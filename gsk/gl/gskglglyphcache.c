#include "config.h"

#include "gskglglyphcacheprivate.h"
#include "gskgldriverprivate.h"
#include "gskdebugprivate.h"
#include "gskprivate.h"

#include <graphene.h>
#include <cairo.h>
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
#define NODES_PER_ATLAS 512

static guint    glyph_cache_hash       (gconstpointer v);
static gboolean glyph_cache_equal      (gconstpointer v1,
                                        gconstpointer v2);
static void     glyph_cache_key_free   (gpointer      v);
static void     glyph_cache_value_free (gpointer      v);

static GskGLGlyphAtlas *
create_atlas (GskGLGlyphCache *cache)
{
  GskGLGlyphAtlas *atlas;

  atlas = g_new0 (GskGLGlyphAtlas, 1);
  atlas->width = ATLAS_SIZE;
  atlas->height = ATLAS_SIZE;
  atlas->image = NULL;

  atlas->nodes = g_malloc0 (sizeof (struct stbrp_node) * NODES_PER_ATLAS);
  stbrp_init_target (&atlas->context,
                     ATLAS_SIZE,
                     ATLAS_SIZE,
                     atlas->nodes,
                     NODES_PER_ATLAS);

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

  g_free (atlas->nodes);
  g_free (atlas);
}

void
gsk_gl_glyph_cache_init (GskGLGlyphCache *self,
                         GskRenderer     *renderer,
                         GskGLDriver     *gl_driver)
{
  self->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                            glyph_cache_key_free, glyph_cache_value_free);
  self->atlases = g_ptr_array_new_with_free_func (free_atlas);
  g_ptr_array_add (self->atlases, create_atlas (self));

  self->renderer = renderer;
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
add_to_cache (GskGLGlyphCache  *cache,
              GlyphCacheKey    *key,
              GskGLCachedGlyph *value)
{
  const int width = value->draw_width * key->scale / 1024;
  const int height = value->draw_height * key->scale / 1024;
  GskGLGlyphAtlas *atlas = NULL;
  stbrp_rect glyph_rect;
  guint i, p;

  glyph_rect.w = width;
  glyph_rect.h = height;

  /* Try all the atlases and pick the first one that can hold
   * our new glyph */
  for (i = 0, p = cache->atlases->len; i < p; i ++)
    {
      GskGLGlyphAtlas *test_atlas = g_ptr_array_index (cache->atlases, i);

      stbrp_pack_rects (&test_atlas->context, &glyph_rect, 1);

      if (glyph_rect.was_packed)
        {
          atlas = test_atlas;
          break;
        }
    }

  if (atlas == NULL)
    {
      atlas = create_atlas (cache);
      g_ptr_array_add (cache->atlases, atlas);

      stbrp_pack_rects (&atlas->context, &glyph_rect, 1);
      g_assert (glyph_rect.was_packed);
    }

  value->tx = (float)glyph_rect.x / atlas->width;
  value->ty = (float)glyph_rect.y / atlas->height;
  value->tw = (float)glyph_rect.w / atlas->width;
  value->th = (float)glyph_rect.h / atlas->height;

  value->atlas = atlas;

  atlas->pending_glyph.key = key;
  atlas->pending_glyph.value = value;

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (cache->renderer, GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tGskGLGlyphAtlas %d (%dx%d): %.2g%% old pixels\n",
                   i, atlas->width, atlas->height,
                   100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height));
        }
    }
#endif
}

static void
render_glyph (const GskGLGlyphAtlas *atlas,
              const DirtyGlyph      *glyph,
              GskImageRegion        *region)
{
  GlyphCacheKey *key = glyph->key;
  GskGLCachedGlyph *value = glyph->value;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  PangoGlyphString glyph_string;
  PangoGlyphInfo glyph_info;

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

  glyph_info.glyph = key->glyph;
  glyph_info.geometry.width = value->draw_width * 1024;
  if (key->glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    glyph_info.geometry.x_offset = 0;
  else
    glyph_info.geometry.x_offset = - value->draw_x * 1024;
  glyph_info.geometry.y_offset = - value->draw_y * 1024;

  glyph_string.num_glyphs = 1;
  glyph_string.glyphs = &glyph_info;

  pango_cairo_show_glyph_string (cr, key->font, &glyph_string);
  cairo_destroy (cr);

  cairo_surface_flush (surface);

  region->width = cairo_image_surface_get_width (surface);
  region->height = cairo_image_surface_get_height (surface);
  region->stride = cairo_image_surface_get_stride (surface);
  region->data = g_memdup (cairo_image_surface_get_data (surface),
                           region->stride * region->height * sizeof (guchar));
  region->x = (gsize)(value->tx * atlas->width);
  region->y = (gsize)(value->ty * atlas->height);

  cairo_surface_destroy (surface);
}

static void
upload_dirty_glyph (GskGLGlyphCache *self,
                    GskGLGlyphAtlas *atlas)
{
  GskImageRegion region;

  g_assert (atlas->pending_glyph.key != NULL);

  render_glyph (atlas, &atlas->pending_glyph, &region);

  gsk_gl_image_upload_regions (atlas->image, self->gl_driver, 1, &region);

  g_free (region.data);

  atlas->pending_glyph.key = NULL;
  atlas->pending_glyph.value = NULL;
}

const GskGLCachedGlyph *
gsk_gl_glyph_cache_lookup (GskGLGlyphCache *cache,
                           gboolean         create,
                           PangoFont       *font,
                           PangoGlyph       glyph,
                           float            scale)
{
  GskGLCachedGlyph *value;

  value = g_hash_table_lookup (cache->hash_table,
                               &(GlyphCacheKey) {
                                 .font = font,
                                 .glyph = glyph,
                                 .scale = (guint)(scale * 1024)
                               });

  if (value)
    {
      const guint age = cache->timestamp - value->timestamp;

      if (MAX_AGE <= age && age < MAX_AGE + CHECK_INTERVAL)
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
      value->scale = (guint)(scale * 1024);

      key->font = g_object_ref (font);
      key->glyph = glyph;
      key->scale = (guint)(scale * 1024);

      if (ink_rect.width > 0 && ink_rect.height > 0 && key->scale > 0)
        add_to_cache (cache, key, value);

      g_hash_table_insert (cache->hash_table, key, value);
    }

  return value;
}

GskGLImage *
gsk_gl_glyph_cache_get_glyph_image (GskGLGlyphCache        *self,
                                    const GskGLCachedGlyph *glyph)
{
  GskGLGlyphAtlas *atlas = glyph->atlas;

  g_assert (atlas != NULL);

  if (atlas->image == NULL)
    {
      atlas->image = g_new0 (GskGLImage, 1);
      gsk_gl_image_create (atlas->image, self->gl_driver, atlas->width, atlas->height);
    }

  if (atlas->pending_glyph.key != NULL)
    upload_dirty_glyph (self, atlas);

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


  if ((self->timestamp - 1) % CHECK_INTERVAL != 0)
    return;

  /* look for glyphs that have grown old since last time */
  g_hash_table_iter_init (&iter, self->hash_table);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      const guint age = self->timestamp - value->timestamp;

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
          GSK_RENDERER_NOTE(self->renderer, GLYPH_CACHE,
                   g_message ("Dropping atlas %d (%g.2%% old)",
                            i, 100.0 * (double)atlas->old_pixels / (double)(atlas->width * atlas->height)));

          static int kk;

          g_message ("Dropping cache...");
          gsk_gl_image_write_to_png (atlas->image, self->gl_driver,
                                     g_strdup_printf ("dropped_%d.png", kk++));

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

  GSK_RENDERER_NOTE(self->renderer, GLYPH_CACHE, g_message ("Dropped %d glyphs", dropped));
}
