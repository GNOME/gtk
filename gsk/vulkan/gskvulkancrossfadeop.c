#include "config.h"

#include "gskvulkancrossfadeopprivate.h"

#include "vulkan/resources/cross-fade.vert.h"

typedef struct _GskVulkanCrossFadeOp GskVulkanCrossFadeOp;

struct _GskVulkanCrossFadeOp
{
  GskVulkanOp op;

  graphene_rect_t bounds;
  float progress;

  struct {
    GskVulkanImage *image;
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } start, end;

  gsize vertex_offset;
};

static void
gsk_vulkan_cross_fade_op_finish (GskVulkanOp *op)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;

  g_object_unref (self->start.image);
  g_object_unref (self->end.image);
}

static void
gsk_vulkan_cross_fade_op_upload (GskVulkanOp           *op,
                                 GskVulkanRenderPass   *pass,
                                 GskVulkanRender       *render,
                                 GskVulkanUploader     *uploader,
                                 const graphene_rect_t *clip,
                                 const graphene_vec2_t *scale)
{
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static gsize
gsk_vulkan_cross_fade_op_count_vertex_data (GskVulkanOp *op,
                                            gsize        n_bytes)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_cross_fade_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_cross_fade_op_collect_vertex_data (GskVulkanOp         *op,
                                              GskVulkanRenderPass *pass,
                                              GskVulkanRender     *render,
                                              guchar              *data)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;
  GskVulkanCrossFadeInstance *instance = (GskVulkanCrossFadeInstance *) (data + self->vertex_offset);

  gsk_vulkan_rect_to_float (&self->bounds, instance->rect);
  gsk_vulkan_rect_to_float (&self->start.rect, instance->start_rect);
  gsk_vulkan_rect_to_float (&self->end.rect, instance->end_rect);
  gsk_vulkan_rect_to_float (&self->start.tex_rect, instance->start_tex_rect);
  gsk_vulkan_rect_to_float (&self->end.tex_rect, instance->end_tex_rect);

  instance->start_tex_id = self->start.image_descriptor;
  instance->end_tex_id = self->end.image_descriptor;
  instance->progress = self->progress;
}

static void
gsk_vulkan_cross_fade_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                  GskVulkanRender *render)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;

  self->start.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                         self->start.image,
                                                                         GSK_VULKAN_SAMPLER_DEFAULT);
  self->end.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                       self->end.image,
                                                                       GSK_VULKAN_SAMPLER_DEFAULT);
}

static VkPipeline
gsk_vulkan_cross_fade_op_get_pipeline (GskVulkanOp *op)
{
  return VK_NULL_HANDLE;
}

static void
gsk_vulkan_cross_fade_op_command (GskVulkanOp      *op,
                                  GskVulkanRender  *render,
                                  VkPipelineLayout  pipeline_layout,
                                  VkCommandBuffer   command_buffer)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_cross_fade_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_CROSS_FADE_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanCrossFadeOp),
  "cross-fade",
  &gsk_vulkan_cross_fade_info,
  gsk_vulkan_cross_fade_op_finish,
  gsk_vulkan_cross_fade_op_upload,
  gsk_vulkan_cross_fade_op_count_vertex_data,
  gsk_vulkan_cross_fade_op_collect_vertex_data,
  gsk_vulkan_cross_fade_op_reserve_descriptor_sets,
  gsk_vulkan_cross_fade_op_get_pipeline,
  gsk_vulkan_cross_fade_op_command
};

void
gsk_vulkan_cross_fade_op (GskVulkanRenderPass    *render_pass,
                          const char             *clip_type,
                          const graphene_rect_t  *bounds,
                          const graphene_point_t *offset,
                          float                   progress,
                          GskVulkanImage         *start_image,
                          const graphene_rect_t  *start_rect,
                          const graphene_rect_t  *start_tex_rect,
                          GskVulkanImage         *end_image,
                          const graphene_rect_t  *end_rect,
                          const graphene_rect_t  *end_tex_rect)
{
  GskVulkanCrossFadeOp *self;

  self = (GskVulkanCrossFadeOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_CROSS_FADE_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  graphene_rect_offset_r (bounds, offset->x, offset->y, &self->bounds);
  self->progress = progress;

  self->start.image = g_object_ref (start_image);
  graphene_rect_offset_r (start_rect, offset->x, offset->y, &self->start.rect);
  gsk_vulkan_normalize_tex_coords (&self->start.tex_rect, bounds, start_tex_rect);

  self->end.image = g_object_ref (end_image);
  graphene_rect_offset_r (end_rect, offset->x, offset->y, &self->end.rect);
  gsk_vulkan_normalize_tex_coords (&self->end.tex_rect, bounds, end_tex_rect);
}
