#include "config.h"

#include "gskgpudeviceprivate.h"

#include "gdk/gdkdisplayprivate.h"

typedef struct _GskGpuDevicePrivate GskGpuDevicePrivate;

struct _GskGpuDevicePrivate
{
  GdkDisplay *display;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuDevice, gsk_gpu_device, G_TYPE_OBJECT)

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

  object_class->finalize = gsk_gpu_device_finalize;
}

static void
gsk_gpu_device_init (GskGpuDevice *self)
{
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

