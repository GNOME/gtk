#include "config.h"

#include "gskvulkanboxshadowpipelineprivate.h"

#include "gskroundedrectprivate.h"

struct _GskVulkanBoxShadowPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanBoxShadowInstance GskVulkanBoxShadowInstance;

struct _GskVulkanBoxShadowInstance
{
  float outline[12];
  float color[4];
  float offset[2];
  float spread;
  float blur_radius;
};

G_DEFINE_TYPE (GskVulkanBoxShadowPipeline, gsk_vulkan_box_shadow_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_box_shadow_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanBoxShadowInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, outline),
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, outline) + 4 * sizeof (float),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, outline) + 8 * sizeof (float),
      },
      {
          .location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, color),
      },
      {
          .location = 4,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, offset),
      },
      {
          .location = 5,
          .binding = 0,
          .format = VK_FORMAT_R32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, spread),
      },
      {
          .location = 6,
          .binding = 0,
          .format = VK_FORMAT_R32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanBoxShadowInstance, blur_radius),
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
gsk_vulkan_box_shadow_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBoxShadowPipeline *self = GSK_VULKAN_BOX_SHADOW_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_box_shadow_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_box_shadow_pipeline_class_init (GskVulkanBoxShadowPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_box_shadow_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_box_shadow_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_box_shadow_pipeline_init (GskVulkanBoxShadowPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_box_shadow_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BOX_SHADOW_PIPELINE, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_box_shadow_pipeline_count_vertex_data (GskVulkanBoxShadowPipeline *pipeline)
{
  return sizeof (GskVulkanBoxShadowInstance);
}

void
gsk_vulkan_box_shadow_pipeline_collect_vertex_data (GskVulkanBoxShadowPipeline *pipeline,
                                                    guchar                     *data,
                                                    const GskRoundedRect       *outline,
                                                    const GdkRGBA              *color,
                                                    float                      dx,
                                                    float                      dy,
                                                    float                      spread,
                                                    float                      blur_radius)
{
  GskVulkanBoxShadowInstance *instance = (GskVulkanBoxShadowInstance *) data;

  gsk_rounded_rect_to_float (outline, instance->outline);
  instance->color[0] = color->red;
  instance->color[1] = color->green;
  instance->color[2] = color->blue;
  instance->color[3] = color->alpha;
  instance->offset[0] = dx;
  instance->offset[1] = dy;
  instance->spread = spread;
  instance->blur_radius = blur_radius;
}

gsize
gsk_vulkan_box_shadow_pipeline_draw (GskVulkanBoxShadowPipeline *pipeline,
                                     VkCommandBuffer         command_buffer,
                                     gsize                   offset,
                                     gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             6 * 8, n_commands,
             0, offset);

  return n_commands;
}
