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

/* Cache eviction strategy
 *
 * Each cached glyph has an age that gets reset every time a cached
 * glyph gets used. Glyphs that have not been used for the
 * MAX_FRAME_AGE frames are considered old.
 *
 * We keep count of the pixels of each atlas that are taken up by old
 * data. When the fraction of old pixels gets too high, we drop the
 * atlas and all the items it contained.
 */

#define MAX_FRAME_AGE (5 * 60)

static guint    glyph_cache_hash       (gconstpointer v);
static gboolean glyph_cache_equal      (gconstpointer v1,
                                        gconstpointer v2);
static void     glyph_cache_key_free   (gpointer      v);
static void     glyph_cache_value_free (gpointer      v);

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

static gboolean
render_glyph (GlyphCacheKey    *key,
              GskGLCachedGlyph *value,
              GskImageRegion   *region)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t cairo_glyph;
  int surface_width, surface_height;
  int stride;
  unsigned char *data;

  scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *)key->font);
  if (G_UNLIKELY (!scaled_font || cairo_scaled_font_status (scaled_font) != CAIRO_STATUS_SUCCESS))
    {
      g_warning ("Failed to get a font");
      return FALSE;
    }

  surface_width = value->draw_width * key->scale / 1024;
  surface_height = value->draw_height * key->scale / 1024;

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, surface_width);
  data = g_malloc0 (stride * surface_height);
  surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32,
                                                 surface_width, surface_height,
                                                 stride);
  cairo_surface_set_device_scale (surface, key->scale / 1024.0, key->scale / 1024.0);

  cr = cairo_create (surface);

  cairo_set_scaled_font (cr, scaled_font);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cairo_glyph.index = key->glyph;
  cairo_glyph.x = 0.25 * key->xshift - value->draw_x;
  cairo_glyph.y = 0.25 * key->yshift - value->draw_y;

  cairo_show_glyphs (cr, &cairo_glyph, 1);

  cairo_destroy (cr);

  cairo_surface_flush (surface);

  region->width = cairo_image_surface_get_width (surface);
  region->height = cairo_image_surface_get_height (surface);
  region->stride = cairo_image_surface_get_stride (surface);
  region->data = data;
  if (value->atlas)
    {
      region->x = (gsize)(value->tx * value->atlas->width);
      region->y = (gsize)(value->ty * value->atlas->height);
    }
  else
    {
      region->x = 0;
      region->y = 0;
    }

  cairo_surface_destroy (surface);

  return TRUE;
}

static void
upload_glyph (GlyphCacheKey    *key,
              GskGLCachedGlyph *value)
{
  GskImageRegion r;

  gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                          "Uploading glyph %d",
                                          key->glyph);

  if (render_glyph (key, value, &r))
    {
      glBindTexture (GL_TEXTURE_2D, value->texture_id);
      glTexSubImage2D (GL_TEXTURE_2D, 0,
                       r.x, r.y, r.width, r.height,
                       GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                       r.data);
      g_free (r.data);
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
  int packed_x = 0;
  int packed_y = 0;

  gsk_gl_texture_atlases_pack (self->atlases, width + 2, height + 2, &atlas, &packed_x, &packed_y);

  value->tx = (float)(packed_x + 1) / atlas->width;
  value->ty = (float)(packed_y + 1) / atlas->height;
  value->tw = (float)width / atlas->width;
  value->th = (float)height / atlas->height;
  value->used = TRUE;

  value->atlas = atlas;
  value->texture_id = atlas->texture_id;

  upload_glyph (key, value);
}

#define PHASE(x) ((x % PANGO_SCALE) * 4 / PANGO_SCALE)

void
gsk_gl_glyph_cache_get_texture (GskGLDriver      *driver,
                                PangoFont        *font,
                                PangoGlyph        glyph,
                                int               x,
                                int               y,
                                float             scale,
                                GskGLCachedGlyph *value)
{
  PangoRectangle ink_rect;
  GlyphCacheKey key;
  int width, height;
  guint texture_id;
  guint xshift = PHASE (x);
  guint yshift = PHASE (y);

  pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
  pango_extents_to_pixels (&ink_rect, NULL);
  if (xshift != 0)
    ink_rect.width += 1;
  if (yshift != 0)
    ink_rect.height += 1;

  key.font = font;
  key.glyph = glyph;
  key.xshift = xshift;
  key.yshift = yshift;
  key.scale = (guint)(scale * 1024);

  value->atlas = NULL;
  value->timestamp = 0;

  value->draw_x = ink_rect.x;
  value->draw_y = ink_rect.y;
  value->draw_width = ink_rect.width;
  value->draw_height = ink_rect.height;

  value->tx = 0.0f;
  value->ty = 0.0f;
  value->tw = 1.0f;
  value->th = 1.0f;

  width = value->draw_width * key.scale / 1024;
  height = value->draw_height * key.scale / 1024;

  texture_id = gsk_gl_driver_create_texture (driver, width, height);
  gsk_gl_driver_bind_source_texture (driver, texture_id);
  gsk_gl_driver_init_texture_empty (driver, texture_id, GL_NEAREST, GL_NEAREST);

  value->texture_id = texture_id;

  upload_glyph (&key, value);
}

gboolean
gsk_gl_glyph_cache_lookup (GskGLGlyphCache  *cache,
                           PangoFont        *font,
                           PangoGlyph        glyph,
                           int               x,
                           int               y,
                           float             scale,
                           GskGLCachedGlyph *glyph_out)
{
  GskGLCachedGlyph *value;
  guint xshift = PHASE (x);
  guint yshift = PHASE (y);

  value = g_hash_table_lookup (cache->hash_table,
                               &(GlyphCacheKey) {
                                 .font = font,
                                 .glyph = glyph,
                                 .xshift = xshift,
                                 .yshift = yshift,
                                 .scale = (guint)(scale * 1024)
                               });

  if (value)
    {
      const guint age = cache->timestamp - value->timestamp;

      if (age > MAX_FRAME_AGE)
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

  if (value == NULL)
    {
      PangoRectangle ink_rect;

      pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
      pango_extents_to_pixels (&ink_rect, NULL);
      if (xshift != 0)
        ink_rect.width += 1;
      if (yshift != 0)
        ink_rect.height += 1;

      value = g_new0 (GskGLCachedGlyph, 1);

      value->draw_x = ink_rect.x;
      value->draw_y = ink_rect.y;
      value->draw_width = ink_rect.width;
      value->draw_height = ink_rect.height;
      value->timestamp = cache->timestamp;
      value->atlas = NULL; /* For now */

      if (ink_rect.width < 128 && ink_rect.height < 128)
        {
          GlyphCacheKey *key;

          key = g_new0 (GlyphCacheKey, 1);

          key->font = g_object_ref (font);
          key->glyph = glyph;
          key->xshift = xshift;
          key->yshift = yshift;
          key->scale = (guint)(scale * 1024);

          if (ink_rect.width > 0 && ink_rect.height > 0 && key->scale > 0)
            add_to_cache (cache, key, value);

          *glyph_out = *value;
          g_hash_table_insert (cache->hash_table, key, value);
        }
      else
        {
          *glyph_out = *value;
          glyph_cache_value_free (value);
        }
    }
  else
    {
      *glyph_out = *value;
    }

  return glyph_out->atlas != NULL;
}

void
gsk_gl_glyph_cache_begin_frame (GskGLGlyphCache *self)
{
  GHashTableIter iter;
  GlyphCacheKey *key;
  GskGLCachedGlyph *value;
  guint dropped = 0;

  self->timestamp++;

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

          if (age > MAX_FRAME_AGE)
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
