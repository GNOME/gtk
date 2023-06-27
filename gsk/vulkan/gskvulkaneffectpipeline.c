#include "config.h"

#include "gskvulkaneffectpipelineprivate.h"

#include "vulkan/resources/color-matrix.vert.h"

struct _GskVulkanEffectPipeline
{
  GObject parent_instance;
};

G_DEFINE_TYPE (GskVulkanEffectPipeline, gsk_vulkan_effect_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_effect_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  return &gsk_vulkan_color_matrix_info;
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
gsk_vulkan_effect_pipeline_new (GdkVulkanContext        *context,
                                VkPipelineLayout         layout,
                                const char              *shader_name,
                                VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_EFFECT_PIPELINE, context, layout, shader_name, render_pass);
}

void
gsk_vulkan_effect_pipeline_collect_vertex_data (GskVulkanEffectPipeline *pipeline,
                                                guchar                  *data,
                                                guint32                  tex_id[2],
                                                const graphene_point_t  *offset,
                                                const graphene_rect_t   *rect,
                                                const graphene_rect_t   *tex_rect,
                                                const graphene_matrix_t *color_matrix,
                                                const graphene_vec4_t   *color_offset)
{
  GskVulkanColorMatrixInstance *instance = (GskVulkanColorMatrixInstance *) data;

  instance->rect[0] = rect->origin.x + offset->x;
  instance->rect[1] = rect->origin.y + offset->y;
  instance->rect[2] = rect->size.width;
  instance->rect[3] = rect->size.height;
  instance->tex_rect[0] = tex_rect->origin.x;
  instance->tex_rect[1] = tex_rect->origin.y;
  instance->tex_rect[2] = tex_rect->size.width;
  instance->tex_rect[3] = tex_rect->size.height;
  graphene_matrix_to_float (color_matrix, instance->color_matrix);
  graphene_vec4_to_float (color_offset, instance->color_offset);
  instance->tex_id[0] = tex_id[0];
  instance->tex_id[1] = tex_id[1];
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
