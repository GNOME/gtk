#include "config.h"

#include "gskvulkancolorpipelineprivate.h"

#include "vulkan/resources/color.vert.h"

struct _GskVulkanColorPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanColorPipeline, gsk_vulkan_color_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_color_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_color_info;
}

static void
gsk_vulkan_color_pipeline_finalize (GObject *gobject)
{
  //GskVulkanColorPipeline *self = GSK_VULKAN_COLOR_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_color_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_color_pipeline_class_init (GskVulkanColorPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_color_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_color_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_color_pipeline_init (GskVulkanColorPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_color_pipeline_new (GdkVulkanContext         *context,
                               VkPipelineLayout         layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_COLOR_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_color_pipeline_collect_vertex_data (GskVulkanColorPipeline *pipeline,
                                               guchar                 *data,
                                               const graphene_point_t *offset,
                                               const graphene_rect_t  *rect,
                                               const GdkRGBA          *color)
{
  GskVulkanColorInstance *instance = (GskVulkanColorInstance *) data;

  instance->rect[0] = rect->origin.x + offset->x;
  instance->rect[1] = rect->origin.y + offset->y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->color[0] = color->red;
  instance->color[1] = color->green;
  instance->color[2] = color->blue;
  instance->color[3] = color->alpha;
}

gsize
gsk_vulkan_color_pipeline_draw (GskVulkanColorPipeline *pipeline,
                                VkCommandBuffer         command_buffer,
                                gsize                   offset,
                                gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
