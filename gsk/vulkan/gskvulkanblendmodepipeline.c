#include "config.h"

#include "gskvulkanblendmodepipelineprivate.h"

#include "vulkan/resources/blend-mode.vert.h"

struct _GskVulkanBlendModePipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanBlendModeInstance GskVulkanBlendModeInstance;

G_DEFINE_TYPE (GskVulkanBlendModePipeline, gsk_vulkan_blend_mode_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_blend_mode_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_blend_mode_info;
}

static void
gsk_vulkan_blend_mode_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBlendModePipeline *self = GSK_VULKAN_BLUR_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_blend_mode_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_blend_mode_pipeline_class_init (GskVulkanBlendModePipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_blend_mode_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_blend_mode_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_blend_mode_pipeline_init (GskVulkanBlendModePipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_blend_mode_pipeline_new (GdkVulkanContext        *context,
                                    VkPipelineLayout         layout,
                                    const char              *shader_name,
                                    VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BLEND_MODE_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_blend_mode_pipeline_collect_vertex_data (GskVulkanBlendModePipeline *pipeline,
                                                    guchar                     *data,
                                                    guint32                     top_tex_id[2],
                                                    guint32                     bottom_tex_id[2],
                                                    const graphene_point_t     *offset,
                                                    const graphene_rect_t      *bounds,
                                                    const graphene_rect_t      *top_bounds,
                                                    const graphene_rect_t      *bottom_bounds,
                                                    const graphene_rect_t      *top_tex_rect,
                                                    const graphene_rect_t      *bottom_tex_rect,
                                                    GskBlendMode                blend_mode)
{
  GskVulkanBlendModeInstance *instance = (GskVulkanBlendModeInstance *) data;

  instance->rect[0] = bounds->origin.x + offset->x;
  instance->rect[1] = bounds->origin.y + offset->y;
  instance->rect[2] = bounds->size.width;
  instance->rect[3] = bounds->size.height;

  instance->top_rect[0] = top_bounds->origin.x + offset->x;
  instance->top_rect[1] = top_bounds->origin.y + offset->y;
  instance->top_rect[2] = top_bounds->size.width;
  instance->top_rect[3] = top_bounds->size.height;

  instance->bottom_rect[0] = bottom_bounds->origin.x + offset->x;
  instance->bottom_rect[1] = bottom_bounds->origin.y + offset->y;
  instance->bottom_rect[2] = bottom_bounds->size.width;
  instance->bottom_rect[3] = bottom_bounds->size.height;

  instance->top_tex_rect[0] = top_tex_rect->origin.x;
  instance->top_tex_rect[1] = top_tex_rect->origin.y;
  instance->top_tex_rect[2] = top_tex_rect->size.width;
  instance->top_tex_rect[3] = top_tex_rect->size.height;

  instance->bottom_tex_rect[0] = bottom_tex_rect->origin.x;
  instance->bottom_tex_rect[1] = bottom_tex_rect->origin.y;
  instance->bottom_tex_rect[2] = bottom_tex_rect->size.width;
  instance->bottom_tex_rect[3] = bottom_tex_rect->size.height;

  instance->top_tex_id[0] = top_tex_id[0];
  instance->top_tex_id[1] = top_tex_id[1];
  instance->bottom_tex_id[0] = bottom_tex_id[0];
  instance->bottom_tex_id[1] = bottom_tex_id[1];
  instance->blend_mode = blend_mode;
}

gsize
gsk_vulkan_blend_mode_pipeline_draw (GskVulkanBlendModePipeline *pipeline,
                                     VkCommandBuffer        command_buffer,
                                     gsize                  offset,
                                     gsize                  n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
