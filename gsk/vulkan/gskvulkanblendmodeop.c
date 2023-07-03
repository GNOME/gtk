#include "config.h"

#include "gskvulkanblendmodeopprivate.h"

#include "gskvulkanprivate.h"

#include "vulkan/resources/blend-mode.vert.h"

typedef struct _GskVulkanBlendModeOp GskVulkanBlendModeOp;

struct _GskVulkanBlendModeOp
{
  GskVulkanOp op;

  graphene_rect_t bounds;
  GskBlendMode blend_mode;

  struct {
    GskVulkanImage *image;
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } top, bottom;

  gsize vertex_offset;
};

static void
gsk_vulkan_blend_mode_op_finish (GskVulkanOp *op)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;

  g_object_unref (self->top.image);
  g_object_unref (self->bottom.image);
}

static void
gsk_vulkan_blend_mode_op_upload (GskVulkanOp           *op,
                                 GskVulkanRenderPass   *pass,
                                 GskVulkanRender       *render,
                                 GskVulkanUploader     *uploader)
{
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static gsize
gsk_vulkan_blend_mode_op_count_vertex_data (GskVulkanOp *op,
                                            gsize        n_bytes)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_blend_mode_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_blend_mode_op_collect_vertex_data (GskVulkanOp         *op,
                                              GskVulkanRenderPass *pass,
                                              GskVulkanRender     *render,
                                              guchar              *data)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;
  GskVulkanBlendModeInstance *instance = (GskVulkanBlendModeInstance *) (data + self->vertex_offset);

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

  self->top.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                       self->top.image,
                                                                       GSK_VULKAN_SAMPLER_DEFAULT);
  self->bottom.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                          self->bottom.image,
                                                                          GSK_VULKAN_SAMPLER_DEFAULT);
}

static void
gsk_vulkan_blend_mode_op_command (GskVulkanOp      *op,
                                  GskVulkanRender  *render,
                                  VkPipelineLayout  pipeline_layout,
                                  VkCommandBuffer   command_buffer)
{
  GskVulkanBlendModeOp *self = (GskVulkanBlendModeOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_blend_mode_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanShaderOpClass GSK_VULKAN_BLEND_MODE_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanBlendModeOp),
  "blend-mode",
  &gsk_vulkan_blend_mode_info,
  gsk_vulkan_blend_mode_op_finish,
  gsk_vulkan_blend_mode_op_upload,
  gsk_vulkan_blend_mode_op_count_vertex_data,
  gsk_vulkan_blend_mode_op_collect_vertex_data,
  gsk_vulkan_blend_mode_op_reserve_descriptor_sets,
  gsk_vulkan_blend_mode_op_command
};

void
gsk_vulkan_blend_mode_op (GskVulkanRenderPass    *render_pass,
                          const char             *clip_type,
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

  self = (GskVulkanBlendModeOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_BLEND_MODE_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  graphene_rect_offset_r (bounds, offset->x, offset->y, &self->bounds);
  self->blend_mode = blend_mode;

  self->top.image = g_object_ref (top_image);
  graphene_rect_offset_r (top_rect, offset->x, offset->y, &self->top.rect);
  gsk_vulkan_normalize_tex_coords (&self->top.tex_rect, bounds, top_tex_rect);

  self->bottom.image = g_object_ref (bottom_image);
  graphene_rect_offset_r (bottom_rect, offset->x, offset->y, &self->bottom.rect);
  gsk_vulkan_normalize_tex_coords (&self->bottom.tex_rect, bounds, bottom_tex_rect);
}
