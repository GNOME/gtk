#include "config.h"

#include "gskgpudeviceprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdktextureprivate.h"

#define MAX_SLICES_PER_ATLAS 64

#define ATLAS_SIZE 1024

#define MAX_ATLAS_ITEM_SIZE 256

typedef enum
{
  GSK_GPU_CACHE_TEXTURE,
  GSK_GPU_CACHE_GLYPH,
  GSK_GPU_CACHE_ATLAS
} GskGpuCacheType;

typedef struct _GskGpuCachedTexture GskGpuCachedTexture;
typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;
typedef struct _GskGpuCachedAtlas GskGpuCachedAtlas;
typedef struct _GskGpuCacheEntry GskGpuCacheEntry;

struct _GskGpuCacheEntry
{
  GskGpuCacheType type;
  guint64 last_use_timestamp;
};

struct _GskGpuCachedAtlas
{
  GskGpuCacheEntry entry;
  GskGpuImage *image;

  gsize n_slices;
  struct {
    gsize width;
    gsize height;
  } slices[MAX_SLICES_PER_ATLAS];
};

struct _GskGpuCachedTexture
{
  GskGpuCacheEntry entry;
  /* atomic */ GdkTexture *texture;
  GskGpuImage *image;
};

struct _GskGpuCachedGlyph
{
  GskGpuCacheEntry entry;
  PangoFont *font;
  PangoGlyph glyph;
  GskGpuGlyphLookupFlags flags;
  float scale;

  GskGpuImage *image;
  graphene_rect_t bounds;
  graphene_point_t origin;
};

#define GDK_ARRAY_NAME gsk_gpu_cache_entries
#define GDK_ARRAY_TYPE_NAME GskGpuCacheEntries
#define GDK_ARRAY_ELEMENT_TYPE GskGpuCacheEntry *
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

typedef struct _GskGpuDevicePrivate GskGpuDevicePrivate;

struct _GskGpuDevicePrivate
{
  GdkDisplay *display;
  gsize max_image_size;

  GskGpuCacheEntries cache;
  guint cache_gc_source;
  GHashTable *glyph_cache;

  GskGpuCachedAtlas *current_atlas;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuDevice, gsk_gpu_device, G_TYPE_OBJECT)

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

void
gsk_gpu_device_gc (GskGpuDevice *self,
                   gint64        timestamp)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  gsize i;

  for (i = 0; i < gsk_gpu_cache_entries_get_size (&priv->cache); i++)
    {
      GskGpuCacheEntry *entry = gsk_gpu_cache_entries_get (&priv->cache, i);

      switch (entry->type)
        {
          case GSK_GPU_CACHE_TEXTURE:
            {
              GskGpuCachedTexture *texture = (GskGpuCachedTexture *) entry;
              if (texture->texture != NULL)
                continue;

              gdk_texture_clear_render_data (texture->texture);
              g_object_unref (texture->image);
            }
            break;

          case GSK_GPU_CACHE_ATLAS:
          case GSK_GPU_CACHE_GLYPH:
            break;

          default:
            g_assert_not_reached ();
            break;
        }

      if (i + 1 != gsk_gpu_cache_entries_get_size (&priv->cache))
        {
          *gsk_gpu_cache_entries_index (&priv->cache, i) = *gsk_gpu_cache_entries_index (&priv->cache, gsk_gpu_cache_entries_get_size (&priv->cache) - 1);
          gsk_gpu_cache_entries_set_size (&priv->cache, gsk_gpu_cache_entries_get_size (&priv->cache) - 1);
        }
      i--;
    }
}

static void
gsk_gpu_device_clear_cache (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  gsize i;

  for (i = 0; i < gsk_gpu_cache_entries_get_size (&priv->cache); i++)
    {
      GskGpuCacheEntry *entry = gsk_gpu_cache_entries_get (&priv->cache, i);

      switch (entry->type)
        {
          case GSK_GPU_CACHE_ATLAS:
            {
              GskGpuCachedAtlas *atlas = (GskGpuCachedAtlas *) entry;

              g_object_unref (atlas->image);
            }
            break;

          case GSK_GPU_CACHE_TEXTURE:
            {
              GskGpuCachedTexture *texture = (GskGpuCachedTexture *) entry;

              if (texture->texture)
                gdk_texture_clear_render_data (texture->texture);
              g_object_unref (texture->image);
            }
            break;

          case GSK_GPU_CACHE_GLYPH:
            {
              GskGpuCachedGlyph *glyph = (GskGpuCachedGlyph *) entry;

              g_object_unref (glyph->font);
              g_object_unref (glyph->image);
            }

            break;

          default:
            g_assert_not_reached ();
            break;
        }

      g_free (entry);
    }

  gsk_gpu_cache_entries_set_size (&priv->cache, 0);
}

static void
gsk_gpu_device_dispose (GObject *object)
{
  GskGpuDevice *self = GSK_GPU_DEVICE (object);
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  g_hash_table_unref (priv->glyph_cache);
  gsk_gpu_device_clear_cache (self);
  gsk_gpu_cache_entries_clear (&priv->cache);

  G_OBJECT_CLASS (gsk_gpu_device_parent_class)->dispose (object);
}

