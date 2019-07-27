#include "config.h"

#include "gskvulkancolortextpipelineprivate.h"

struct _GskVulkanColorTextPipeline
{
  GObject parent_instance;
};

typedef struct _GskVulkanColorTextInstance GskVulkanColorTextInstance;

struct _GskVulkanColorTextInstance
{
  float rect[4];
  float tex_rect[4];
};

G_DEFINE_TYPE (GskVulkanColorTextPipeline, gsk_vulkan_color_text_pipeline, GSK_TYPE_VULKAN_PIPELINE)

static const VkPipelineVertexInputStateCreateInfo *
gsk_vulkan_color_text_pipeline_get_input_state_create_info (GskVulkanPipeline *self)
{
  static const VkVertexInputBindingDescription vertexBindingDescriptions[] = {
      {
          .binding = 0,
          .stride = sizeof (GskVulkanColorTextInstance),
          .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
      }
  };
  static const VkVertexInputAttributeDescription vertexInputAttributeDescription[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanColorTextInstance, rect),
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = G_STRUCT_OFFSET (GskVulkanColorTextInstance, tex_rect),
      },
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
gsk_vulkan_color_text_pipeline_finalize (GObject *gobject)
{
  //GskVulkanColorTextPipeline *self = GSK_VULKAN_COLOR_TEXT_PIPELINE (gobject);

  G_OBJECT_CLASS (gsk_vulkan_color_text_pipeline_parent_class)->finalize (gobject);
}

static void
gsk_vulkan_color_text_pipeline_class_init (GskVulkanColorTextPipelineClass *klass)
{
  GskVulkanPipelineClass *pipeline_class = GSK_VULKAN_PIPELINE_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_color_text_pipeline_finalize;

  pipeline_class->get_input_state_create_info = gsk_vulkan_color_text_pipeline_get_input_state_create_info;
}

static void
gsk_vulkan_color_text_pipeline_init (GskVulkanColorTextPipeline *self)
{
}

GskVulkanPipeline *
gsk_vulkan_color_text_pipeline_new (GdkVulkanContext        *context,
                                    VkPipelineLayout         layout,
                                    const char              *shader_name,
                                    VkRenderPass             render_pass)
{
  return gsk_vulkan_pipeline_new_full (GSK_TYPE_VULKAN_COLOR_TEXT_PIPELINE, context, layout, shader_name, render_pass,
                                       VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
}

gsize
gsk_vulkan_color_text_pipeline_count_vertex_data (GskVulkanColorTextPipeline *pipeline,
                                                  int                         num_instances)
{
  return sizeof (GskVulkanColorTextInstance) * num_instances;
}

void
gsk_vulkan_color_text_pipeline_collect_vertex_data (GskVulkanColorTextPipeline *pipeline,
                                                    guchar                     *data,
                                                    GskVulkanRenderer          *renderer,
                                                    const graphene_rect_t      *rect,
                                                    PangoFont                  *font,
                                                    guint                       total_glyphs,
                                                    const PangoGlyphInfo       *glyphs,
                                                    const graphene_point_t     *offset,
                                                    guint                       start_glyph,
                                                    guint                       num_glyphs,
                                                    float                       scale)
{
  GskVulkanColorTextInstance *instances = (GskVulkanColorTextInstance *) data;
  int i;
  int count = 0;
  int x_position = 0;

  for (i = 0; i < start_glyph; i++)
    x_position += glyphs[i].geometry.width;

  for (; i < total_glyphs && count < num_glyphs; i++)
    {
      const PangoGlyphInfo *gi = &glyphs[i];

      if (gi->glyph != PANGO_GLYPH_EMPTY)
        {
          double cx = (x_position + gi->geometry.x_offset) / PANGO_SCALE;
          double cy = gi->geometry.y_offset / PANGO_SCALE;
          GskVulkanColorTextInstance *instance = &instances[count];
          GskVulkanCachedGlyph *glyph;

          glyph = gsk_vulkan_renderer_get_cached_glyph (renderer,
                                                        font,
                                                        gi->glyph,
                                                        x_position + gi->geometry.x_offset,
                                                        gi->geometry.y_offset,
                                                        scale);

          instance->tex_rect[0] = glyph->tx;
          instance->tex_rect[1] = glyph->ty;
          instance->tex_rect[2] = glyph->tw;
          instance->tex_rect[3] = glyph->th;

          instance->rect[0] = offset->x + cx + glyph->draw_x;
          instance->rect[1] = offset->y + cy + glyph->draw_y;
          instance->rect[2] = glyph->draw_width;
          instance->rect[3] = glyph->draw_height;

          count++;
       }
     x_position += gi->geometry.width;
   }
}

gsize
gsk_vulkan_color_text_pipeline_draw (GskVulkanColorTextPipeline *pipeline,
                                     VkCommandBuffer             command_buffer,
                                     gsize                       offset,
                                     gsize                       n_commands)
{
  vkCmdDraw (command_buffer,
             6, n_commands,
             0, offset);

  return n_commands;
}
