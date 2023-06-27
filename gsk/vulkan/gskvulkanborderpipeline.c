#include "config.h"

#include "gskvulkanborderpipelineprivate.h"

#include "gskroundedrectprivate.h"

#include "vulkan/resources/border.vert.h"

struct _GskVulkanBorderPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanBorderPipeline, gsk_vulkan_border_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_border_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_border_info;
}

static void
gsk_vulkan_border_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBorderPipeline *self = GSK_VULKAN_BORDER_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_border_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_border_pipeline_class_init (GskVulkanBorderPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_border_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_border_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_border_pipeline_init (GskVulkanBorderPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_border_pipeline_new (GdkVulkanContext        *context,
                                VkPipelineLayout         layout,
                                const char              *shader_name,
                                VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BORDER_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_border_pipeline_collect_vertex_data (GskVulkanBorderPipeline *pipeline,
                                                guchar                  *data,
                                                const graphene_point_t  *offset,
                                                const GskRoundedRect    *rect,
                                                const float              widths[4],
                                                const GdkRGBA            colors[4])
{
  GskVulkanBorderInstance *instance = (GskVulkanBorderInstance *) data;
  guint i;

  gsk_rounded_rect_to_float (rect, offset, instance->rect);
  for (i = 0; i < 4; i++)
    {
      instance->border_widths[i] = widths[i];
      instance->border_colors[4 * i + 0] = colors[i].red;
      instance->border_colors[4 * i + 1] = colors[i].green;
      instance->border_colors[4 * i + 2] = colors[i].blue;
      instance->border_colors[4 * i + 3] = colors[i].alpha;
    }
}

gsize
gsk_vulkan_border_pipeline_draw (GskVulkanBorderPipeline *pipeline,
                                VkCommandBuffer         command_buffer,
                                gsize                   offset,
                                gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             6 * 8, n_commands,
             0, offset);

  return n_commands;
}
