#include "config.h"

#include "gskvulkanmaskopprivate.h"

#include "gskvulkanprivate.h"

#include "vulkan/resources/mask.vert.h"

typedef struct _GskVulkanMaskOp GskVulkanMaskOp;

struct _GskVulkanMaskOp
{
  GskVulkanOp op;

  struct {
    GskVulkanImage *image;
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } source, mask;
  GskMaskMode mask_mode;

  gsize vertex_offset;
};

static void
gsk_vulkan_mask_op_finish (GskVulkanOp *op)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;

  g_object_unref (self->source.image);
  g_object_unref (self->mask.image);
}

static void
gsk_vulkan_mask_op_upload (GskVulkanOp         *op,
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
gsk_vulkan_mask_op_count_vertex_data (GskVulkanOp *op,
                                      gsize        n_bytes)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_mask_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_mask_op_collect_vertex_data (GskVulkanOp         *op,
                                        GskVulkanRenderPass *pass,
                                        GskVulkanRender     *render,
                                        guchar              *data)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;
  GskVulkanMaskInstance *instance = (GskVulkanMaskInstance *) (data + self->vertex_offset);

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

  self->source.image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->source.image, GSK_VULKAN_SAMPLER_DEFAULT);
  self->mask.image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->mask.image, GSK_VULKAN_SAMPLER_DEFAULT);
}

static void
gsk_vulkan_mask_op_command (GskVulkanOp      *op,
                            GskVulkanRender  *render,
                            VkPipelineLayout  pipeline_layout,
                            VkCommandBuffer   command_buffer)
{
  GskVulkanMaskOp *self = (GskVulkanMaskOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_mask_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_COLOR_MASK_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanMaskOp),
  "mask",
  &gsk_vulkan_mask_info,
  gsk_vulkan_mask_op_finish,
  gsk_vulkan_mask_op_upload,
  gsk_vulkan_mask_op_count_vertex_data,
  gsk_vulkan_mask_op_collect_vertex_data,
  gsk_vulkan_mask_op_reserve_descriptor_sets,
  gsk_vulkan_mask_op_command
};

void
gsk_vulkan_mask_op (GskVulkanRenderPass    *render_pass,
                    const char             *clip_type,
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

  self = (GskVulkanMaskOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_COLOR_MASK_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  self->source.image = g_object_ref (source);
  graphene_rect_offset_r (source_rect, offset->x, offset->y, &self->source.rect);
  gsk_vulkan_normalize_tex_coords (&self->source.tex_rect, source_rect, source_tex_rect);
  self->mask.image = g_object_ref (mask);
  graphene_rect_offset_r (mask_rect, offset->x, offset->y, &self->mask.rect);
  gsk_vulkan_normalize_tex_coords (&self->mask.tex_rect, mask_rect, mask_tex_rect);
  self->mask_mode = mask_mode;
}
