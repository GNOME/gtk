#include "config.h"

#include "gskgpudeviceprivate.h"

#include "gskgpucacheprivate.h"

#include "gdk/gdkprofilerprivate.h"

#include "gsk/gskdebugprivate.h"

#define CACHE_TIMEOUT 15  /* seconds */

typedef struct _GskGpuDevicePrivate GskGpuDevicePrivate;

struct _GskGpuDevicePrivate
{
  GdkDisplay *display;
  gsize max_image_size;
  gsize tile_size;

  GskGpuCache *cache; /* we don't own a ref, but manage the cache */
  guint cache_gc_source;
  int cache_timeout;  /* in seconds, or -1 to disable gc */
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuDevice, gsk_gpu_device, G_TYPE_OBJECT)

/* Returns TRUE if everything was GC'ed */
static gboolean
gsk_gpu_device_gc (GskGpuDevice *self,
                   gint64        timestamp)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  gint64 before G_GNUC_UNUSED = GDK_PROFILER_CURRENT_TIME;
  gboolean result;

  if (priv->cache == NULL)
    return TRUE;

  gsk_gpu_device_make_current (self);

  result = gsk_gpu_cache_gc (priv->cache,
                             priv->cache_timeout >= 0 ? priv->cache_timeout * G_TIME_SPAN_SECOND : -1,
                             timestamp);
  if (result)
    g_clear_object (&priv->cache);

  gdk_profiler_end_mark (before, "Glyph cache GC", NULL);

  return result;
}

static gboolean
cache_gc_cb (gpointer data)
{
  GskGpuDevice *self = data;
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  gint64 timestamp;
  gboolean result = G_SOURCE_CONTINUE;

  timestamp = g_get_monotonic_time ();
  GSK_DEBUG (CACHE, "Periodic GC (timestamp %lld)", (long long) timestamp);

  /* gc can collect the device if all windows are closed and only
   * the cache is keeping it alive */
  g_object_ref (self);

  if (gsk_gpu_device_gc (self, timestamp))
    {
      priv->cache_gc_source = 0;
      result = G_SOURCE_REMOVE;
    }

  g_object_unref (self);

  return result;
}

void
gsk_gpu_device_maybe_gc (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  gsize dead_texture_pixels, dead_textures;

  if (priv->cache_timeout < 0)
    return;

  if (priv->cache == NULL)
    return;

  dead_textures = gsk_gpu_cache_get_dead_textures (priv->cache);
  dead_texture_pixels = gsk_gpu_cache_get_dead_texture_pixels (priv->cache);

  if (priv->cache_timeout == 0 || dead_textures > 50 || dead_texture_pixels > 1000 * 1000)
    {
      GSK_DEBUG (CACHE, "Pre-frame GC (%" G_GSIZE_FORMAT " dead textures, %" G_GSIZE_FORMAT " dead pixels)",
                 dead_textures, dead_texture_pixels);
      gsk_gpu_device_gc (self, g_get_monotonic_time ());
    }
}

void
gsk_gpu_device_queue_gc (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  if (priv->cache_timeout > 0 && !priv->cache_gc_source)
    priv->cache_gc_source = g_timeout_add_seconds (priv->cache_timeout, cache_gc_cb, self);
}

static void
gsk_gpu_device_dispose (GObject *object)
{
  GskGpuDevice *self = GSK_GPU_DEVICE (object);
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  g_clear_handle_id (&priv->cache_gc_source, g_source_remove);

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
}
void
gsk_gpu_device_setup (GskGpuDevice *self,
                      GdkDisplay   *display,
                      gsize         max_image_size,
                      gsize         tile_size)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);
  const char *str;

  priv->display = g_object_ref (display);
  priv->max_image_size = max_image_size;
  priv->tile_size = tile_size;
  priv->cache_timeout = CACHE_TIMEOUT;

  str = g_getenv ("GSK_CACHE_TIMEOUT");
  if (str != NULL)
    {
      gint64 value;
      GError *error = NULL;

      if (!g_ascii_string_to_signed (str, 10, -1, G_MAXINT, &value, &error))
        {
          g_warning ("Failed to parse GSK_CACHE_TIMEOUT: %s", error->message);
          g_error_free (error);
        }
      else
        {
          priv->cache_timeout = (int) value;
        }
    }

  if (GSK_DEBUG_CHECK (CACHE))
    {
      if (priv->cache_timeout < 0)
        gdk_debug_message ("Cache GC disabled");
      else if (priv->cache_timeout == 0)
        gdk_debug_message ("Cache GC before every frame");
      else
        gdk_debug_message ("Cache GC timeout: %d seconds", priv->cache_timeout);
    }
}

