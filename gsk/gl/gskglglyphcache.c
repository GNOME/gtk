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
create_atlas (GskGLGlyphCache *self,
              int              width,
              int              height)
{
  GskGLTextureAtlas *atlas;

  atlas = g_new (GskGLTextureAtlas, 1);
  gsk_gl_texture_atlas_init (atlas, MAX (width, ATLAS_SIZE), MAX (height, ATLAS_SIZE));

  GSK_NOTE(GLYPH_CACHE, g_message ("Create atlas %d x %d", atlas->width, atlas->height));

  return atlas;
}

GskGLGlyphCache *
gsk_gl_glyph_cache_new (GdkDisplay *display,
                        GskGLTextureAtlases *atlases)
{
  GskGLGlyphCache *glyph_cache;

  glyph_cache = g_new0 (GskGLGlyphCache, 1);

  glyph_cache->display = display;
  glyph_cache->hash_table = g_hash_table_new_full (glyph_cache_hash, glyph_cache_equal,
                                                   glyph_cache_key_free, glyph_cache_value_free);

  glyph_cache->atlases = gsk_gl_texture_atlases_ref (atlases);

  glyph_cache->ref_count = 1;

  return glyph_cache;
}

GskGLGlyphCache *
gsk_gl_glyph_cache_ref (GskGLGlyphCache *self)
{
  self->ref_count++;

  return self;
}

void
gsk_gl_glyph_cache_unref (GskGLGlyphCache *self)
{
  g_assert (self->ref_count > 0);

  if (self->ref_count == 1)
    {
      gsk_gl_texture_atlases_unref (self->atlases);
      g_hash_table_unref (self->hash_table);
      g_free (self);
      return;
    }

  self->ref_count--;
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

static gboolean
render_glyph (GlyphCacheKey    *key,
              GskGLCachedGlyph *value,
              GskImageRegion   *region)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  PangoGlyphString glyph_string;
  PangoGlyphInfo glyph_info;
  int surface_width, surface_height;
  int stride;
  unsigned char *data;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    return FALSE;

  surface_width = value->draw_width * key->scale / 1024;
  surface_height = value->draw_height * key->scale / 1024;

  /* XXX should never happen */
  if (surface_width > value->atlas->width || surface_height > value->atlas->height)
    return FALSE;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, surface_width);
  data = g_malloc0 (stride * surface_height);
  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32,
                                                 surface_width, surface_height,
                                                 stride);
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
  region->data = data;
  region->x = (gsize)(value->tx * value->atlas->width);
  region->y = (gsize)(value->ty * value->atlas->height);

  cairo_surface_destroy (surface);

  return TRUE;
}

static void
upload_region (guint           texture_id,
               GskImageRegion *region)
{
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glTextureSubImage2D (texture_id, 0, region->x, region->y, region->width, region->height,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, region->data);
}

static void
upload_glyph (GskGLGlyphCache  *self,
              GlyphCacheKey    *key,
              GskGLCachedGlyph *value)
{
  GskImageRegion region;

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Uploading glyph %d",
                                          key->glyph);

  if (render_glyph (key, value, &region))
    {
      upload_region (value->atlas->texture_id, &region);
      g_free (region.data);
    }

  gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());
}

static void
add_to_cache (GskGLGlyphCache  *self,
              GlyphCacheKey    *key,
              GskGLCachedGlyph *value)
{
  const int width = value->draw_width * key->scale / 1024;
  const int height = value->draw_height * key->scale / 1024;
  GskGLTextureAtlas *atlas = NULL;
  guint i, p;
  int packed_x, packed_y;
  GPtrArray *atlases = self->atlases->atlases;

  /* Try all the atlases and pick the first one that can hold
   * our new glyph */
  for (i = 0, p = atlases->len; i < p; i ++)
    {
      GskGLTextureAtlas *test_atlas = g_ptr_array_index (atlases, i);
      gboolean was_packed;

      was_packed = gsk_gl_texture_atlas_pack (test_atlas, width + 2, height + 2, &packed_x, &packed_y);

      if (was_packed)
        {
          atlas = test_atlas;
          break;
        }
    }

  if (atlas == NULL)
    {
      gboolean was_packed;

      atlas = create_atlas (self, width + 2, height + 2);

      g_ptr_array_add (atlases, atlas);

      was_packed = gsk_gl_texture_atlas_pack (atlas,
                                              width + 2, height + 2,
                                              &packed_x, &packed_y);

      g_assert (was_packed);
    }

  value->tx = (float)(packed_x + 1) / atlas->width;
  value->ty = (float)(packed_y + 1) / atlas->height;
  value->tw = (float)width / atlas->width;
  value->th = (float)height / atlas->height;
  value->used = TRUE;

  value->atlas = atlas;

  upload_glyph (self, key, value);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (GLYPH_CACHE))
    {
      for (i = 0; i < atlases->len; i++)
        {
          atlas = g_ptr_array_index (atlases, i);
          g_message ("atlas %d (%dx%d): %.2g%% old pixels",
                   i, atlas->width, atlas->height,
                   gsk_gl_texture_atlas_get_unused_ratio (atlas));
        }
    }
#endif
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

      if (MAX_AGE <= age)
        {
          GskGLTextureAtlas *atlas = value->atlas;

          if (atlas && !value->used)
            {
              gsk_gl_texture_atlas_mark_used (atlas, value->draw_width, value->draw_height);
              value->used = TRUE;
            }

          value->timestamp = cache->timestamp;
        }

      value->timestamp = cache->timestamp;
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

guint
gsk_gl_glyph_cache_get_glyph_texture_id (GskGLGlyphCache        *self,
                                         const GskGLCachedGlyph *glyph)
{
  return glyph->atlas->texture_id;
}

void
gsk_gl_glyph_cache_begin_frame (GskGLGlyphCache *self)
{
  GHashTableIter iter;
  GlyphCacheKey *key;
  GskGLCachedGlyph *value;
  guint dropped = 0;

  self->timestamp++;

  if ((self->timestamp - 1) % CHECK_INTERVAL != 0)
    return;

  g_hash_table_iter_init (&iter, self->hash_table);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      guint pos;

      if (!g_ptr_array_find (self->atlases->atlases, value->atlas, &pos))
        {
          g_hash_table_iter_remove (&iter);
          dropped++;
        }
      else
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
    }

  GSK_NOTE(GLYPH_CACHE, if (dropped > 0) g_message ("Dropped %d glyphs", dropped));
}
