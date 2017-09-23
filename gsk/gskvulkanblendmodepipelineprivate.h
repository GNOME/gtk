#ifndef __GSK_VULKAN_BLEND_MODE_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_BLEND_MODE_PIPELINE_PRIVATE_H__

#include <graphene.h>

#include "gskvulkanpipelineprivate.h"
#include "gskenums.h"

G_BEGIN_DECLS

typedef struct _GskVulkanBlendModePipelineLayout GskVulkanBlendModePipelineLayout;

#define GSK_TYPE_VULKAN_BLEND_MODE_PIPELINE (gsk_vulkan_blend_mode_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanBlendModePipeline, gsk_vulkan_blend_mode_pipeline, GSK, VULKAN_BLEND_MODE_PIPELINE, GskVulkanPipeline)

GskVulkanPipeline * gsk_vulkan_blend_mode_pipeline_new                 (GdkVulkanContext           *context,
                                                                        VkPipelineLayout            layout,
                                                                        const char                 *shader_name,
                                                                        VkRenderPass                render_pass);

gsize               gsk_vulkan_blend_mode_pipeline_count_vertex_data   (GskVulkanBlendModePipeline *pipeline);
void                gsk_vulkan_blend_mode_pipeline_collect_vertex_data (GskVulkanBlendModePipeline *pipeline,
                                                                        guchar                     *data,
                                                                        const graphene_rect_t      *bounds,
                                                                        const graphene_rect_t      *start_bounds,
                                                                        const graphene_rect_t      *end_bounds,
                                                                        GskBlendMode                blend_mode);
gsize               gsk_vulkan_blend_mode_pipeline_draw                (GskVulkanBlendModePipeline *pipeline,
                                                                        VkCommandBuffer             command_buffer,
                                                                        gsize                       offset,
                                                                        gsize                       n_commands);


G_END_DECLS

#endif /* __GSK_VULKAN_BLEND_MODE_PIPELINE_PRIVATE_H__ */
