#pragma once

#include "gskgputypesprivate.h"

#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_GPU_CACHE         (gsk_gpu_cache_get_type ())
#define GSK_GPU_CACHE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSK_TYPE_GPU_CACHE, GskGpuCache))
#define GSK_GPU_CACHE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSK_TYPE_GPU_CACHE, GskGpuCacheClass))
#define GSK_IS_GPU_CACHE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSK_TYPE_GPU_CACHE))
#define GSK_IS_GPU_CACHE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSK_TYPE_GPU_CACHE))
#define GSK_GPU_CACHE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSK_TYPE_GPU_CACHE, GskGpuCacheClass))

typedef struct _GskGpuCacheClass GskGpuCacheClass;

struct _GskGpuCacheClass
{
  GObjectClass parent_class;
};

GType                   gsk_gpu_cache_get_type                          (void) G_GNUC_CONST;

GskGpuCache *           gsk_gpu_cache_new                               (GskGpuDevice           *device);

GskGpuDevice *          gsk_gpu_cache_get_device                        (GskGpuCache            *self);
void                    gsk_gpu_cache_set_time                          (GskGpuCache            *self,
                                                                         gint64                  timestamp);

gboolean                gsk_gpu_cache_gc                                (GskGpuCache            *self,
                                                                         gint64                  cache_timeout,
                                                                         gint64                  timestamp);
gsize                   gsk_gpu_cache_get_dead_textures                 (GskGpuCache            *self);
gsize                   gsk_gpu_cache_get_dead_texture_pixels           (GskGpuCache            *self);
GskGpuImage *           gsk_gpu_cache_get_atlas_image                   (GskGpuCache            *self);
GskGpuImage *           gsk_gpu_cache_add_atlas_image                   (GskGpuCache            *self,
                                                                         gsize                   width,
                                                                         gsize                   height,
                                                                         gsize                  *out_x,
                                                                         gsize                  *out_y);
GskGpuCachePrivate *    gsk_gpu_cache_get_private                       (GskGpuCache            *self);

GskGpuImage *           gsk_gpu_cache_lookup_texture_image              (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         GdkColorState          *color_state);
void                    gsk_gpu_cache_cache_texture_image               (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         GskGpuImage            *image,
                                                                         GdkColorState          *color_state);
GskGpuImage *           gsk_gpu_cache_lookup_tile                       (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         guint                   lod_level,
                                                                         GskScalingFilter        lod_filter,
                                                                         gsize                   tile_id,
                                                                         GdkColorState         **out_color_state);
void                    gsk_gpu_cache_cache_tile                        (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         guint                   lod_level,
                                                                         GskScalingFilter        lod_filter,
                                                                         gsize                   tile_id,
                                                                         GskGpuImage            *image,
                                                                         GdkColorState          *color_state);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuCache, g_object_unref)

G_END_DECLS
