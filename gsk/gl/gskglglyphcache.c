#include "config.h"

#include "gskglglyphcacheprivate.h"
#include "gskgldriverprivate.h"
#include "gskdebugprivate.h"
#include "gskprivate.h"
#include "gskgltextureatlasprivate.h"

#include "gdk/gdkglcontextprivate.h"

#include <graphene.h>
#include <cairo.h>
#include <epoxy/gl.h>

/* Parameters for our cache eviction strategy.
 *
 * Each cached glyph has an age that gets reset every time a cached glyph gets used.
 * Glyphs that have not been used for the MAX_AGE frames are considered old. We keep
 * count of the pixels of each atlas that are taken up by old glyphs. We check the
 * fraction of old pixels every CHECK_INTERVAL frames, and if it is above MAX_OLD_RATIO, then
 * we drop the atlas an all the glyphs contained in it from the cache.
 */

#define MAX_AGE 60
#define CHECK_INTERVAL 10
#define MAX_OLD_RATIO 0.333

#define ATLAS_SIZE 512

static guint    glyph_cache_hash       (gconstpointer v);
static gboolean glyph_cache_equal      (gconstpointer v1,
                                        gconstpointer v2);
static void     glyph_cache_key_free   (gpointer      v);
static void     glyph_cache_value_free (gpointer      v);

static GskGLTextureAtlas *
create_atlas (GskGLGlyphCache *cache)
{
  GskGLTextureAtlas *atlas;

  atlas = g_new (GskGLTextureAtlas, 1);
  gsk_gl_texture_atlas_init (atlas, ATLAS_SIZE, ATLAS_SIZE);

  return atlas;
}

static void
free_atlas (gpointer v)
{
  GskGLTextureAtlas *atlas = v;

  g_assert (atlas->image.texture_id == 0);
  gsk_gl_texture_atlas_free (atlas);

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
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (atlas->image.texture_id != 0)
        {
          gsk_gl_image_destroy (&atlas->image, self->gl_driver);
          atlas->image.texture_id = 0;
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
  GskGLTextureAtlas *atlas = NULL;
  guint i, p;
  int packed_x, packed_y;

  /* Try all the atlases and pick the first one that can hold
   * our new glyph */
  for (i = 0, p = cache->atlases->len; i < p; i ++)
    {
      GskGLTextureAtlas *test_atlas = g_ptr_array_index (cache->atlases, i);
      gboolean was_packed;

      was_packed = gsk_gl_texture_atlas_pack (test_atlas, width, height, &packed_x, &packed_y);

      if (was_packed)
        {
          atlas = test_atlas;
          break;
        }
    }

  if (atlas == NULL)
    {
      gboolean was_packed;

      atlas = create_atlas (cache);
      g_ptr_array_add (cache->atlases, atlas);

      was_packed = gsk_gl_texture_atlas_pack (atlas,
                                              width, height,
                                              &packed_x, &packed_y);

      g_assert (was_packed);
    }

  value->tx = (float)packed_x / atlas->width;
  value->ty = (float)packed_y / atlas->height;
  value->tw = (float)width    / atlas->width;
  value->th = (float)height   / atlas->height;
  value->used = TRUE;

  value->atlas = atlas;

  if (atlas->user_data == NULL)
    atlas->user_data = g_new0 (DirtyGlyph, 1);

  ((DirtyGlyph *)atlas->user_data)->key = key;
  ((DirtyGlyph *)atlas->user_data)->value = value;

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (cache->renderer, GLYPH_CACHE))
    {
      g_print ("Glyph cache:\n");
      for (i = 0; i < cache->atlases->len; i++)
        {
          atlas = g_ptr_array_index (cache->atlases, i);
          g_print ("\tGskGLTextureAtlas %d (%dx%d): %.2g%% old pixels\n",
                   i, atlas->width, atlas->height,
                   gsk_gl_texture_atlas_get_unused_ratio (atlas));
        }
    }
#endif
}

