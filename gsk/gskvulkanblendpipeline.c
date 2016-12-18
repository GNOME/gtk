#include "config.h"

#include "gskvulkanblendpipelineprivate.h"

struct _GskVulkanBlendPipeline
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

G_DEFINE_TYPE (GskVulkanBlendPipeline, gsk_vulkan_blend_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static void
gsk_vulkan_blend_pipeline_finalize (GObject *gobject)
{
  //GskVulkanBlendPipeline *self = GSK_VULKAN_BLEND_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_blend_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_blend_pipeline_class_init (GskVulkanBlendPipelineClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_blend_pipeline_finalize;
}

static void
gsk_vulkan_blend_pipeline_init (GskVulkanBlendPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_blend_pipeline_new (GskVulkanPipelineLayout *layout,
                               const char              *shader_name,
                               VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new (GSK_TYPE_VULKAN_BLEND_PIPELINE, layout, shader_name, render_pass);
}

gsize
gsk_vulkan_blend_pipeline_count_vertex_data (GskVulkanBlendPipeline *pipeline)
{
  return sizeof (GskVulkanVertex) * 6;
}

void
gsk_vulkan_blend_pipeline_collect_vertex_data (GskVulkanBlendPipeline *pipeline,
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
gsk_vulkan_blend_pipeline_draw (GskVulkanBlendPipeline *pipeline,
                                VkCommandBuffer         command_buffer,
                                gsize                   offset,
                                gsize                   n_commands)
{
  vkCmdDraw (command_buffer,
             n_commands * 6, 1,
             offset, 0);

  return n_commands * 6;
}
