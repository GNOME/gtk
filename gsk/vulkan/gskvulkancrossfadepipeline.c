#include "config.h"

#include "gskvulkancrossfadepipelineprivate.h"

#include "vulkan/resources/cross-fade.vert.h"

struct _GskVulkanCrossFadePipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanCrossFadePipeline, gsk_vulkan_cross_fade_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_cross_fade_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_cross_fade_info;
}

static void
gsk_vulkan_cross_fade_pipeline_finalize (GObject *gobject)
{
  //GskVulkanCrossFadePipeline *self = GSK_VULKAN_BLUR_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_cross_fade_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_cross_fade_pipeline_class_init (GskVulkanCrossFadePipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_cross_fade_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_cross_fade_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_cross_fade_pipeline_init (GskVulkanCrossFadePipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_cross_fade_pipeline_new (GdkVulkanContext        *context,
                                    VkPipelineLayout         layout,
                                    const char              *shader_name,
                                    VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_CROSS_FADE_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_cross_fade_pipeline_collect_vertex_data (GskVulkanCrossFadePipeline *pipeline,
                                                    guchar                     *data,
                                                    guint32                     start_tex_id[2],
                                                    guint32                     end_tex_id[2],
                                                    const graphene_point_t     *offset,
                                                    const graphene_rect_t      *bounds,
                                                    const graphene_rect_t      *start_bounds,
                                                    const graphene_rect_t      *end_bounds,
                                                    const graphene_rect_t      *start_tex_rect,
                                                    const graphene_rect_t      *end_tex_rect,
                                                    double                      progress)
{
  GskVulkanCrossFadeInstance *instance = (GskVulkanCrossFadeInstance *) data;

  instance->rect[0] = bounds->origin.x + offset->x;
  instance->rect[1] = bounds->origin.y + offset->y;
  instance->rect[2] = bounds->size.width;
  instance->rect[3] = bounds->size.height;

  instance->start_rect[0] = start_bounds->origin.x + offset->x;
  instance->start_rect[1] = start_bounds->origin.y + offset->y;
  instance->start_rect[2] = start_bounds->size.width;
  instance->start_rect[3] = start_bounds->size.height;

  instance->end_rect[0] = end_bounds->origin.x + offset->x;
  instance->end_rect[1] = end_bounds->origin.y + offset->y;
  instance->end_rect[2] = end_bounds->size.width;
  instance->end_rect[3] = end_bounds->size.height;

  instance->start_tex_rect[0] = start_tex_rect->origin.x;
  instance->start_tex_rect[1] = start_tex_rect->origin.y;
  instance->start_tex_rect[2] = start_tex_rect->size.width;
  instance->start_tex_rect[3] = start_tex_rect->size.height;

  instance->end_tex_rect[0] = end_tex_rect->origin.x;
  instance->end_tex_rect[1] = end_tex_rect->origin.y;
  instance->end_tex_rect[2] = end_tex_rect->size.width;
  instance->end_tex_rect[3] = end_tex_rect->size.height;

  instance->start_tex_id[0] = start_tex_id[0];
  instance->start_tex_id[1] = start_tex_id[1];
  instance->end_tex_id[0] = end_tex_id[0];
  instance->end_tex_id[1] = end_tex_id[1];
  instance->progress = progress;
}

gsize
gsk_vulkan_cross_fade_pipeline_draw (GskVulkanCrossFadePipeline *pipeline,
                                     VkCommandBuffer        command_buffer,
                                     gsize                  offset,
                                     gsize                  n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
