#ifndef __GSK_VULKAN_RENDERER_PRIVATE_H__
#define __GSK_VULKAN_RENDERER_PRIVATE_H__

#include "gskvulkanrenderer.h"
#include "gskvulkanimageprivate.h"

G_BEGIN_DECLS

GskVulkanImage *        gsk_vulkan_renderer_ref_texture_image           (GskVulkanRenderer      *self,
                                                                         GdkTexture             *texture,
                                                                         GskVulkanUploader      *uploader);

typedef struct
{
  guint texture_index;

  float tx;
  float ty;
  float tw;
  float th;

  int draw_x;
  int draw_y;
  int draw_width;
  int draw_height;

  guint64 timestamp;
} GskVulkanCachedGlyph;

guint                  gsk_vulkan_renderer_cache_glyph      (GskVulkanRenderer *renderer,
                                                             PangoFont         *font,
                                                             PangoGlyph         glyph,
                                                             int                x,
                                                             int                y,
                                                             float              scale);

GskVulkanImage *       gsk_vulkan_renderer_ref_glyph_image  (GskVulkanRenderer *self,
                                                             GskVulkanUploader *uploader,
                                                             guint              index);

GskVulkanCachedGlyph * gsk_vulkan_renderer_get_cached_glyph (GskVulkanRenderer *self,
                                                             PangoFont         *font,
                                                             PangoGlyph         glyph,
                                                             int                x,
                                                             int                y,
                                                             float              scale);


G_END_DECLS

#endif /* __GSK_VULKAN_RENDERER_PRIVATE_H__ */
