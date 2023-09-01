#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GPU_DEVICE         (gsk_gpu_device_get_type ())
#define GSK_GPU_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_DEVICE, GskGpuDevice))
#define GSK_GPU_DEVICE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_DEVICE, GskGpuDeviceClass))
#define GSK_IS_GPU_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_DEVICE))
#define GSK_IS_GPU_DEVICE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_DEVICE))
#define GSK_GPU_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_DEVICE, GskGpuDeviceClass))

typedef struct _GskGpuDeviceClass GskGpuDeviceClass;

struct _GskGpuDevice
{
  GObject parent_instance;
};

struct _GskGpuDeviceClass
{
  GObjectClass parent_class;

  GskGpuImage *         (* create_offscreen_image)                      (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_upload_image)                         (GskGpuDevice           *self,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
};

GType                   gsk_gpu_device_get_type                         (void) G_GNUC_CONST;

void                    gsk_gpu_device_setup                            (GskGpuDevice           *self,
                                                                         GdkDisplay             *display);
void                    gsk_gpu_device_gc                               (GskGpuDevice           *self,
                                                                         gint64                  timestamp);

GdkDisplay *            gsk_gpu_device_get_display                      (GskGpuDevice           *self);

GskGpuImage *           gsk_gpu_device_create_offscreen_image           (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_upload_image              (GskGpuDevice           *self,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskGpuImage *           gsk_gpu_device_lookup_texture_image             (GskGpuDevice           *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp);
void                    gsk_gpu_device_cache_texture_image              (GskGpuDevice           *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp,
                                                                         GskGpuImage            *image);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuDevice, g_object_unref)

G_END_DECLS