static gboolean
render_glyph (const GskGLTextureAtlas *atlas,
              const DirtyGlyph        *glyph,
              GskImageRegion          *region)
{
  GlyphCacheKey *key = glyph->key;
  GskGLCachedGlyph *value = glyph->value;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  PangoGlyphString glyph_string;
  PangoGlyphInfo glyph_info;
  int surface_width, surface_height;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return FALSE;

  surface_width = value->draw_width * key->scale / 1024;
  surface_height = value->draw_height * key->scale / 1024;

  /* TODO: Give glyphs that large their own texture in the proper size. Don't
   *       put them in the atlas at all. */
  if (surface_width > ATLAS_SIZE || surface_height > ATLAS_SIZE)
    return FALSE;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, surface_width, surface_height);
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
  return TRUE;
}

static void
upload_dirty_glyph (GskGLGlyphCache   *self,
                    GskGLTextureAtlas *atlas)
{
  GskImageRegion region;

  g_assert (atlas->user_data != NULL);

  gdk_gl_context_push_debug_group_printf (gsk_gl_driver_get_gl_context (self->gl_driver),
                                          "Uploading glyph %d", ((DirtyGlyph *)atlas->user_data)->key->glyph);

  if (render_glyph (atlas, (DirtyGlyph *)atlas->user_data, &region))
    {

      gsk_gl_image_upload_region (&atlas->image, self->gl_driver, &region);

      g_free (region.data);
    }

  gdk_gl_context_pop_debug_group (gsk_gl_driver_get_gl_context (self->gl_driver));
  /* TODO: This could be unnecessary. We can just reuse the allocated
   *       DirtyGlyph next time. */
  g_clear_pointer (&atlas->user_data, g_free);
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
          GskGLTextureAtlas *atlas = value->atlas;

          if (atlas && !value->used)
            {
              gsk_gl_texture_atlas_mark_used (atlas, value->draw_width, value->draw_height);
              value->used = TRUE;
            }

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
  GskGLTextureAtlas *atlas = glyph->atlas;

  g_assert (atlas != NULL);

  if (atlas->image.texture_id == 0)
    {
      gsk_gl_image_create (&atlas->image, self->gl_driver, atlas->width, atlas->height);
      gdk_gl_context_label_object_printf (gsk_gl_driver_get_gl_context (self->gl_driver),
                                          GL_TEXTURE, atlas->image.texture_id,
                                          "Glyph atlas %d", atlas->image.texture_id);
    }

  if (atlas->user_data != NULL)
    upload_dirty_glyph (self, atlas);

  return &atlas->image;
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

  /* look for atlases to drop, and create a mapping of updated texture indices */
  for (i = self->atlases->len - 1; i >= 0; i--)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (gsk_gl_texture_atlas_get_unused_ratio (atlas) > MAX_OLD_RATIO)
        {
          GSK_RENDERER_NOTE(self->renderer, GLYPH_CACHE,
                   g_message ("Dropping atlas %d (%g.2%% old)", i,
                              gsk_gl_texture_atlas_get_unused_ratio (atlas)));

#if 0
          static int kk;

          g_message ("Dropping glyph cache... Ratio: %f",
                     gsk_gl_texture_atlas_get_unused_ratio (atlas));
          gsk_gl_image_write_to_png (&atlas->image, self->gl_driver,
                                     g_strdup_printf ("dropped_%d.png", kk++));
#endif

          if (atlas->image.texture_id != 0)
            {
              gsk_gl_image_destroy (&atlas->image, self->gl_driver);
              atlas->image.texture_id = 0;
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

  /* look for glyphs that have grown old since last time */
  g_hash_table_iter_init (&iter, self->hash_table);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      const guint age = self->timestamp - value->timestamp;

      if (MAX_AGE <= age && age < MAX_AGE + CHECK_INTERVAL)
        {
          GskGLTextureAtlas *atlas = value->atlas;

          if (atlas && value->used)
            {
              gsk_gl_texture_atlas_mark_unused (atlas, value->draw_width, value->draw_height);
              value->used = FALSE;
            }
        }
    }

  GSK_RENDERER_NOTE(self->renderer, GLYPH_CACHE, g_message ("Dropped %d glyphs", dropped));
}
