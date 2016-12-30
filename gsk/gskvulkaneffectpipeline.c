#include "config.h"

#include "gskvulkaneffectpipelineprivate.h"

struct _GskVulkanEffectPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanEffectInstance GskVulkanEffectInstance;

struct _GskVulkanEffectInstance
{
  float rect[4];
  float tex_rect[4];
  float value;
};

G_DEFINE_TYPE (GskVulkanEffectPipeline, gsk_vulkan_effect_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_effect_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanEffectInstance),
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
          .offset = G_STRUCT_OFFSET (GskVulkanEffectInstance, tex_rect),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanEffectInstance, value),
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
gsk_vulkan_effect_pipeline_finalize (GObject *gobject)
{
  //GskVulkanEffectPipeline *self = GSK_VULKAN_EFFECT_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_effect_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_effect_pipeline_class_init (GskVulkanEffectPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_effect_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_effect_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_effect_pipeline_init (GskVulkanEffectPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_effect_pipeline_new (GskVulkanPipelineLayout *layout,
                                const char              *shader_name,
                                VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_EFFECT_PIPELINE, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_effect_pipeline_count_vertex_data (GskVulkanEffectPipeline *pipeline)
{
  return sizeof (GskVulkanEffectInstance);
}

void
gsk_vulkan_effect_pipeline_collect_vertex_data (GskVulkanEffectPipeline *pipeline,
                                                guchar                  *data,
                                                const graphene_rect_t   *rect,
                                                float                    value)
{
  GskVulkanEffectInstance *instance = (GskVulkanEffectInstance *) data;

  instance->rect[0] = rect->origin.x;
  instance->rect[1] = rect->origin.y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->tex_rect[0] = 0.0;
  instance->tex_rect[1] = 0.0;
  instance->tex_rect[2] = 1.0;
  instance->tex_rect[3] = 1.0;
  instance->value = value;
}

gsize
gsk_vulkan_effect_pipeline_draw (GskVulkanEffectPipeline *pipeline,
                                VkCommandBuffer           command_buffer,
                                gsize                     offset,
                                gsize                     n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
