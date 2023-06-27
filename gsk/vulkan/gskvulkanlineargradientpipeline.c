#include "config.h"

#include "gskvulkanlineargradientpipelineprivate.h"

#include "vulkan/resources/linear.vert.h"

struct _GskVulkanLinearGradientPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanLinearGradientPipeline, gsk_vulkan_linear_gradient_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_linear_gradient_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_linear_info;
}

static void
gsk_vulkan_linear_gradient_pipeline_finalize (GObject *gobject)
{
  //GskVulkanLinearGradientPipeline *self = GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_linear_gradient_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_linear_gradient_pipeline_class_init (GskVulkanLinearGradientPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_linear_gradient_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_linear_gradient_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_linear_gradient_pipeline_init (GskVulkanLinearGradientPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_linear_gradient_pipeline_new (GdkVulkanContext        *context,
                                         VkPipelineLayout         layout,
                                         const char              *shader_name,
                                         VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_LINEAR_GRADIENT_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_linear_gradient_pipeline_collect_vertex_data (GskVulkanLinearGradientPipeline *pipeline,
                                                         guchar                          *data,
                                                         const graphene_point_t          *offset,
                                                         const graphene_rect_t           *rect,
                                                         const graphene_point_t          *start,
                                                         const graphene_point_t          *end,
                                                         gboolean                         repeating,
                                                         gsize                            gradient_offset,
                                                         gsize                            n_stops)
{
  GskVulkanLinearInstance *instance = (GskVulkanLinearInstance *) data;

  instance->rect[0] = rect->origin.x + offset->x;
  instance->rect[1] = rect->origin.y + offset->y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->start[0] = start->x + offset->x;
  instance->start[1] = start->y + offset->y;
  instance->end[0] = end->x + offset->x;
  instance->end[1] = end->y + offset->y;
  instance->repeating = repeating;
  instance->stop_offset = gradient_offset;
  instance->stop_count = n_stops;
}

gsize
gsk_vulkan_linear_gradient_pipeline_draw (GskVulkanLinearGradientPipeline *pipeline,
                                   VkCommandBuffer            command_buffer,
                                   gsize                      offset,
                                   gsize                      n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
