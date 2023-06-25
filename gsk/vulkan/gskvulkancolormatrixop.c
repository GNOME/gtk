#include "config.h"

#include "gskvulkancolormatrixopprivate.h"

#include "gskvulkaneffectpipelineprivate.h"

typedef struct _GskVulkanColorMatrixOp GskVulkanColorMatrixOp;

struct _GskVulkanColorMatrixOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;

  guint32 image_descriptor;
  guint32 sampler_descriptor;
  GskVulkanPipeline *pipeline;
  gsize vertex_offset;
};

static void
gsk_vulkan_color_matrix_op_finish (GskVulkanOp *op)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_color_matrix_op_upload (GskVulkanOp           *op,
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
gsk_vulkan_color_matrix_op_count_vertex_data (GskVulkanOp *op,
                                              gsize        n_bytes)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_pipeline_get_vertex_stride (self->pipeline);
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_color_matrix_op_collect_vertex_data (GskVulkanOp         *op,
                                           GskVulkanRenderPass *pass,
                                           GskVulkanRender     *render,
                                           guchar              *data)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  gsk_vulkan_effect_pipeline_collect_vertex_data (GSK_VULKAN_EFFECT_PIPELINE (self->pipeline),
                                                  data + self->vertex_offset,
                                                  (guint32[2]) {
                                                   self->image_descriptor,
                                                   self->sampler_descriptor,
                                                  },
                                                  graphene_point_zero (),
                                                  &self->rect,
                                                  &self->tex_rect,
                                                  &self->color_matrix,
                                                  &self->color_offset);
}

static void
gsk_vulkan_color_matrix_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                    GskVulkanRender *render)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->image);
  self->sampler_descriptor = gsk_vulkan_render_get_sampler_descriptor (render, GSK_VULKAN_SAMPLER_DEFAULT);
}

static GskVulkanPipeline *
gsk_vulkan_color_matrix_op_get_pipeline (GskVulkanOp *op)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  return self->pipeline;
}

static void
gsk_vulkan_color_matrix_op_command (GskVulkanOp      *op,
                                    GskVulkanRender *render,
                                    VkPipelineLayout  pipeline_layout,
                                    VkCommandBuffer   command_buffer)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  gsk_vulkan_effect_pipeline_draw (GSK_VULKAN_EFFECT_PIPELINE (self->pipeline),
                                   command_buffer,
                                   self->vertex_offset / gsk_vulkan_pipeline_get_vertex_stride (self->pipeline),
                                   1);
}

static const GskVulkanOpClass GSK_VULKAN_COLOR_MATRIX_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanColorMatrixOp),
  gsk_vulkan_color_matrix_op_finish,
  gsk_vulkan_color_matrix_op_upload,
  gsk_vulkan_color_matrix_op_count_vertex_data,
  gsk_vulkan_color_matrix_op_collect_vertex_data,
  gsk_vulkan_color_matrix_op_reserve_descriptor_sets,
  gsk_vulkan_color_matrix_op_get_pipeline,
  gsk_vulkan_color_matrix_op_command
};

gsize
gsk_vulkan_color_matrix_op_size (void)
{
  return GSK_VULKAN_COLOR_MATRIX_OP_CLASS.size;
}

void
gsk_vulkan_color_matrix_op_init (GskVulkanOp             *op,
                                 GskVulkanPipeline       *pipeline,
                                 GskVulkanImage          *image,
                                 const graphene_rect_t   *rect,
                                 const graphene_point_t  *offset,
                                 const graphene_rect_t   *tex_rect,
                                 const graphene_matrix_t *color_matrix,
                                 const graphene_vec4_t   *color_offset)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  gsk_vulkan_op_init (op, &GSK_VULKAN_COLOR_MATRIX_OP_CLASS);

  self->pipeline = pipeline;
  self->image = g_object_ref (image);
  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
  self->color_matrix = *color_matrix;
  self->color_offset = *color_offset;
}
