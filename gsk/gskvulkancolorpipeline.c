#include "config.h"

#include "gskvulkancolorpipelineprivate.h"

struct _GskVulkanColorPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanVertex GskVulkanVertex;

struct _GskVulkanVertex
{
  float x;
  float y;
  float tex_x;
  float tex_y;
};

G_DEFINE_TYPE (GskVulkanColorPipeline, gsk_vulkan_color_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_color_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = 4 * sizeof (float),
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = 0,
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = 2 * sizeof (float),
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
gsk_vulkan_color_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_COLOR_PIPELINE, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_color_pipeline_count_vertex_data (GskVulkanColorPipeline *pipeline)
{
  return sizeof (GskVulkanVertex) * 6;
}

void
gsk_vulkan_color_pipeline_collect_vertex_data (GskVulkanColorPipeline *pipeline,
                                               guchar                 *data,
                                               const graphene_rect_t  *rect)
{
  GskVulkanVertex *vertices = (GskVulkanVertex *) data;

  vertices[0] = (GskVulkanVertex) { rect->origin.x,                    rect->origin.y,                     0.0, 0.0 };
  vertices[1] = (GskVulkanVertex) { rect->origin.x + rect->size.width, rect->origin.y,                     1.0, 0.0 };
  vertices[2] = (GskVulkanVertex) { rect->origin.x,                    rect->origin.y + rect->size.height, 0.0, 1.0 };
  vertices[3] = (GskVulkanVertex) { rect->origin.x,                    rect->origin.y + rect->size.height, 0.0, 1.0 };
  vertices[4] = (GskVulkanVertex) { rect->origin.x + rect->size.width, rect->origin.y,                     1.0, 0.0 };
  vertices[5] = (GskVulkanVertex) { rect->origin.x + rect->size.width, rect->origin.y + rect->size.height, 1.0, 1.0 };
}

gsize
gsk_vulkan_color_pipeline_draw (GskVulkanColorPipeline *pipeline,
                                VkCommandBuffer         command_buffer,
                                gsize                   offset,
                                gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             n_commands * 6, 1,
             offset, 0);

  return n_commands * 6;
}
