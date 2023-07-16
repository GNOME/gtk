#include "config.h"

#include "gskvulkanblendmodeopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/blend-mode.vert.h"

typedef struct _GskVulkanBlendModeOp GskVulkanBlendModeOp;

struct _GskVulkanBlendModeOp
{
  GskVulkanShaderOp op;

  graphene_rect_t bounds;
  GskBlendMode blend_mode;

  struct {
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } top, bottom;
};

static void
gsk_vulkan_blend_mode_op_print (GskVulkanOp *op,
                                GString     *string,
                                guint        indent)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->bounds);
  g_string_append_printf (string, "blend-mode %d%% ", self->blend_mode);
  print_newline (string);
}

static void
gsk_vulkan_blend_mode_op_collect_vertex_data (GskVulkanOp *op,
                                              guchar      *data)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;
  GskVulkanBlendModeInstance *instance = (GskVulkanBlendModeInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->bounds, instance->rect);
  gsk_vulkan_rect_to_float (&self->top.rect, instance->top_rect);
  gsk_vulkan_rect_to_float (&self->bottom.rect, instance->bottom_rect);
  gsk_vulkan_rect_to_float (&self->top.tex_rect, instance->top_tex_rect);
  gsk_vulkan_rect_to_float (&self->bottom.tex_rect, instance->bottom_tex_rect);

  instance->top_tex_id = self->top.image_descriptor;
  instance->bottom_tex_id = self->bottom.image_descriptor;
  instance->blend_mode = self->blend_mode;
}

static void
gsk_vulkan_blend_mode_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                  GskVulkanRender *render)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->top.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                       shader->images[0],
                                                                       GSK_VULKAN_SAMPLER_DEFAULT);
  self->bottom.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                          shader->images[1],
                                                                          GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_BLEND_MODE_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanBlendModeOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_blend_mode_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_blend_mode_op_collect_vertex_data,
    gsk_vulkan_blend_mode_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "blend-mode",
  2,
  &gsk_vulkan_blend_mode_info,
};

void
gsk_vulkan_blend_mode_op (GskVulkanRender        *render,
                          GskVulkanShaderClip     clip,
                          const graphene_rect_t  *bounds,
                          const graphene_point_t *offset,
                          GskBlendMode            blend_mode,
                          GskVulkanImage         *top_image,
                          const graphene_rect_t  *top_rect,
                          const graphene_rect_t  *top_tex_rect,
                          GskVulkanImage         *bottom_image,
                          const graphene_rect_t  *bottom_rect,
                          const graphene_rect_t  *bottom_tex_rect)
{
  GskVulkanBlendModeOp *self;

  self = (GskVulkanBlendModeOp *) gsk_vulkan_shader_op_alloc (render,
                                                              &GSK_VULKAN_BLEND_MODE_OP_CLASS,
                                                              clip,
                                                              (GskVulkanImage *[2]) {
                                                                  top_image,
                                                                  bottom_image
                                                              });

  graphene_rect_offset_r (bounds, offset->x, offset->y, &self->bounds);
  self->blend_mode = blend_mode;

  graphene_rect_offset_r (top_rect, offset->x, offset->y, &self->top.rect);
  gsk_vulkan_normalize_tex_coords (&self->top.tex_rect, bounds, top_tex_rect);

  graphene_rect_offset_r (bottom_rect, offset->x, offset->y, &self->bottom.rect);
  gsk_vulkan_normalize_tex_coords (&self->bottom.tex_rect, bounds, bottom_tex_rect);
}
