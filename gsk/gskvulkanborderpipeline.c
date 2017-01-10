#include "config.h"

#include "gskvulkanborderpipelineprivate.h"

#include "gskroundedrectprivate.h"

struct _GskVulkanBorderPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanBorderInstance GskVulkanBorderInstance;

struct _GskVulkanBorderInstance
{
  float rect[12];
  float widths[4];
  float colors[16];
};

G_DEFINE_TYPE (GskVulkanBorderPipeline, gsk_vulkan_border_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_border_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanBorderInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, rect),
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, rect) + 4 * sizeof (float),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, rect) + 8 * sizeof (float),
      },
      {
          .location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, widths),
      },
      {
          .location = 4,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, colors),
      },
      {
          .location = 5,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, colors) + 4 * sizeof (float),
      },
      {
          .location = 6,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, colors) + 8 * sizeof (float),
      },
      {
          .location = 7,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBorderInstance, colors) + 12 * sizeof (float),
      }
  };
  static const VkPipelineVertexInputStateCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = G_N_ELEMENTS (vertexBindingDescriptions),
      .pVertexBindingDescriptions = vertexBindingDescriptions,
      .vertexAttributeDescriptionCount = G_N_ELEMENTS (vertexInputAttributeDescription),
      .pVertexAttributeDescriptions = vertexInputAttributeDescription
  };

  return &info;
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
gsk_vulkan_border_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BORDER_PIPELINE, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_border_pipeline_count_vertex_data (GskVulkanBorderPipeline *pipeline)
{
  return sizeof (GskVulkanBorderInstance);
}

void
gsk_vulkan_border_pipeline_collect_vertex_data (GskVulkanBorderPipeline *pipeline,
                                                guchar                  *data,
                                                const GskRoundedRect    *rect,
                                                const float              widths[4],
                                                const GdkRGBA            colors[4])
{
  GskVulkanBorderInstance *instance = (GskVulkanBorderInstance *) data;
  guint i;

  gsk_rounded_rect_to_float (rect, instance->rect);
  for (i = 0; i < 4; i++)
    {
      instance->widths[i] = widths[i];
      instance->colors[4 * i + 0] = colors[i].red;
      instance->colors[4 * i + 1] = colors[i].green;
      instance->colors[4 * i + 2] = colors[i].blue;
      instance->colors[4 * i + 3] = colors[i].alpha;
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
