#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_PIPELINE (gsk_vulkan_pipeline_get_type ())

G_DECLARE_DERIVABLE_TYPE (GskVulkanPipeline, gsk_vulkan_pipeline, GSK, VULKAN_PIPELINE, GObject)

struct _GskVulkanPipelineClass
{
  GObjectClass parent_class;

  const VkPipelineVertexInputStateCreateInfo *
                                (* get_input_state_create_info)         (GskVulkanPipeline              *self);
};

GskVulkanPipeline *     gsk_vulkan_pipeline_new                         (GType                           pipeline_type,
                                                                         GdkVulkanContext               *context,
                                                                         VkPipelineLayout                layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);
VkPipeline              gsk_vulkan_pipeline_get_pipeline                (GskVulkanPipeline              *self);
gsize                   gsk_vulkan_pipeline_get_vertex_stride           (GskVulkanPipeline              *self);

G_END_DECLS

