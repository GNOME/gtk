#include "config.h"

#include "gskvulkanblendpipelineprivate.h"

struct _GskVulkanBlendPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanBlendPipeline, gsk_vulkan_blend_pipeline, G_TYPE_OBJECT)

static void
gsk_vulkan_blend_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBlendPipeline *self = GSK_VULKAN_BLEND_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_blend_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_blend_pipeline_class_init (GskVulkanBlendPipelineClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_blend_pipeline_finalize;
}

static void
gsk_vulkan_blend_pipeline_init (GskVulkanBlendPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_blend_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BLEND_PIPELINE, layout, shader_name, render_pass);
}
