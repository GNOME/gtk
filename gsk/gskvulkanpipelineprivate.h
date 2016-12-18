#ifndef __GSK_VULKAN_PIPELINE_PRIVATE_H__
#define __GSK_VULKAN_PIPELINE_PRIVATE_H__

#include <gdk/gdk.h>

#include "gskdebugprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanPipelineLayout GskVulkanPipelineLayout;

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
      GSK_NOTE (VULKAN,g_printerr ("%s(): %s (%d)\n", called_function, gdk_vulkan_strerror (res), res));
    }
  return res;
}

#define GSK_VK_CHECK(func, ...) gsk_vulkan_handle_result (func (__VA_ARGS__), G_STRINGIFY (func))

GskVulkanPipelineLayout *       gsk_vulkan_pipeline_layout_new          (GdkVulkanContext               *context);
GskVulkanPipelineLayout *       gsk_vulkan_pipeline_layout_ref          (GskVulkanPipelineLayout        *self);
void                            gsk_vulkan_pipeline_layout_unref        (GskVulkanPipelineLayout        *self);

VkPipelineLayout                gsk_vulkan_pipeline_layout_get_pipeline_layout
                                                                        (GskVulkanPipelineLayout        *self);
VkDescriptorSetLayout           gsk_vulkan_pipeline_layout_get_descriptor_set_layout
                                                                        (GskVulkanPipelineLayout        *self);


GskVulkanPipeline *     gsk_vulkan_pipeline_new                         (GType                           pipeline_type,
                                                                         GskVulkanPipelineLayout        *layout,
                                                                         const char                     *shader_name,
                                                                         VkRenderPass                    render_pass);

VkPipeline              gsk_vulkan_pipeline_get_pipeline                (GskVulkanPipeline              *self);

G_END_DECLS

#endif /* __GSK_VULKAN_PIPELINE_PRIVATE_H__ */
