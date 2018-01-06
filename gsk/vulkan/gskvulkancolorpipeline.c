#include "config.h"

#include "gskvulkancolorpipelineprivate.h"

struct _GskVulkanColorPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanColorInstance GskVulkanColorInstance;

struct _GskVulkanColorInstance
{
  float rect[4];
  float color[4];
};

G_DEFINE_TYPE (GskVulkanColorPipeline, gsk_vulkan_color_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_color_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanColorInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 0,
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanColorInstance, color),
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

gsize
gsk_vulkan_color_pipeline_count_vertex_data (GskVulkanColorPipeline *pipeline)
{
  return sizeof (GskVulkanColorInstance);
}

void
gsk_vulkan_color_pipeline_collect_vertex_data (GskVulkanColorPipeline *pipeline,
                                               guchar                 *data,
                                               const graphene_rect_t  *rect,
                                               const GdkRGBA          *color)
{
  GskVulkanColorInstance *instance = (GskVulkanColorInstance *) data;

  instance->rect[0] = rect->origin.x;
  instance->rect[1] = rect->origin.y;
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
