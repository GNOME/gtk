#ifndef __GSK_VULKAN_RENDER_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PRIVATE_H__

#include <gdk/gdk.h>
#include <gsk/gskrendernode.h>

#include "gsk/gskvulkanimageprivate.h"
#include "gsk/gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef enum {
  GSK_VULKAN_PIPELINE_BLEND,
  GSK_VULKAN_PIPELINE_BLEND_CLIP,
  GSK_VULKAN_PIPELINE_BLEND_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_COLOR,
  GSK_VULKAN_PIPELINE_COLOR_CLIP,
  GSK_VULKAN_PIPELINE_COLOR_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_LINEAR_GRADIENT,
  GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP,
  GSK_VULKAN_PIPELINE_LINEAR_GRADIENT_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_COLOR_MATRIX,
  GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP,
  GSK_VULKAN_PIPELINE_COLOR_MATRIX_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_BORDER,
  GSK_VULKAN_PIPELINE_BORDER_CLIP,
  GSK_VULKAN_PIPELINE_BORDER_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_INSET_SHADOW,
  GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP,
  GSK_VULKAN_PIPELINE_INSET_SHADOW_CLIP_ROUNDED,
  GSK_VULKAN_PIPELINE_OUTSET_SHADOW,
  GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP,
  GSK_VULKAN_PIPELINE_OUTSET_SHADOW_CLIP_ROUNDED,
  /* add more */
  GSK_VULKAN_N_PIPELINES
} GskVulkanPipelineType;

typedef struct _GskVulkanRender GskVulkanRender;

GskVulkanRender *       gsk_vulkan_render_new                           (GskRenderer            *renderer,
                                                                         GdkVulkanContext       *context);
void                    gsk_vulkan_render_free                          (GskVulkanRender        *self);

gboolean                gsk_vulkan_render_is_busy                       (GskVulkanRender        *self);
void                    gsk_vulkan_render_reset                         (GskVulkanRender        *self,
                                                                         GskVulkanImage         *target,
                                                                         const graphene_rect_t  *rect);

GskRenderer *           gsk_vulkan_render_get_renderer                  (GskVulkanRender        *self);

void                    gsk_vulkan_render_add_cleanup_image             (GskVulkanRender        *self,
                                                                         GskVulkanImage         *image);

void                    gsk_vulkan_render_add_node                      (GskVulkanRender        *self,
                                                                         GskRenderNode          *node);

void                    gsk_vulkan_render_upload                        (GskVulkanRender        *self);

GskVulkanPipeline *     gsk_vulkan_render_get_pipeline                  (GskVulkanRender        *self,
                                                                         GskVulkanPipelineType   pipeline_type);
VkDescriptorSet         gsk_vulkan_render_get_descriptor_set            (GskVulkanRender        *self,
                                                                         gsize                   id);
gsize                   gsk_vulkan_render_reserve_descriptor_set        (GskVulkanRender        *self,
                                                                         GskVulkanImage         *source);
void                    gsk_vulkan_render_draw                          (GskVulkanRender        *self,
                                                                         VkSampler               sampler);

void                    gsk_vulkan_render_submit                        (GskVulkanRender        *self);

GskTexture *            gsk_vulkan_render_download_target               (GskVulkanRender        *self);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PRIVATE_H__ */
