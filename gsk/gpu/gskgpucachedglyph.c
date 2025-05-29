#include "config.h"

#include "gskgpucachedglyphprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gsk/gskprivate.h"

typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;

struct _GskGpuCachedGlyph
{
  GskGpuCached parent;

  PangoFont *font;
  PangoGlyph glyph;
  GskGpuGlyphLookupFlags flags;
  float scale;

  GskGpuImage *image;
  graphene_rect_t bounds;
  graphene_point_t origin;
};

static void
gsk_gpu_cached_glyph_free (GskGpuCached *cached)
{
  GskGpuCachedGlyph *self = (GskGpuCachedGlyph *) cached;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);

  g_hash_table_remove (priv->glyph_cache, self);

  g_object_unref (self->font);
  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_glyph_should_collect (GskGpuCached *cached,
                                     gint64        cache_timeout,
                                     gint64        timestamp)
{
  if (gsk_gpu_cached_is_old (cached, cache_timeout, timestamp))
    {
      if (cached->atlas)
        gsk_gpu_cached_set_stale (cached, TRUE);
      else
        return TRUE;
    }

  /* Glyphs are only collected when their atlas is freed */
  return FALSE;
}

static guint
gsk_gpu_cached_glyph_hash (gconstpointer data)
{
  const GskGpuCachedGlyph *glyph = data;

  return GPOINTER_TO_UINT (glyph->font) ^
         glyph->glyph ^
         (glyph->flags << 24) ^
         ((guint) glyph->scale * PANGO_SCALE);
}

static gboolean
gsk_gpu_cached_glyph_equal (gconstpointer v1,
                            gconstpointer v2)
{
  const GskGpuCachedGlyph *glyph1 = v1;
  const GskGpuCachedGlyph *glyph2 = v2;

  return glyph1->font == glyph2->font
      && glyph1->glyph == glyph2->glyph
      && glyph1->flags == glyph2->flags
      && glyph1->scale == glyph2->scale;
}

static const GskGpuCachedClass GSK_GPU_CACHED_GLYPH_CLASS =
{
  sizeof (GskGpuCachedGlyph),
  "Glyph",
  gsk_gpu_cached_glyph_free,
  gsk_gpu_cached_glyph_should_collect
};

typedef struct
{
  PangoFont *font;
  PangoGlyph glyph;
} DrawGlyph;

static void
draw_glyph_free (gpointer data)
{
  DrawGlyph *dg = (DrawGlyph *) data;

  g_object_unref (dg->font);
  g_free (dg);
}

static void
draw_glyph (gpointer  data,
            cairo_t  *cr)
{
  DrawGlyph *dg = (DrawGlyph *) data;
  PangoRectangle ink_rect = { 0, };

  /* Draw glyph */
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  /* The pango code for drawing hex boxes uses the glyph width */
  if (dg->glyph & PANGO_GLYPH_UNKNOWN_FLAG)
    pango_font_get_glyph_extents (dg->font, dg->glyph, &ink_rect, NULL);

  pango_cairo_show_glyph_string (cr,
                                 dg->font,
                                 &(PangoGlyphString) {
                                     .num_glyphs = 1,
                                     .glyphs = (PangoGlyphInfo[1]) { {
                                         .glyph = dg->glyph,
                                         .geometry = {
                                           .width = ink_rect.width,
                                         }
                                     } }
                                 });
}

static void
draw_glyph_print (gpointer  data,
                  GString  *string)
{
  DrawGlyph *dg = (DrawGlyph *) data;
  PangoFontDescription *desc;
  char *str;

  desc = pango_font_describe_with_absolute_size (dg->font);
  str = pango_font_description_to_string (desc);

  g_string_append_printf (string, "glyph %u font %s", dg->glyph, str);

  g_free (str);
  pango_font_description_free (desc);
}

