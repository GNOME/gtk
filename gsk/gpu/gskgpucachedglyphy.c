#include "config.h"

#include "gskgpucachedglyphyprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpuuploadopprivate.h"

#include <glyphy.h>
#include <glyphy-harfbuzz.h>

#include <cairo.h>
#include <hb.h>
#include <string.h>

#define ITEM_W 64
#define ITEM_H_QUANTUM 8
#define GLYPHY_ATLAS_WIDTH 2048
#define GLYPHY_ATLAS_HEIGHT 1024
#define GLYPHY_ATLAS_TIMEOUT_SCALE 4

typedef guint FontKey;

typedef struct _GskGpuGlyphyKey
{
  FontKey font;
  PangoGlyph glyph;
} GskGpuGlyphyKey;

typedef struct _GskGpuGlyphyContext
{
  glyphy_curve_accumulator_t *acc;
  GArray *curves;
  glyphy_encoder_t *encoder;
} GskGpuGlyphyContext;

typedef struct {
  guint16 r;
  guint16 g;
  guint16 b;
  guint16 a;
} GskGpuGlyphyTexelU16;

typedef struct _GskGpuCachedGlyphyAtlas GskGpuCachedGlyphyAtlas;

typedef struct _GskGpuCachedGlyphy GskGpuCachedGlyphy;

struct _GskGpuCachedGlyphyAtlas
{
  GskGpuCached parent;

  GskGpuImage *image;
  guint width;
  guint height;
  guint cursor_x;
  guint cursor_y;
  guint item_w;
  guint item_h_q;
  gsize remaining_pixels;
};

struct _GskGpuCachedGlyphy
{
  GskGpuCached parent;

  GskGpuGlyphyKey key;
  GskGpuImage *image;
  GskGpuGlyphyValue value;
};

static void
gsk_gpu_cached_glyphy_free (GskGpuCached *cached)
{
  GskGpuCachedGlyphy *self = (GskGpuCachedGlyphy *) cached;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);

  g_hash_table_remove (priv->glyphy_cache, self);

  g_clear_object (&self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_glyphy_should_collect (GskGpuCached *cached,
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

static const GskGpuCachedClass GSK_GPU_CACHED_GLYPHY_CLASS =
{
  sizeof (GskGpuCachedGlyphy),
  "Glyphy",
  gsk_gpu_cached_glyphy_free,
  gsk_gpu_cached_glyphy_should_collect
};

static void
gsk_gpu_cached_glyphy_atlas_free (GskGpuCached *cached)
{
  GskGpuCachedGlyphyAtlas *self = (GskGpuCachedGlyphyAtlas *) cached;
  GskGpuCache *cache = cached->cache;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  gsk_gpu_cache_free_atlas_items (cache, (GskGpuCachedAtlas *) self);

  if (priv->glyphy_atlas == self)
    priv->glyphy_atlas = NULL;

  g_object_unref (self->image);

  g_free (self);
}

static gboolean
gsk_gpu_cached_glyphy_atlas_should_collect (GskGpuCached *cached,
                                            gint64        cache_timeout,
                                            gint64        timestamp)
{
  GskGpuCachedGlyphyAtlas *self = (GskGpuCachedGlyphyAtlas *) cached;
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);

  if (priv->glyphy_atlas == self &&
      gsk_gpu_cached_is_old (cached, cache_timeout * GLYPHY_ATLAS_TIMEOUT_SCALE, timestamp) &&
      cached->pixels == 0)
    return TRUE;

  return cached->pixels + self->remaining_pixels < (self->width * self->height / 2);
}

static const GskGpuCachedClass GSK_GPU_CACHED_GLYPHY_ATLAS_CLASS =
{
  sizeof (GskGpuCachedGlyphyAtlas),
  "GlyphyAtlas",
  gsk_gpu_cached_glyphy_atlas_free,
  gsk_gpu_cached_glyphy_atlas_should_collect
};

static GskGpuCachedGlyphyAtlas *
gsk_gpu_cached_glyphy_atlas_new (GskGpuCache *cache)
{
  GskGpuCachedGlyphyAtlas *self;
  GskGpuImage *image;

  image = gsk_gpu_device_create_upload_image (gsk_gpu_cache_get_device (cache),
                                              FALSE,
                                              GDK_MEMORY_R16G16B16A16,
                                              GSK_GPU_CONVERSION_NONE,
                                              GLYPHY_ATLAS_WIDTH,
                                              GLYPHY_ATLAS_HEIGHT);
  if (image == NULL)
    return NULL;

  self = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_GLYPHY_ATLAS_CLASS);
  self->image = image;

  self->width = gsk_gpu_image_get_width (self->image);
  self->height = gsk_gpu_image_get_height (self->image);
  self->remaining_pixels = self->width * self->height;
  self->item_w = ITEM_W;
  self->item_h_q = ITEM_H_QUANTUM;

  return self;
}

