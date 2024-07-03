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

gboolean                gsk_gpu_cache_gc                                (GskGpuCache            *self,
                                                                         gint64                  cache_timeout,
                                                                         gint64                  timestamp);
gsize                   gsk_gpu_cache_get_dead_texture_pixels           (GskGpuCache            *self);
GskGpuImage *           gsk_gpu_cache_get_atlas_image                   (GskGpuCache            *self);

GskGpuImage *           gsk_gpu_cache_lookup_texture_image              (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp);
void                    gsk_gpu_cache_cache_texture_image               (GskGpuCache            *self,
                                                                         GdkTexture             *texture,
                                                                         gint64                  timestamp,
                                                                         GskGpuImage            *image);

typedef enum
{
  GSK_GPU_GLYPH_X_OFFSET_1 = 0x1,
  GSK_GPU_GLYPH_X_OFFSET_2 = 0x2,
  GSK_GPU_GLYPH_X_OFFSET_3 = 0x3,
  GSK_GPU_GLYPH_Y_OFFSET_1 = 0x4,
  GSK_GPU_GLYPH_Y_OFFSET_2 = 0x8,
  GSK_GPU_GLYPH_Y_OFFSET_3 = 0xC
} GskGpuGlyphLookupFlags;

GskGpuImage *           gsk_gpu_cache_lookup_glyph_image                (GskGpuCache            *self,
                                                                         GskGpuFrame            *frame,
                                                                         PangoFont              *font,
                                                                         PangoGlyph              glyph,
                                                                         GskGpuGlyphLookupFlags  flags,
                                                                         float                   scale,
                                                                         graphene_rect_t        *out_bounds,
                                                                         graphene_point_t       *out_origin);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GskGpuCache, g_object_unref)

G_END_DECLS
