#include "config.h"

#include "gskvulkanboxshadowpipelineprivate.h"

#include "vulkan/resources/inset-shadow.vert.h"

#include "gskroundedrectprivate.h"

struct _GskVulkanBoxShadowPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanBoxShadowPipeline, gsk_vulkan_box_shadow_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_box_shadow_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_inset_shadow_info;
}

static void
gsk_vulkan_box_shadow_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBoxShadowPipeline *self = GSK_VULKAN_BOX_SHADOW_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_box_shadow_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_box_shadow_pipeline_class_init (GskVulkanBoxShadowPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_box_shadow_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_box_shadow_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_box_shadow_pipeline_init (GskVulkanBoxShadowPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_box_shadow_pipeline_new (GdkVulkanContext        *context,
                                    VkPipelineLayout         layout,
                                    const char              *shader_name,
                                    VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BOX_SHADOW_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_box_shadow_pipeline_collect_vertex_data (GskVulkanBoxShadowPipeline *pipeline,
                                                    guchar                     *data,
                                                    const graphene_point_t     *offset,
                                                    const GskRoundedRect       *outline,
                                                    const GdkRGBA              *color,
                                                    float                      dx,
                                                    float                      dy,
                                                    float                      spread,
                                                    float                      blur_radius)
{
  GskVulkanInsetShadowInstance *instance = (GskVulkanInsetShadowInstance *) data;

  gsk_rounded_rect_to_float (outline, offset, instance->outline);
  instance->color[0] = color->red;
  instance->color[1] = color->green;
  instance->color[2] = color->blue;
  instance->color[3] = color->alpha;
  instance->offset[0] = dx;
  instance->offset[1] = dy;
  instance->spread = spread;
  instance->blur_radius = blur_radius;
}

gsize
gsk_vulkan_box_shadow_pipeline_draw (GskVulkanBoxShadowPipeline *pipeline,
                                     VkCommandBuffer         command_buffer,
                                     gsize                   offset,
                                     gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             6 * 8, n_commands,
             0, offset);

  return n_commands;
}
