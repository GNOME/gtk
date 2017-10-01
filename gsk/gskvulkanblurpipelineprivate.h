#ifndef __GSK_VULKAN_BLUR_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_BLUR_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanBlurPipelineLayout GskVulkanBlurPipelineLayout;

#define GSK_TYPE_VULKAN_BLUR_PIPELINE (gsk_vulkan_blur_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanBlurPipeline, gsk_vulkan_blur_pipeline, GSK, VULKAN_BLUR_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_blur_pipeline_new                   (GdkVulkanContext        *context,
                                                                        VkPipelineLayout         layout,
                                                                        const char              *shader_name,
                                                                        VkRenderPass             render_pass);

gsize                   gsk_vulkan_blur_pipeline_count_vertex_data     (GskVulkanBlurPipeline   *pipeline);
void                    gsk_vulkan_blur_pipeline_collect_vertex_data   (GskVulkanBlurPipeline   *pipeline,
                                                                        guchar                  *data,
                                                                        const graphene_rect_t   *rect,
                                                                        const graphene_rect_t   *tex_rect,
                                                                        double                   radius);
gsize                   gsk_vulkan_blur_pipeline_draw                  (GskVulkanBlurPipeline   *pipeline,
                                                                        VkCommandBuffer          command_buffer,
                                                                        gsize                    offset,
                                                                        gsize                    n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_BLUR_PIPELINE_PRIVATE_H__ */
