#include "config.h"

#include "gskgpudeviceprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdktextureprivate.h"

typedef enum
{
  GSK_GPU_CACHE_TEXTURE,
  GSK_GPU_CACHE_GLYPH
} GskGpuCacheType;

typedef struct _GskGpuCachedTexture GskGpuCachedTexture;
typedef struct _GskGpuCachedGlyph GskGpuCachedGlyph;
typedef struct _GskGpuCacheEntry GskGpuCacheEntry;

struct _GskGpuCacheEntry
{
  GskGpuCacheType type;
  guint64 last_use_timestamp;
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

  GskGpuCacheEntries cache;
  guint cache_gc_source;
  GHashTable *glyph_cache;
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
                      GdkDisplay   *display)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  priv->display = g_object_ref (display);
}

GdkDisplay *
gsk_gpu_device_get_display (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->display;
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
  rect.origin.x = floor (ink_rect.x * scale / PANGO_SCALE) - 1;
  rect.origin.y = floor (ink_rect.y * scale / PANGO_SCALE) - 1;
  rect.size.width = ceil ((ink_rect.x + ink_rect.width) * scale / PANGO_SCALE) - rect.origin.x + 2;
  rect.size.height = ceil ((ink_rect.y + ink_rect.height) * scale / PANGO_SCALE) - rect.origin.y + 2;

  *cache = (GskGpuCachedGlyph) {
      .entry = {
          .type = GSK_GPU_CACHE_GLYPH,
          .last_use_timestamp = gsk_gpu_frame_get_timestamp (frame),
      },
      .font = g_object_ref (font),
      .glyph = glyph,
      .flags = flags,
      .scale = scale,
      .image = gsk_gpu_device_create_upload_image (self, GDK_MEMORY_DEFAULT, rect.size.width, rect.size.height),
      .bounds = GRAPHENE_RECT_INIT (0, 0, rect.size.width, rect.size.height),
      .origin = GRAPHENE_POINT_INIT (-rect.origin.x + (flags & 3) / 4.f,
                                     -rect.origin.y + ((flags >> 2) & 3) / 4.f)
  };

  gsk_gpu_upload_glyph_op (frame,
                           cache->image,
                           font,
                           glyph,
                           &(cairo_rectangle_int_t) {
                               .x = 0,
                               .y = 0,
                               .width = rect.size.width,
                               .height = rect.size.height
                           },
                           scale,
                           &cache->origin);

  g_hash_table_insert (priv->glyph_cache, cache, cache);
  gsk_gpu_cache_entries_append (&priv->cache, (GskGpuCacheEntry *) cache);

  *out_bounds = cache->bounds;
  *out_origin = cache->origin;
  return cache->image;
}