static void
gsk_gpu_device_finalize (GObject *object)
{
  GskGpuDevice *self = GSK_GPU_DEVICE (object);
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  g_object_unref (priv->display);

  G_OBJECT_CLASS (gsk_gpu_device_parent_class)->finalize (object);
}

static void
gsk_gpu_device_class_init (GskGpuDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_gpu_device_dispose;
  object_class->finalize = gsk_gpu_device_finalize;
}

static void
gsk_gpu_device_init (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  gsk_gpu_cache_entries_init (&priv->cache);
  priv->glyph_cache = g_hash_table_new (gsk_gpu_cached_glyph_hash,
                                        gsk_gpu_cached_glyph_equal);
}

void
gsk_gpu_device_setup (GskGpuDevice *self,
                      GdkDisplay   *display,
                      gsize         max_image_size)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  priv->display = g_object_ref (display);
  priv->max_image_size = max_image_size;
}

GdkDisplay *
gsk_gpu_device_get_display (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->display;
}

gsize
gsk_gpu_device_get_max_image_size (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->max_image_size;
}

GskGpuImage *
gsk_gpu_device_create_offscreen_image (GskGpuDevice   *self,
                                       GdkMemoryDepth  depth,
                                       gsize           width,
                                       gsize           height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_offscreen_image (self, depth, width, height);
}

GskGpuImage *
gsk_gpu_device_create_upload_image (GskGpuDevice   *self,
                                    GdkMemoryFormat format,
                                    gsize           width,
                                    gsize           height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_upload_image (self, format, width, height);
}

/* This rounds up to the next number that has <= 2 bits set:
 * 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, ...
 * That is roughly sqrt(2), so it should limit waste
 */
static gsize
round_up_atlas_size (gsize num)
{
  gsize storage = g_bit_storage (num);

  num = num + (((1 << storage) - 1) >> 2);
  num &= (((gsize) 7) << storage) >> 2;

  return num;
}

static gboolean
gsk_gpu_cached_atlas_allocate (GskGpuCachedAtlas *atlas,
                               gsize              width,
                               gsize              height,
                               gsize             *out_x,
                               gsize             *out_y)
{
  gsize i;
  gsize waste, slice_waste;
  gsize best_slice;
  gsize y, best_y;
  gboolean can_add_slice;

  best_y = 0;
  best_slice = G_MAXSIZE;
  can_add_slice = atlas->n_slices < MAX_SLICES_PER_ATLAS;
  if (can_add_slice)
    waste = height; /* Require less than 100% waste */
  else
    waste = G_MAXSIZE; /* Accept any slice, we can't make better ones */

  for (i = 0, y = 0; i < atlas->n_slices; y += atlas->slices[i].height, i++)
    {
      if (atlas->slices[i].height < height || ATLAS_SIZE - atlas->slices[i].width < width)
        continue;

      slice_waste = atlas->slices[i].height - height;
      if (slice_waste < waste)
        {
          waste = slice_waste;
          best_slice = i;
          best_y = y;
          if (waste == 0)
            break;
        }
    }

  if (best_slice >= i && i == atlas->n_slices)
    {
      if (!can_add_slice)
        return FALSE;

      atlas->n_slices++;
      if (atlas->n_slices == MAX_SLICES_PER_ATLAS)
        atlas->slices[i].height = ATLAS_SIZE - y;
      else
        atlas->slices[i].height = round_up_atlas_size (MAX (height, 4));
      atlas->slices[i].width = 0;
      best_y = y;
      best_slice = i;
    }

  *out_x = atlas->slices[best_slice].width;
  *out_y = best_y;

  atlas->slices[best_slice].width += width;
  g_assert (atlas->slices[best_slice].width <= ATLAS_SIZE);

  return TRUE;
}
                            
static GskGpuImage *
gsk_gpu_device_add_atlas_image (GskGpuDevice      *self,
                                gint64             timestamp,
                                gsize              width,
                                gsize              height,
                                gsize             *out_x,
                                gsize             *out_y)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  if (width > MAX_ATLAS_ITEM_SIZE || height > MAX_ATLAS_ITEM_SIZE)
    return NULL;

  if (priv->current_atlas &&
      gsk_gpu_cached_atlas_allocate (priv->current_atlas, width, height, out_x, out_y))
    {
      priv->current_atlas->entry.last_use_timestamp = timestamp;
      return priv->current_atlas->image;
    }

  priv->current_atlas = g_new (GskGpuCachedAtlas, 1);
  *priv->current_atlas = (GskGpuCachedAtlas) {
      .entry = {
          .type = GSK_GPU_CACHE_ATLAS,
          .last_use_timestamp = timestamp,
      },
      .image = GSK_GPU_DEVICE_GET_CLASS (self)->create_atlas_image (self, ATLAS_SIZE, ATLAS_SIZE),
      .n_slices = 0,
  };

  gsk_gpu_cache_entries_append (&priv->cache, (GskGpuCacheEntry *) priv->current_atlas);

  if (gsk_gpu_cached_atlas_allocate (priv->current_atlas, width, height, out_x, out_y))
    {
      priv->current_atlas->entry.last_use_timestamp = timestamp;
      return priv->current_atlas->image;
    }

  return NULL;
}

