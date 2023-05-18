#pragma once

#include <gdk/gdk.h>

#include "gskdebugprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_PIPELINE (gsk_vulkan_pipeline_get_type ())

G_DECLARE_DERIVABLE_TYPE (GskVulkanPipeline, gsk_vulkan_pipeline, GSK, VULKAN_PIPELINE, GObject)

struct _GskVulkanPipelineClass
{
  GObjectClass parent_class;

  const VkPipelineVertexInputStateCreateInfo *
                                (* get_input_state_create_info)         (GskVulkanPipeline              *self);
};

static inline VkResult
gsk_vulkan_handle_result (VkResult    res,
                          const char *called_function)
{
  if (res != VK_SUCCESS)
    {
      GSK_DEBUG (VULKAN, "%s(): %s (%d)", called_function, gdk_vulkan_strerror (res), res);
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))

GskVulkanPipeline *     gsk_vulkan_pipeline_new                         (GType                           pipeline_type,
                                                                         GdkVulkanContext               *context,
                                                                         VkPipelineLayout                layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);
VkPipeline              gsk_vulkan_pipeline_get_pipeline                (GskVulkanPipeline              *self);
gsize                   gsk_vulkan_pipeline_get_vertex_stride           (GskVulkanPipeline              *self);

G_END_DECLS

