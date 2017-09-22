#ifndef __GSK_VULKAN_CROSS_FADE_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_CROSS_FADE_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanCrossFadePipelineLayout GskVulkanCrossFadePipelineLayout;

#define GSK_TYPE_VULKAN_CROSS_FADE_PIPELINE (gsk_vulkan_cross_fade_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanCrossFadePipeline, gsk_vulkan_cross_fade_pipeline, GSK, VULKAN_CROSS_FADE_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline * gsk_vulkan_cross_fade_pipeline_new                 (GdkVulkanContext           *context,
                                                                        VkPipelineLayout            layout,
                                                                        const char                 *shader_name,
                                                                        VkRenderPass                render_pass);

gsize               gsk_vulkan_cross_fade_pipeline_count_vertex_data   (GskVulkanCrossFadePipeline *pipeline);
void                gsk_vulkan_cross_fade_pipeline_collect_vertex_data (GskVulkanCrossFadePipeline *pipeline,
                                                                        guchar                     *data,
                                                                        const graphene_rect_t      *bounds,
                                                                        const graphene_rect_t      *start_bounds,
                                                                        const graphene_rect_t      *end_bounds,
                                                                        double                      progress);
gsize               gsk_vulkan_cross_fade_pipeline_draw                (GskVulkanCrossFadePipeline *pipeline,
                                                                        VkCommandBuffer             command_buffer,
                                                                        gsize                       offset,
                                                                        gsize                       n_commands);

G_END_DECLS

#endif /* __GSK_VULKAN_CROSS_FADE_PIPELINE_PRIVATE_H__ */
