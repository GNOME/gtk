#include "config.h"

#include "gskvulkantextureopprivate.h"

#include "gskvulkantexturepipelineprivate.h"

typedef struct _GskVulkanTextureOp GskVulkanTextureOp;

struct _GskVulkanTextureOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskVulkanRenderSampler sampler;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;

  guint32 image_descriptor;
  guint32 sampler_descriptor;
  GskVulkanPipeline *pipeline;
  gsize vertex_offset;
};

static void
gsk_vulkan_texture_op_finish (GskVulkanOp *op)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_texture_op_upload (GskVulkanOp           *op,
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
gsk_vulkan_texture_op_count_vertex_data (GskVulkanOp *op,
                                         gsize        n_bytes)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_pipeline_get_vertex_stride (self->pipeline);
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_texture_op_collect_vertex_data (GskVulkanOp         *op,
                                           GskVulkanRenderPass *pass,
                                           GskVulkanRender     *render,
                                           guchar              *data)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  gsk_vulkan_texture_pipeline_collect_vertex_data (GSK_VULKAN_TEXTURE_PIPELINE (self->pipeline),
                                                   data + self->vertex_offset,
                                                   (guint32[2]) {
                                                    self->image_descriptor,
                                                    self->sampler_descriptor,
                                                   },
                                                   graphene_point_zero (),
                                                   &self->rect,
                                                   &self->tex_rect);
}

static void
gsk_vulkan_texture_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                               GskVulkanRender *render)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->image);
  self->sampler_descriptor = gsk_vulkan_render_get_sampler_descriptor (render, self->sampler);
}

static VkPipeline
gsk_vulkan_texture_op_get_pipeline (GskVulkanOp *op)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  return gsk_vulkan_pipeline_get_pipeline (self->pipeline);
}

static void
gsk_vulkan_texture_op_command (GskVulkanOp      *op,
                               GskVulkanRender  *render,
                               VkPipelineLayout  pipeline_layout,
                               VkCommandBuffer   command_buffer)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  gsk_vulkan_texture_pipeline_draw (GSK_VULKAN_TEXTURE_PIPELINE (self->pipeline),
                                    command_buffer,
                                    self->vertex_offset / gsk_vulkan_pipeline_get_vertex_stride (self->pipeline),
                                    1);
}

static const GskVulkanOpClass GSK_VULKAN_TEXTURE_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanTextureOp),
  gsk_vulkan_texture_op_finish,
  gsk_vulkan_texture_op_upload,
  gsk_vulkan_texture_op_count_vertex_data,
  gsk_vulkan_texture_op_collect_vertex_data,
  gsk_vulkan_texture_op_reserve_descriptor_sets,
  gsk_vulkan_texture_op_get_pipeline,
  gsk_vulkan_texture_op_command
};

gsize
gsk_vulkan_texture_op_size (void)
{
  return GSK_VULKAN_TEXTURE_OP_CLASS.size;
}

void
gsk_vulkan_texture_op_init (GskVulkanOp            *op,
                            GskVulkanPipeline      *pipeline,
                            GskVulkanImage         *image,
                            GskVulkanRenderSampler  sampler,
                            const graphene_rect_t  *rect,
                            const graphene_point_t *offset,
                            const graphene_rect_t  *tex_rect)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  gsk_vulkan_op_init (op, &GSK_VULKAN_TEXTURE_OP_CLASS);

  self->pipeline = pipeline;
  self->image = g_object_ref (image);
  self->sampler = sampler;
  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
}