GskGpuImage *
gsk_gpu_device_lookup_texture_image (GskGpuDevice *self,
                                     GdkTexture   *texture,
                                     gint64        timestamp)
{
  GskGpuCachedTexture *cache;

  cache = gdk_texture_get_render_data (texture, self);
  if (cache)
    {
      cache->entry.last_use_timestamp = timestamp;
      return g_object_ref (cache->image);
    }

  return NULL;
}

static void
gsk_gpu_device_cache_texture_destroy_cb (gpointer data)
{
  GskGpuCachedTexture *cache = data;

  g_atomic_pointer_set (&cache->texture, NULL);
}

void
gsk_gpu_device_cache_texture_image (GskGpuDevice *self,
                                    GdkTexture   *texture,
                                    gint64        timestamp,
                                    GskGpuImage  *image)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  GskGpuCachedTexture *cache;

  cache = g_new (GskGpuCachedTexture, 1);

  *cache = (GskGpuCachedTexture) {
      .entry = {
          .type = GSK_GPU_CACHE_TEXTURE,
          .last_use_timestamp = timestamp,
      },
      .texture = texture,
      .image = g_object_ref (image)
  };

  if (gdk_texture_get_render_data (texture, self))
    gdk_texture_clear_render_data (texture);
  gdk_texture_set_render_data (texture, self, cache, gsk_gpu_device_cache_texture_destroy_cb);
  gsk_gpu_cache_entries_append (&priv->cache, (GskGpuCacheEntry *) cache);
}

GskGpuImage *
gsk_gpu_device_lookup_glyph_image (GskGpuDevice           *self,
                                   GskGpuFrame            *frame,
                                   PangoFont              *font,
                                   PangoGlyph              glyph,
                                   GskGpuGlyphLookupFlags  flags,
                                   float                   scale,
                                   graphene_rect_t        *out_bounds,
                                   graphene_point_t       *out_origin)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
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

  cache = g_hash_table_lookup (priv->glyph_cache, &lookup);
  if (cache)
    {
      cache->entry.last_use_timestamp = gsk_gpu_frame_get_timestamp (frame);
      *out_bounds = cache->bounds;
      *out_origin = cache->origin;
      return cache->image;
    }

  cache = g_new (GskGpuCachedGlyph, 1);
  pango_font_get_glyph_extents (font, glyph, &ink_rect, NULL);
  origin.x = floor (ink_rect.x * scale / PANGO_SCALE);
  origin.y = floor (ink_rect.y * scale / PANGO_SCALE);
  rect.size.width = ceil ((ink_rect.x + ink_rect.width) * scale / PANGO_SCALE) - origin.x;
  rect.size.height = ceil ((ink_rect.y + ink_rect.height) * scale / PANGO_SCALE) - origin.y;
  padding = 1;

  image = gsk_gpu_device_add_atlas_image (self,
                                          gsk_gpu_frame_get_timestamp (frame),
                                          rect.size.width + 2 * padding, rect.size.height + 2 * padding,
                                          &atlas_x, &atlas_y);
  if (image)
    {
      g_object_ref (image);
      rect.origin.x = atlas_x + padding;
      rect.origin.y = atlas_y + padding;
    }
  else
    {
      image = gsk_gpu_device_create_upload_image (self, GDK_MEMORY_DEFAULT, rect.size.width, rect.size.height),
      rect.origin.x = 0;
      rect.origin.y = 0;
      padding = 0;
    }


  *cache = (GskGpuCachedGlyph) {
      .entry = {
          .type = GSK_GPU_CACHE_GLYPH,
          .last_use_timestamp = gsk_gpu_frame_get_timestamp (frame),
      },
      .font = g_object_ref (font),
      .glyph = glyph,
      .flags = flags,
      .scale = scale,
      .bounds = rect,
      .image = image,
      .origin = GRAPHENE_POINT_INIT (- origin.x + (flags & 3) / 4.f,
                                     - origin.y + ((flags >> 2) & 3) / 4.f)
  };

  gsk_gpu_upload_glyph_op (frame,
                           cache->image,
                           font,
                           glyph,
                           &(cairo_rectangle_int_t) {
                               .x = rect.origin.x - padding,
                               .y = rect.origin.y - padding,
                               .width = rect.size.width + 2 * padding,
                               .height = rect.size.height + 2 * padding,
                           },
                           scale,
                           &GRAPHENE_POINT_INIT (cache->origin.x + 1,
                                                 cache->origin.y + 1));

  g_hash_table_insert (priv->glyph_cache, cache, cache);
  gsk_gpu_cache_entries_append (&priv->cache, (GskGpuCacheEntry *) cache);

  *out_bounds = cache->bounds;
  *out_origin = cache->origin;
  return cache->image;
}

