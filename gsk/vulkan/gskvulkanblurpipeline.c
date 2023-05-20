#include "config.h"

#include "gskvulkanblurpipelineprivate.h"

struct _GskVulkanBlurPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanBlurInstance GskVulkanBlurInstance;

struct _GskVulkanBlurInstance
{
  float rect[4];
  float tex_rect[4];
  float blur_radius;
  guint32 tex_id;
};

G_DEFINE_TYPE (GskVulkanBlurPipeline, gsk_vulkan_blur_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_blur_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanBlurInstance),
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
          .offset = G_STRUCT_OFFSET (GskVulkanBlurInstance, tex_rect),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBlurInstance, blur_radius),
      },
      {
          .location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32_UINT,
          .offset = G_STRUCT_OFFSET (GskVulkanBlurInstance, tex_id),
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
                                              guint32                 tex_id,
                                              const graphene_point_t *offset,
                                              const graphene_rect_t  *rect,
                                              const graphene_rect_t  *tex_rect,
                                              double                  blur_radius)
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
  instance->blur_radius = blur_radius;
  instance->tex_id = tex_id;
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
