#pragma once

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

void                    gsk_vulkan_blur_pipeline_collect_vertex_data   (GskVulkanBlurPipeline   *pipeline,
                                                                        guchar                  *data,
                                                                        guint32                  tex_id,
                                                                        const graphene_point_t  *offset,
                                                                        const graphene_rect_t   *rect,
                                                                        const graphene_rect_t   *tex_rect,
                                                                        double                   radius);
gsize                   gsk_vulkan_blur_pipeline_draw                  (GskVulkanBlurPipeline   *pipeline,
                                                                        VkCommandBuffer          command_buffer,
                                                                        gsize                    offset,
                                                                        gsize                    n_commands);

G_END_DECLS

