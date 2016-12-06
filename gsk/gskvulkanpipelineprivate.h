#ifndef __GSK_VULKAN_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_PIPELINE_PRIVATE_H__

#include <gdk/gdk.h>

#include "gskdebugprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_PIPELINE (gsk_vulkan_pipeline_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanPipeline, gsk_vulkan_pipeline, GSK, VULKAN_PIPELINE, GObject)

static inline VkResult
gsk_vulkan_handle_result (VkResult    res,
                          const char *called_function)
{
  if (res != VK_SUCCESS)
    {
      GSK_NOTE (VULKAN,g_printerr ("%s(): %s (%d)\n", called_function, gdk_vulkan_strerror (res), res));
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))

GskVulkanPipeline *     gsk_vulkan_pipeline_new                         (GdkVulkanContext       *context,
                                                                         VkRenderPass            render_pass);

VkPipeline              gsk_vulkan_pipeline_get_pipeline                (GskVulkanPipeline      *self);
VkPipelineLayout        gsk_vulkan_pipeline_get_pipeline_layout         (GskVulkanPipeline      *self);
VkDescriptorSetLayout   gsk_vulkan_pipeline_get_descriptor_set_layout   (GskVulkanPipeline      *self);

G_END_DECLS

#endif /* __GSK_VULKAN_PIPELINE_PRIVATE_H__ */
