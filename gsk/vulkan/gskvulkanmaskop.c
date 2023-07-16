#include "config.h"

#include "gskvulkanmaskopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/mask.vert.h"

typedef struct _GskVulkanMaskOp GskVulkanMaskOp;

struct _GskVulkanMaskOp
{
  GskVulkanShaderOp op;

  struct {
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } source, mask;
  GskMaskMode mask_mode;
};

static void
gsk_vulkan_mask_op_print (GskVulkanOp *op,
                          GString     *string,
                          guint        indent)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->source.rect);
  g_string_append (string, "mask ");
  print_rect (string, &self->mask.rect);
  switch (self->mask_mode)
  {
    case GSK_MASK_MODE_ALPHA:
      g_string_append (string, "alpha ");
      break;
    case GSK_MASK_MODE_INVERTED_ALPHA:
      g_string_append (string, "inverted-alpha ");
      break;
    case GSK_MASK_MODE_LUMINANCE:
      g_string_append (string, "luminance ");
      break;
    case GSK_MASK_MODE_INVERTED_LUMINANCE:
      g_string_append (string, "inverted-luminance ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  print_newline (string);
}

static void
gsk_vulkan_mask_op_collect_vertex_data (GskVulkanOp *op,
                                        guchar      *data)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;
  GskVulkanMaskInstance *instance = (GskVulkanMaskInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->source.rect, instance->source_rect);
  gsk_vulkan_rect_to_float (&self->source.tex_rect, instance->source_tex_rect);
  instance->source_id = self->source.image_descriptor;
  gsk_vulkan_rect_to_float (&self->mask.rect, instance->mask_rect);
  gsk_vulkan_rect_to_float (&self->mask.tex_rect, instance->mask_tex_rect);
  instance->mask_id = self->mask.image_descriptor;
  instance->mask_mode = self->mask_mode;
}

static void
gsk_vulkan_mask_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                            GskVulkanRender *render)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->source.image_descriptor = gsk_vulkan_render_get_image_descriptor (render, shader->images[0], GSK_VULKAN_SAMPLER_DEFAULT);
  self->mask.image_descriptor = gsk_vulkan_render_get_image_descriptor (render, shader->images[1], GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_COLOR_MASK_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanMaskOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_mask_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_mask_op_collect_vertex_data,
    gsk_vulkan_mask_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "mask",
  2,
  &gsk_vulkan_mask_info,
};

void
gsk_vulkan_mask_op (GskVulkanRender        *render,
                    GskVulkanShaderClip     clip,
                    const graphene_point_t *offset,
                    GskVulkanImage         *source,
                    const graphene_rect_t  *source_rect,
                    const graphene_rect_t  *source_tex_rect,
                    GskVulkanImage         *mask,
                    const graphene_rect_t  *mask_rect,
                    const graphene_rect_t  *mask_tex_rect,
                    GskMaskMode             mask_mode)
{
  GskVulkanMaskOp *self;

  self = (GskVulkanMaskOp *) gsk_vulkan_shader_op_alloc (render,
                                                         &GSK_VULKAN_COLOR_MASK_OP_CLASS,
                                                         clip,
                                                         (GskVulkanImage *[2]) {
                                                             source,
                                                             mask,
                                                         });

  graphene_rect_offset_r (source_rect, offset->x, offset->y, &self->source.rect);
  gsk_vulkan_normalize_tex_coords (&self->source.tex_rect, source_rect, source_tex_rect);
  graphene_rect_offset_r (mask_rect, offset->x, offset->y, &self->mask.rect);
  gsk_vulkan_normalize_tex_coords (&self->mask.tex_rect, mask_rect, mask_tex_rect);
  self->mask_mode = mask_mode;
}
