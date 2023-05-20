#pragma once

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

void                gsk_vulkan_cross_fade_pipeline_collect_vertex_data (GskVulkanCrossFadePipeline *pipeline,
                                                                        guchar                     *data,
                                                                        guint32                     start_tex_id,
                                                                        guint32                     end_tex_id,
                                                                        const graphene_point_t     *offset,
                                                                        const graphene_rect_t      *bounds,
                                                                        const graphene_rect_t      *start_bounds,
                                                                        const graphene_rect_t      *end_bounds,
                                                                        const graphene_rect_t      *start_tex_rect,
                                                                        const graphene_rect_t      *end_tex_rect,
                                                                        double                      progress);
gsize               gsk_vulkan_cross_fade_pipeline_draw                (GskVulkanCrossFadePipeline *pipeline,
                                                                        VkCommandBuffer             command_buffer,
                                                                        gsize                       offset,
                                                                        gsize                       n_commands);

G_END_DECLS

