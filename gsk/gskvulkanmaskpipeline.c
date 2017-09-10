#include "config.h"

#include "gskvulkanmaskpipelineprivate.h"

struct _GskVulkanMaskPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanMaskInstance GskVulkanMaskInstance;

struct _GskVulkanMaskInstance
{
  float rect[4];
  float tex_rect[4];
  float color[4];
};

G_DEFINE_TYPE (GskVulkanMaskPipeline, gsk_vulkan_mask_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_mask_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanMaskInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanMaskInstance, rect),
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanMaskInstance, tex_rect),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanMaskInstance, color),
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
gsk_vulkan_mask_pipeline_finalize (GObject *gobject)
{
  //GskVulkanMaskPipeline *self = GSK_VULKAN_MASK_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_mask_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_mask_pipeline_class_init (GskVulkanMaskPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_mask_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_mask_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_mask_pipeline_init (GskVulkanMaskPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_mask_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new_full (GSK_TYPE_VULKAN_MASK_PIPELINE, layout, shader_name, render_pass,
                                       VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
}

gsize
gsk_vulkan_mask_pipeline_count_vertex_data (GskVulkanMaskPipeline *pipeline)
{
  return sizeof (GskVulkanMaskInstance);
}

void
gsk_vulkan_mask_pipeline_collect_vertex_data (GskVulkanMaskPipeline *pipeline,
                                               guchar                 *data,
                                               const graphene_rect_t  *rect,
                                               const GdkRGBA          *color)
{
  GskVulkanMaskInstance *instance = (GskVulkanMaskInstance *) data;

  instance->rect[0] = rect->origin.x;
  instance->rect[1] = rect->origin.y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->tex_rect[0] = 0.0;
  instance->tex_rect[1] = 0.0;
  instance->tex_rect[2] = 1.0;
  instance->tex_rect[3] = 1.0;
  instance->color[0] = color->red;
  instance->color[1] = color->green;
  instance->color[2] = color->blue;
  instance->color[3] = color->alpha;
}

gsize
gsk_vulkan_mask_pipeline_draw (GskVulkanMaskPipeline *pipeline,
                                VkCommandBuffer         command_buffer,
                                gsize                   offset,
                                gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