GskGpuImage *
gsk_gpu_cached_glyph_lookup (GskGpuCache            *self,
                             GskGpuFrame            *frame,
                             PangoFont              *font,
                             PangoGlyph              glyph,
                             GskGpuGlyphLookupFlags  flags,
                             float                   scale,
                             graphene_rect_t        *out_bounds,
                             graphene_point_t       *out_origin)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (self);
  GskGpuCachedGlyph lookup = {
    .font = font,
    .glyph = glyph,
    .flags = flags,
    .scale = scale
  };
  GskGpuCachedGlyph *cache;
  PangoRectangle ink_rect;
  graphene_rect_t rect;
  graphene_point_t origin;
  GskGpuImage *image;
  gsize atlas_x, atlas_y, padding;
  float subpixel_x, subpixel_y;
  PangoFont *scaled_font;

  cache = g_hash_table_lookup (priv->glyph_cache, &lookup);
  if (cache)
    {
      gsk_gpu_cached_use ((GskGpuCached *) cache);

      *out_bounds = cache->bounds;
      *out_origin = cache->origin;
      return cache->image;
    }

  scaled_font = gsk_reload_font (font, scale, CAIRO_HINT_METRICS_DEFAULT, CAIRO_HINT_STYLE_DEFAULT, CAIRO_ANTIALIAS_DEFAULT);

  subpixel_x = (flags & 3) / 4.f;
  subpixel_y = ((flags >> 2) & 3) / 4.f;
  pango_font_get_glyph_extents (scaled_font, glyph, &ink_rect, NULL);
  origin.x = floor (ink_rect.x * 1.0 / PANGO_SCALE + subpixel_x);
  origin.y = floor (ink_rect.y * 1.0 / PANGO_SCALE + subpixel_y);
  rect.size.width = ceil ((ink_rect.x + ink_rect.width) * 1.0 / PANGO_SCALE + subpixel_x) - origin.x;
  rect.size.height = ceil ((ink_rect.y + ink_rect.height) * 1.0 / PANGO_SCALE + subpixel_y) - origin.y;
  padding = 1;

  image = gsk_gpu_cache_add_atlas_image (self,
                                         rect.size.width + 2 * padding, rect.size.height + 2 * padding,
                                         &atlas_x, &atlas_y);
  if (image)
    {
      g_object_ref (image);
      rect.origin.x = atlas_x + padding;
      rect.origin.y = atlas_y + padding;
      cache = gsk_gpu_cached_new_from_current_atlas (self, &GSK_GPU_CACHED_GLYPH_CLASS);
    }
  else
    {
      image = gsk_gpu_device_create_upload_image (gsk_gpu_cache_get_device (self), FALSE, GDK_MEMORY_DEFAULT, FALSE, rect.size.width, rect.size.height),
      rect.origin.x = 0;
      rect.origin.y = 0;
      padding = 0;
      cache = gsk_gpu_cached_new (self, &GSK_GPU_CACHED_GLYPH_CLASS);
    }

  cache->font = g_object_ref (font);
  cache->glyph = glyph;
  cache->flags = flags;
  cache->scale = scale;
  cache->bounds = rect;
  cache->image = image;
  cache->origin = GRAPHENE_POINT_INIT (- origin.x + subpixel_x,
                                       - origin.y + subpixel_y);
  ((GskGpuCached *) cache)->pixels = (rect.size.width + 2 * padding) * (rect.size.height + 2 * padding);

  gsk_gpu_upload_cairo_into_op (frame,
                                cache->image,
                                &(cairo_rectangle_int_t) {
                                  .x = rect.origin.x - padding,
                                  .y = rect.origin.y - padding,
                                  .width = rect.size.width + 2 * padding,
                                  .height = rect.size.height + 2 * padding,
                                },
                                &GRAPHENE_RECT_INIT (- cache->origin.x - padding,
                                                     - cache->origin.y - padding,
                                                     rect.size.width + 2 * padding,
                                                     rect.size.height + 2 * padding),
                                draw_glyph,
                                draw_glyph_print,
                                g_memdup2 (&(DrawGlyph) {
                                  .font = g_object_ref (scaled_font),
                                  .glyph = glyph
                                }, sizeof (DrawGlyph)),
                                draw_glyph_free);

  g_hash_table_insert (priv->glyph_cache, cache, cache);
  gsk_gpu_cached_use ((GskGpuCached *) cache);

  *out_bounds = cache->bounds;
  *out_origin = cache->origin;

  g_object_unref (scaled_font);

  return cache->image;
}

void
gsk_gpu_cached_glyph_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->glyph_cache = g_hash_table_new (gsk_gpu_cached_glyph_hash,
                                        gsk_gpu_cached_glyph_equal);
}

void
gsk_gpu_cached_glyph_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_hash_table_unref (priv->glyph_cache);
}
