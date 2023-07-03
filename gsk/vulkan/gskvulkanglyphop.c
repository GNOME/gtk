#include "config.h"

#include "gskvulkanglyphopprivate.h"

#include "gskvulkanprivate.h"

#include "vulkan/resources/glyph.vert.h"

typedef struct _GskVulkanGlyphOp GskVulkanGlyphOp;

struct _GskVulkanGlyphOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;
  GdkRGBA color;

  guint32 image_descriptor;
  gsize vertex_offset;
};

static void
gsk_vulkan_glyph_op_finish (GskVulkanOp *op)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_glyph_op_upload (GskVulkanOp         *op,
                            GskVulkanRenderPass *pass,
                            GskVulkanRender     *render,
                            GskVulkanUploader   *uploader)
{
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static gsize
gsk_vulkan_glyph_op_count_vertex_data (GskVulkanOp *op,
                                       gsize        n_bytes)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_glyph_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_glyph_op_collect_vertex_data (GskVulkanOp         *op,
                                         GskVulkanRenderPass *pass,
                                         GskVulkanRender     *render,
                                         guchar              *data)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;
  GskVulkanGlyphInstance *instance = (GskVulkanGlyphInstance *) (data + self->vertex_offset);

  gsk_vulkan_rect_to_float (&self->rect, instance->rect);
  gsk_vulkan_rect_to_float (&self->tex_rect, instance->tex_rect);
  instance->tex_id = self->image_descriptor;
  gsk_vulkan_rgba_to_float (&self->color, instance->color);
}

static void
gsk_vulkan_glyph_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                             GskVulkanRender *render)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->image, GSK_VULKAN_SAMPLER_DEFAULT);
}

static void
gsk_vulkan_glyph_op_command (GskVulkanOp      *op,
                             GskVulkanRender  *render,
                             VkPipelineLayout  pipeline_layout,
                             VkCommandBuffer   command_buffer)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_glyph_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_GLYPH_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanGlyphOp),
  "glyph",
  &gsk_vulkan_glyph_info,
  gsk_vulkan_glyph_op_finish,
  gsk_vulkan_glyph_op_upload,
  gsk_vulkan_glyph_op_count_vertex_data,
  gsk_vulkan_glyph_op_collect_vertex_data,
  gsk_vulkan_glyph_op_reserve_descriptor_sets,
  gsk_vulkan_glyph_op_command
};

void
gsk_vulkan_glyph_op (GskVulkanRenderPass    *render_pass,
                     const char             *clip_type,
                     GskVulkanImage         *image,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const graphene_rect_t  *tex_rect,
                     const GdkRGBA          *color)
{
  GskVulkanGlyphOp *self;

  self = (GskVulkanGlyphOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_GLYPH_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  self->image = g_object_ref (image);
  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
  self->color = *color;
}