static gboolean
gsk_gpu_cached_glyphy_atlas_allocate (GskGpuCachedGlyphyAtlas *atlas,
                                      guint                    width,
                                      guint                    height,
                                      guint                   *out_x,
                                      guint                   *out_y)
{
  guint cursor_save_x;
  guint cursor_save_y;

  if ((height & (atlas->item_h_q - 1)) != 0)
    height = (height + atlas->item_h_q - 1) & ~(atlas->item_h_q - 1);

  if (height == 0)
    return FALSE;

  if (width != atlas->item_w)
    return FALSE;

  if (height > atlas->height || width > atlas->width)
    return FALSE;

  cursor_save_x = atlas->cursor_x;
  cursor_save_y = atlas->cursor_y;

  if (atlas->cursor_y + height > atlas->height)
    {
      atlas->cursor_x += atlas->item_w;
      atlas->cursor_y = 0;
    }

  if (atlas->cursor_x + width <= atlas->width &&
      atlas->cursor_y + height <= atlas->height)
    {
      *out_x = atlas->cursor_x;
      *out_y = atlas->cursor_y;
      atlas->cursor_y += height;
      atlas->remaining_pixels -= width * height;
      ((GskGpuCached *) atlas)->pixels += width * height;
      return TRUE;
    }

  atlas->cursor_x = cursor_save_x;
  atlas->cursor_y = cursor_save_y;

  return FALSE;
}

static guint
gsk_gpu_glyphy_key_hash (gconstpointer data)
{
  const GskGpuCachedGlyphy *glyphy = data;
  const GskGpuGlyphyKey *key = &glyphy->key;

  return (key->font << 8) ^ key->glyph;
}

static gboolean
gsk_gpu_glyphy_key_equal (gconstpointer v1,
                          gconstpointer v2)
{
  const GskGpuCachedGlyphy *glyphy1 = v1;
  const GskGpuCachedGlyphy *glyphy2 = v2;

  return memcmp (&glyphy1->key, &glyphy2->key, sizeof (GskGpuGlyphyKey)) == 0;
}

static glyphy_bool_t
accumulate_curve (const glyphy_curve_t *curve,
                  GArray              *curves)
{
  g_array_append_vals (curves, curve, 1);
  return TRUE;
}

static inline hb_font_t *
get_nominal_size_hb_font (PangoFont *font)
{
  hb_font_t *hbfont;
  const float *coords;
  unsigned int length;

  hbfont = (hb_font_t *) g_object_get_data ((GObject *)font, "glyphy-nominal-size-font");
  if (hbfont == NULL)
    {
      hbfont = hb_font_create (hb_font_get_face (pango_font_get_hb_font (font)));
      coords = hb_font_get_var_coords_design (pango_font_get_hb_font (font), &length);
      if (length > 0)
        hb_font_set_var_coords_design (hbfont, coords, length);
      hb_font_set_synthetic_slant (hbfont, hb_font_get_synthetic_slant (pango_font_get_hb_font (font)));

      g_object_set_data_full ((GObject *)font, "glyphy-nominal-size-font",
                              hbfont, (GDestroyNotify)hb_font_destroy);
    }

  return hbfont;
}

