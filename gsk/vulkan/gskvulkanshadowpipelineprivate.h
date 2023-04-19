#pragma once

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanShadowPipelineLayout GskVulkanShadowPipelineLayout;

#define GSK_TYPE_VULKAN_SHADOW_PIPELINE (gsk_vulkan_shadow_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanShadowPipeline, gsk_vulkan_shadow_pipeline, GSK, VULKAN_SHADOW_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline *     gsk_vulkan_shadow_pipeline_new                  (GdkVulkanContext               *context,
                                                                         VkPipelineLayout                layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

gsize                   gsk_vulkan_shadow_pipeline_count_vertex_data    (GskVulkanShadowPipeline        *pipeline);
void                    gsk_vulkan_shadow_pipeline_collect_vertex_data  (GskVulkanShadowPipeline        *pipeline,
                                                                         guchar                         *data,
                                                                         const GskShadow                *shadow);
gsize                   gsk_vulkan_shadow_pipeline_draw                 (GskVulkanShadowPipeline        *pipeline,
                                                                         VkCommandBuffer                 command_buffer,
                                                                         gsize                           offset,
                                                                         gsize                           n_commands);

G_END_DECLS