GdkDisplay *
gsk_gpu_device_get_display (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->display;
}

GskGpuCache *
gsk_gpu_device_get_cache (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  if (G_LIKELY (priv->cache))
    return priv->cache;

  priv->cache = gsk_gpu_cache_new (self);

  return priv->cache;
}

/*<private>
 * gsk_gpu_device_get_max_image_size:
 * @self: a device
 *
 * Returns the max image size supported by this device.
 *
 * This maps to GL_MAX_TEXTURE_SIZE on GL, but Vulkan is more flexible with
 * per-format size limits, so this is an estimate and code should still handle
 * failures of image creation at smaller sizes. (Besides handling them anyway
 * in case of OOM.)
 *
 * Returns: The maximum size in pixels for creating a GskGpuImage
 **/
gsize
gsk_gpu_device_get_max_image_size (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->max_image_size;
}

/*<private>
 * gsk_gpu_device_get_tile_size:
 * @self: a device
 *
 * The suggested size for tiling images. This value will be small enough so that
 * image creation never fails due to size constraints. It should also not be too
 * large to allow efficient caching of tiles and evictions of unused tiles
 * (think of an image editor showing only a section of a large image).
 *
 * Returns: The suggested size of tiles when tiling images.
 **/
gsize
gsk_gpu_device_get_tile_size (GskGpuDevice *self)
{
  GskGpuDevicePrivate *priv = gsk_gpu_device_get_instance_private (self);

  return priv->tile_size;
}

/**
 * gsk_gpu_device_create_offscreen_image:
 * @self: the device to create the offscreen in
 * @with_mipmap: whether to allocate memory for mipmap levels
 * @format: the desired format
 * @is_srgb: if the format should be srgb
 * @width: width of the image
 * @height: height of the image
 *
 * Creates an image suitable for offscreen rendering. Note that the format
 * is a hint and the device may choose a different format if the desired
 * format is not be renderable on the device.
 * 
 * If width/height is too large or the device is out of memory, NULL may
 * be returned.
 *
 * Returns: (nullable): The created image or NULL on error.
 **/
GskGpuImage *
gsk_gpu_device_create_offscreen_image (GskGpuDevice    *self,
                                       gboolean         with_mipmap,
                                       GdkMemoryFormat  format,
                                       gboolean         is_srgb,
                                       gsize            width,
                                       gsize            height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_offscreen_image (self, with_mipmap, format, is_srgb, width, height);
}

GskGpuImage *
gsk_gpu_device_create_atlas_image (GskGpuDevice *self,
                                   gsize         width,
                                   gsize         height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_atlas_image (self, width, height);
}

GskGpuImage *
gsk_gpu_device_create_upload_image (GskGpuDevice   *self,
                                    gboolean        with_mipmap,
                                    GdkMemoryFormat format,
                                    gboolean        try_srgb,
                                    gsize           width,
                                    gsize           height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_upload_image (self, with_mipmap, format, try_srgb, width, height);
}

GskGpuImage *
gsk_gpu_device_create_download_image (GskGpuDevice   *self,
                                      GdkMemoryDepth  depth,
                                      gsize           width,
                                      gsize           height)
{
  return GSK_GPU_DEVICE_GET_CLASS (self)->create_download_image (self, depth, width, height);
}

void
gsk_gpu_device_make_current (GskGpuDevice *self)
{
  GSK_GPU_DEVICE_GET_CLASS (self)->make_current (self);
}

/* }}} */
/* vim:set foldmethod=marker: */
