#include "config.h"

#include "gskd3d12deviceprivate.h"
#include "gskd3d12imageprivate.h"

#include "gdk/win32/gdkd3d12contextprivate.h"
#include "gdk/win32/gdkd3d12texture.h"
#include "gdk/win32/gdkdisplay-win32.h"

struct _GskD3d12Device
{
  GskGpuDevice parent_instance;

  ID3D12Device *device;
};

struct _GskD3d12DeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskD3d12Device, gsk_d3d12_device, GSK_TYPE_GPU_DEVICE)

static GskGpuImage *
gsk_d3d12_device_create_offscreen_image (GskGpuDevice   *device,
                                         gboolean        with_mipmap,
                                         GdkMemoryFormat format,
                                         gboolean        is_srgb,
                                         gsize           width,
                                         gsize           height)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (device);

  g_warn_if_fail (with_mipmap == FALSE);

  return gsk_d3d12_image_new (self->device,
                              format,
                              width,
                              height,
                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                              0,
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
}

static GskGpuImage *
gsk_d3d12_device_create_atlas_image (GskGpuDevice *device,
                                     gsize         width,
                                     gsize         height)
{
  g_return_val_if_reached (NULL);
}

static GskGpuImage *
gsk_d3d12_device_create_upload_image (GskGpuDevice     *device,
                                      gboolean          with_mipmap,
                                      GdkMemoryFormat   format,
                                      GskGpuConversion  conv,
                                      gsize             width,
                                      gsize             height)
{
  g_return_val_if_reached (NULL);
}

static GskGpuImage *
gsk_d3d12_device_create_download_image (GskGpuDevice   *device,
                                        GdkMemoryDepth  depth,
                                        gsize           width,
                                        gsize           height)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (device);

  return gsk_d3d12_image_new (self->device,
                              gdk_memory_depth_get_format (depth),
                              width,
                              height,
                              D3D12_RESOURCE_STATE_RENDER_TARGET,
                              D3D12_HEAP_FLAG_SHARED,
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
}

static void
gsk_d3d12_device_make_current (GskGpuDevice *device)
{
}

static void
gsk_d3d12_device_finalize (GObject *object)
{
  GskD3d12Device *self = GSK_D3D12_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  GdkDisplay *display;

  display = gsk_gpu_device_get_display (device);
  g_object_steal_data (G_OBJECT (display), "-gsk-d3d12-device");

  gdk_win32_com_clear (&self->device);

  G_OBJECT_CLASS (gsk_d3d12_device_parent_class)->finalize (object);
}

static void
gsk_d3d12_device_class_init (GskD3d12DeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_d3d12_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_d3d12_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_d3d12_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_d3d12_device_create_download_image;
  gpu_device_class->make_current = gsk_d3d12_device_make_current;

  object_class->finalize = gsk_d3d12_device_finalize;
}

static void
gsk_d3d12_device_init (GskD3d12Device *self)
{
}

static void
gsk_d3d12_device_setup (GskD3d12Device *self,
                        GdkDisplay     *display)
{
  self->device = gdk_win32_display_get_d3d12_device (GDK_WIN32_DISPLAY (display));
  ID3D12Device_AddRef (self->device);

  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION,
                        GSK_GPU_DEVICE_DEFAULT_TILE_SIZE,
                        /* not used */ 1);
}

GskGpuDevice *
gsk_d3d12_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  GskD3d12Device *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-d3d12-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_win32_display_get_d3d12_device (GDK_WIN32_DISPLAY (display)))
    {
      if (!gdk_has_feature (GDK_FEATURE_D3D12))
        g_set_error (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE, "D3D12 disabled via GDK_DISABLE");
      else
        g_set_error (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE, "D3D12 is not available");

      return NULL;
    }
  self = g_object_new (GSK_TYPE_D3D12_DEVICE, NULL);

  gsk_d3d12_device_setup (self, display);

  g_object_set_data (G_OBJECT (display), "-gsk-d3d12-device", self);

  return GSK_GPU_DEVICE (self);
}

ID3D12Device *
gsk_d3d12_device_get_d3d12_device (GskD3d12Device *self)
{
  return self->device;
}