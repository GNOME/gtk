#include "config.h"

#include "gskgpudeviceprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdktextureprivate.h"

typedef enum
{
  GSK_GPU_CACHE_TEXTURE
} GskGpuCacheType;

typedef struct _GskGpuCachedTexture GskGpuCachedTexture;
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
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuDevice, gsk_gpu_device, G_TYPE_OBJECT)

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
