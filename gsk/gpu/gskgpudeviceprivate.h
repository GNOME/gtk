#pragma once

#include "gskgputypesprivate.h"

G_BEGIN_DECLS

#define GSK_GPU_DEVICE_DEFAULT_TILE_SIZE 1024

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
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_atlas_image)                          (GskGpuDevice           *self,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_upload_image)                         (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         gboolean                try_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
  GskGpuImage *         (* create_download_image)                       (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
  void                  (* make_current)                                (GskGpuDevice           *self);

};

GType                   gsk_gpu_device_get_type                         (void) G_GNUC_CONST;

void                    gsk_gpu_device_setup                            (GskGpuDevice           *self,
                                                                         GdkDisplay             *display,
                                                                         gsize                   max_image_size,
                                                                         gsize                   tile_size);
void                    gsk_gpu_device_maybe_gc                         (GskGpuDevice           *self);
void                    gsk_gpu_device_queue_gc                         (GskGpuDevice           *self);
GdkDisplay *            gsk_gpu_device_get_display                      (GskGpuDevice           *self);
GskGpuCache *           gsk_gpu_device_get_cache                        (GskGpuDevice           *self);
gsize                   gsk_gpu_device_get_max_image_size               (GskGpuDevice           *self);
gsize                   gsk_gpu_device_get_tile_size                    (GskGpuDevice           *self);

GskGpuImage *           gsk_gpu_device_create_offscreen_image           (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_atlas_image               (GskGpuDevice           *self,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_upload_image              (GskGpuDevice           *self,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         gboolean                try_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_gpu_device_create_download_image            (GskGpuDevice           *self,
                                                                         GdkMemoryDepth          depth,
                                                                         gsize                   width,
                                                                         gsize                   height);
void                    gsk_gpu_device_make_current                     (GskGpuDevice           *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuDevice, g_object_unref)

G_END_DECLS