static gboolean
encode_glyph (GskGpuGlyphyContext *ctx,
              hb_font_t           *font,
              unsigned int         glyph_index,
              glyphy_texel_t      *buffer,
              guint                buffer_len,
              guint               *output_len,
              glyphy_extents_t    *extents)
{
  ctx->curves->len = 0;

  glyphy_curve_accumulator_reset (ctx->acc);
  glyphy_curve_accumulator_set_callback (ctx->acc,
                                         (glyphy_curve_accumulator_callback_t)accumulate_curve,
                                         ctx->curves);

  glyphy_harfbuzz(font_get_glyph_shape) (font, glyph_index, ctx->acc);
  if (!glyphy_curve_accumulator_successful (ctx->acc))
    return FALSE;

  if (!glyphy_encoder_encode (ctx->encoder,
                              ctx->curves->len ? (glyphy_curve_t *) ctx->curves->data : NULL,
                              ctx->curves->len,
                              buffer,
                              buffer_len,
                              output_len,
                              extents))
    return FALSE;

  return TRUE;
}

guint
gsk_gpu_glyphy_get_font_key (PangoFont *font)
{
  FontKey key;

  key = (FontKey) GPOINTER_TO_UINT (g_object_get_data ((GObject *)font, "glyphy-font-key"));
  if (key == 0)
    {
      PangoFontDescription *desc = pango_font_describe (font);
      pango_font_description_set_size (desc, 10 * PANGO_SCALE);
      key = (FontKey) pango_font_description_hash (desc);
      pango_font_description_free (desc);
      g_object_set_data ((GObject *)font, "glyphy-font-key", GUINT_TO_POINTER (key));
    }

  return key;
}

float
gsk_gpu_glyphy_get_font_scale (PangoFont *font)
{
  hb_font_t *hbfont;
  int x_scale, y_scale;

  hbfont = pango_font_get_hb_font (font);
  hb_font_get_scale (hbfont, &x_scale, &y_scale);

  return MAX (x_scale, y_scale) / 1000.0f;
}

