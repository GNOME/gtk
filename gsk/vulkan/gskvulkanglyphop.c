#include "config.h"

#include "gskvulkanglyphopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/glyph.vert.h"

typedef struct _GskVulkanGlyphOp GskVulkanGlyphOp;

struct _GskVulkanGlyphOp
{
  GskVulkanShaderOp op;

  GskVulkanImage *image;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;
  GdkRGBA color;

  guint32 image_descriptor;
};

static void
gsk_vulkan_glyph_op_print (GskVulkanOp *op,
                           GString     *string,
                           guint        indent)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append (string, "glyph ");
  print_rgba (string, &self->color);
  print_newline (string);
}

static void
gsk_vulkan_glyph_op_collect_vertex_data (GskVulkanOp         *op,
                                         guchar              *data)
{
  GskVulkanGlyphOp *self = (GskVulkanGlyphOp *) op;
  GskVulkanGlyphInstance *instance = (GskVulkanGlyphInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

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
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, shader->images[0], GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_GLYPH_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanGlyphOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_glyph_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_glyph_op_collect_vertex_data,
    gsk_vulkan_glyph_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "glyph",
  1,
  &gsk_vulkan_glyph_info,
};

void
gsk_vulkan_glyph_op (GskVulkanRender        *render,
                     GskVulkanShaderClip     clip,
                     GskVulkanImage         *image,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const graphene_rect_t  *tex_rect,
                     const GdkRGBA          *color)
{
  GskVulkanGlyphOp *self;

  self = (GskVulkanGlyphOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_GLYPH_OP_CLASS, clip, &image);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
  self->color = *color;
}
