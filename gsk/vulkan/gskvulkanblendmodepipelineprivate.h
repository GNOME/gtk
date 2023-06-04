#pragma once

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

void                gsk_vulkan_blend_mode_pipeline_collect_vertex_data (GskVulkanBlendModePipeline *pipeline,
                                                                        guchar                     *data,
                                                                        guint32                     top_tex_id[2],
                                                                        guint32                     bottom_tex_id[2],
                                                                        const graphene_point_t     *offset,
                                                                        const graphene_rect_t      *bounds,
                                                                        const graphene_rect_t      *top_bounds,
                                                                        const graphene_rect_t      *bottom_bounds,
                                                                        const graphene_rect_t      *top_tex_rect,
                                                                        const graphene_rect_t      *bottom_tex_rect,
                                                                        GskBlendMode                blend_mode);
gsize               gsk_vulkan_blend_mode_pipeline_draw                (GskVulkanBlendModePipeline *pipeline,
                                                                        VkCommandBuffer             command_buffer,
                                                                        gsize                       offset,
                                                                        gsize                       n_commands);


G_END_DECLS