GskGpuImage *
gsk_gpu_cached_glyphy_lookup (GskGpuCache             *cache,
                              GskGpuFrame             *frame,
                              PangoFont               *font,
                              PangoGlyph               glyph,
                              const GskGpuGlyphyValue **out_value)
{
  static glyphy_texel_t buffer[4096 * 16];
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskGpuCachedGlyphy lookup = {
    .key = {
      .font = gsk_gpu_glyphy_get_font_key (font),
      .glyph = glyph,
    }
  };
  GskGpuCachedGlyphy *cached;
  GskGpuCachedGlyphyAtlas *atlas = NULL;
  GskGpuImage *image;
  glyphy_extents_t extents;
  hb_font_t *hbfont;
  guint packed_x, packed_y;
  guint output_len = 0;
  guint width, height;
  gsize upload_len;
  GskGpuGlyphyTexelU16 *upload;

  g_assert (out_value != NULL);

  cached = g_hash_table_lookup (priv->glyphy_cache, &lookup);
  if (cached)
    {
      gsk_gpu_cached_use ((GskGpuCached *) cached);
      *out_value = &cached->value;
      return cached->image;
    }

  hbfont = get_nominal_size_hb_font (font);

  if (!encode_glyph ((GskGpuGlyphyContext *)priv->glyphy_context,
                     hbfont,
                     glyph,
                     buffer,
                     G_N_ELEMENTS (buffer),
                     &output_len,
                     &extents))
    return NULL;

  if (output_len == 0 || glyphy_extents_is_empty (&extents))
    return NULL;

  width = ITEM_W;
  height = (output_len + width - 1) / width;
  if ((height & (ITEM_H_QUANTUM - 1)) != 0)
    height = (height + ITEM_H_QUANTUM - 1) & ~(ITEM_H_QUANTUM - 1);

  atlas = (GskGpuCachedGlyphyAtlas *) priv->glyphy_atlas;
  if (atlas == NULL || !gsk_gpu_cached_glyphy_atlas_allocate (atlas, width, height, &packed_x, &packed_y))
    {
      atlas = gsk_gpu_cached_glyphy_atlas_new (cache);
      if (atlas)
        priv->glyphy_atlas = atlas;
      if (atlas == NULL || !gsk_gpu_cached_glyphy_atlas_allocate (atlas, width, height, &packed_x, &packed_y))
        atlas = NULL;
    }

  if (atlas)
    {
      image = g_object_ref (atlas->image);
      cached = gsk_gpu_cached_new_from_atlas (cache, &GSK_GPU_CACHED_GLYPHY_CLASS, (GskGpuCachedAtlas *) atlas);
    }
  else
    {
      image = gsk_gpu_device_create_upload_image (gsk_gpu_cache_get_device (cache),
                                                   FALSE,
                                                   GDK_MEMORY_R16G16B16A16,
                                                   GSK_GPU_CONVERSION_NONE,
                                                   width,
                                                   height);
      if (image == NULL)
        return NULL;
      packed_x = 0;
      packed_y = 0;
      cached = gsk_gpu_cached_new (cache, &GSK_GPU_CACHED_GLYPHY_CLASS);
    }

  upload_len = width * height;
  upload = g_malloc0 (upload_len * sizeof (GskGpuGlyphyTexelU16));
  for (guint i = 0; i < output_len; i++)
    {
      upload[i].r = (guint16) ((int) buffer[i].r + 32768);
      upload[i].g = (guint16) ((int) buffer[i].g + 32768);
      upload[i].b = (guint16) ((int) buffer[i].b + 32768);
      upload[i].a = (guint16) ((int) buffer[i].a + 32768);
    }

  gsk_gpu_upload_data_into_op (frame,
                               image,
                               &(cairo_rectangle_int_t) {
                                 .x = packed_x,
                                 .y = packed_y,
                                 .width = width,
                                 .height = height,
                               },
                               GDK_MEMORY_R16G16B16A16,
                               width,
                               height,
                               width * sizeof (GskGpuGlyphyTexelU16),
                               upload,
                               g_free);

  cached->key = lookup.key;
  cached->image = image;
  cached->value.min_x = extents.min_x;
  cached->value.min_y = extents.min_y;
  cached->value.max_x = extents.max_x;
  cached->value.max_y = extents.max_y;
  cached->value.atlas_x = packed_x;
  cached->value.atlas_y = packed_y;
  cached->value.atlas_width = gsk_gpu_image_get_width (image);
  cached->value.atlas_height = gsk_gpu_image_get_height (image);
  ((GskGpuCached *) cached)->pixels = width * height;

  g_hash_table_insert (priv->glyphy_cache, cached, cached);
  gsk_gpu_cached_use ((GskGpuCached *) cached);

  *out_value = &cached->value;

  return cached->image;
}

void
gsk_gpu_cached_glyphy_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskGpuGlyphyContext *ctx;

  priv->glyphy_cache = g_hash_table_new (gsk_gpu_glyphy_key_hash,
                                         gsk_gpu_glyphy_key_equal);

  ctx = g_new0 (GskGpuGlyphyContext, 1);
  ctx->acc = glyphy_curve_accumulator_create ();
  ctx->curves = g_array_new (FALSE, FALSE, sizeof (glyphy_curve_t));
  ctx->encoder = glyphy_encoder_create ();
  priv->glyphy_context = ctx;
  priv->glyphy_atlas = NULL;
}

void
gsk_gpu_cached_glyphy_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskGpuGlyphyContext *ctx = (GskGpuGlyphyContext *) priv->glyphy_context;

  if (ctx)
    {
      g_clear_pointer (&ctx->encoder, glyphy_encoder_destroy);
      g_clear_pointer (&ctx->acc, glyphy_curve_accumulator_destroy);
      g_clear_pointer (&ctx->curves, g_array_unref);
      g_free (ctx);
    }

  priv->glyphy_context = NULL;
  priv->glyphy_atlas = NULL;

  g_clear_pointer (&priv->glyphy_cache, g_hash_table_unref);
}
