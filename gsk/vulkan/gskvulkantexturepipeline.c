#include "config.h"

#include "gskvulkantexturepipelineprivate.h"

struct _GskVulkanTexturePipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanTextureInstance GskVulkanTextureInstance;

struct _GskVulkanTextureInstance
{
  float rect[4];
  float tex_rect[4];
};

G_DEFINE_TYPE (GskVulkanTexturePipeline, gsk_vulkan_texture_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_texture_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanTextureInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanTextureInstance, rect),
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanTextureInstance, tex_rect),
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
gsk_vulkan_texture_pipeline_finalize (GObject *gobject)
{
  //GskVulkanTexturePipeline *self = GSK_VULKAN_TEXTURE_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_texture_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_texture_pipeline_class_init (GskVulkanTexturePipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_texture_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_texture_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_texture_pipeline_init (GskVulkanTexturePipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_texture_pipeline_new (GdkVulkanContext *context,
                                 VkPipelineLayout  layout,
                                 const char       *shader_name,
                                 VkRenderPass      render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_TEXTURE_PIPELINE, context, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_texture_pipeline_count_vertex_data (GskVulkanTexturePipeline *pipeline)
{
  return sizeof (GskVulkanTextureInstance);
}

void
gsk_vulkan_texture_pipeline_collect_vertex_data (GskVulkanTexturePipeline *pipeline,
                                                 guchar                   *data,
                                                 const graphene_rect_t    *rect,
                                                 const graphene_rect_t    *tex_rect)
{
  GskVulkanTextureInstance *instance = (GskVulkanTextureInstance *) data;

  instance->rect[0] = rect->origin.x;
  instance->rect[1] = rect->origin.y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->tex_rect[0] = tex_rect->origin.x;
  instance->tex_rect[1] = tex_rect->origin.y;
  instance->tex_rect[2] = tex_rect->size.width;
  instance->tex_rect[3] = tex_rect->size.height;
}

gsize
gsk_vulkan_texture_pipeline_draw (GskVulkanTexturePipeline *pipeline,
                                  VkCommandBuffer           command_buffer,
                                  gsize                     offset,
                                  gsize                     n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
