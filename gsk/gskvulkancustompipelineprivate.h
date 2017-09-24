#ifndef __GSK_VULKAN_CUSTOM_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_CUSTOM_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanCustomPipelineLayout GskVulkanCustomPipelineLayout;

#define GSK_TYPE_VULKAN_CUSTOM_PIPELINE (gsk_vulkan_custom_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanCustomPipeline, gsk_vulkan_custom_pipeline, GSK, VULKAN_CUSTOM_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_custom_pipeline_new                   (GdkVulkanContext        *context,
                                                                          VkPipelineLayout         layout,
                                                                          GBytes                  *fragment_shader,
                                                                          VkRenderPass             render_pass);

gsize                   gsk_vulkan_custom_pipeline_count_vertex_data     (GskVulkanCustomPipeline   *pipeline);
void                    gsk_vulkan_custom_pipeline_collect_vertex_data   (GskVulkanCustomPipeline   *pipeline,
                                                                          guchar                  *data,
                                                                          const graphene_rect_t   *rect,
                                                                          const graphene_rect_t   *child1_bounds,
                                                                          const graphene_rect_t   *child2_bounds,
                                                                          float                    time);
gsize                   gsk_vulkan_custom_pipeline_draw                  (GskVulkanCustomPipeline   *pipeline,
                                                                          VkCommandBuffer          command_buffer,
                                                                          gsize                    offset,
                                                                          gsize                    n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_CUSTOM_PIPELINE_PRIVATE_H__ */
