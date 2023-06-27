#include "config.h"

#include "gskvulkanblurpipelineprivate.h"

#include "vulkan/resources/blur.vert.h"

struct _GskVulkanBlurPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanBlurPipeline, gsk_vulkan_blur_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_blur_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_blur_info;
}

static void
gsk_vulkan_blur_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBlurPipeline *self = GSK_VULKAN_BLUR_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_blur_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_blur_pipeline_class_init (GskVulkanBlurPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_blur_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_blur_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_blur_pipeline_init (GskVulkanBlurPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_blur_pipeline_new (GdkVulkanContext        *context,
                              VkPipelineLayout         layout,
                              const char              *shader_name,
                              VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BLUR_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_blur_pipeline_collect_vertex_data (GskVulkanBlurPipeline  *pipeline,
                                              guchar                 *data,
                                              guint32                 tex_id[2],
                                              const graphene_point_t *offset,
                                              const graphene_rect_t  *rect,
                                              const graphene_rect_t  *tex_rect,
                                              double                  radius)
{
  GskVulkanBlurInstance *instance = (GskVulkanBlurInstance *) data;

  instance->rect[0] = rect->origin.x + offset->x;
  instance->rect[1] = rect->origin.y + offset->y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->tex_rect[0] = tex_rect->origin.x;
  instance->tex_rect[1] = tex_rect->origin.y;
  instance->tex_rect[2] = tex_rect->size.width;
  instance->tex_rect[3] = tex_rect->size.height;
  instance->radius = radius;
  instance->tex_id[0] = tex_id[0];
  instance->tex_id[1] = tex_id[1];
}

gsize
gsk_vulkan_blur_pipeline_draw (GskVulkanBlurPipeline *pipeline,
                               VkCommandBuffer        command_buffer,
                               gsize                  offset,
                               gsize                  n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
