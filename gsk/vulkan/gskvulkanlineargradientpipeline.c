#include "config.h"

#include "gskvulkanlineargradientpipelineprivate.h"

struct _GskVulkanLinearGradientPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanLinearGradientInstance GskVulkanLinearGradientInstance;

struct _GskVulkanLinearGradientInstance
{
  float rect[4];
  float start[2];
  float end[2];
  gint32 repeating;
  gint32 stop_count;
  float offsets[GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS];
  float colors[GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS][4];
};

G_DEFINE_TYPE (GskVulkanLinearGradientPipeline, gsk_vulkan_linear_gradient_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_linear_gradient_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanLinearGradientInstance),
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
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, start),
      },
      {
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, end),
      },
      {
          .location = 3,
          .binding = 0,
          .format = VK_FORMAT_R32_SINT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, repeating),
      },
      {
          .location = 4,
          .binding = 0,
          .format = VK_FORMAT_R32_SINT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, stop_count),
      },
      {
          .location = 5,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, offsets),
      },
      {
          .location = 6,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, offsets) + sizeof (float) * 4,
      },
      {
          .location = 7,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[0]),
      },
      {
          .location = 8,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[1]),
      },
      {
          .location = 9,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[2]),
      },
      {
          .location = 10,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[3]),
      },
      {
          .location = 11,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[4]),
      },
      {
          .location = 12,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[5]),
      },
      {
          .location = 13,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[6]),
      },
      {
          .location = 14,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanLinearGradientInstance, colors[7]),
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
gsk_vulkan_linear_gradient_pipeline_finalize (GObject *gobject)
{
  //GskVulkanLinearGradientPipeline *self = GSK_VULKAN_LINEAR_GRADIENT_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_linear_gradient_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_linear_gradient_pipeline_class_init (GskVulkanLinearGradientPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_linear_gradient_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_linear_gradient_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_linear_gradient_pipeline_init (GskVulkanLinearGradientPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_linear_gradient_pipeline_new (GdkVulkanContext        *context,
                                         VkPipelineLayout         layout,
                                         const char              *shader_name,
                                         VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_LINEAR_GRADIENT_PIPELINE, context, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_linear_gradient_pipeline_count_vertex_data (GskVulkanLinearGradientPipeline *pipeline)
{
  return sizeof (GskVulkanLinearGradientInstance);
}

void
gsk_vulkan_linear_gradient_pipeline_collect_vertex_data (GskVulkanLinearGradientPipeline *pipeline,
                                                         guchar                    *data,
                                                         const graphene_rect_t     *rect,
                                                         const graphene_point_t    *start,
                                                         const graphene_point_t    *end,
                                                         gboolean                   repeating,
                                                         gsize                      n_stops,
                                                         const GskColorStop        *stops)
{
  GskVulkanLinearGradientInstance *instance = (GskVulkanLinearGradientInstance *) data;
  gsize i;

  if (n_stops > GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS)
    {
      g_warning ("Only %u color stops supported.", GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS);
      n_stops = GSK_VULKAN_LINEAR_GRADIENT_PIPELINE_MAX_COLOR_STOPS;
    }
  instance->rect[0] = rect->origin.x;
  instance->rect[1] = rect->origin.y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->start[0] = start->x;
  instance->start[1] = start->y;
  instance->end[0] = end->x;
  instance->end[1] = end->y;
  instance->repeating = repeating;
  instance->stop_count = n_stops;
  for (i = 0; i < n_stops; i++)
    {
      instance->offsets[i] = stops[i].offset;
      instance->colors[i][0] = stops[i].color.red;
      instance->colors[i][1] = stops[i].color.green;
      instance->colors[i][2] = stops[i].color.blue;
      instance->colors[i][3] = stops[i].color.alpha;
    }
}

gsize
gsk_vulkan_linear_gradient_pipeline_draw (GskVulkanLinearGradientPipeline *pipeline,
                                   VkCommandBuffer            command_buffer,
                                   gsize                      offset,
                                   gsize                      n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
