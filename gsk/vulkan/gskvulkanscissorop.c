#include "config.h"

#include "gskvulkanscissoropprivate.h"

typedef struct _GskVulkanScissorOp GskVulkanScissorOp;

struct _GskVulkanScissorOp
{
  GskVulkanOp op;

  cairo_rectangle_int_t rect;
};

static void
gsk_vulkan_scissor_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_scissor_op_upload (GskVulkanOp           *op,
                              GskVulkanRenderPass   *pass,
                              GskVulkanRender       *render,
                              GskVulkanUploader     *uploader,
                              const graphene_rect_t *clip,
                              const graphene_vec2_t *scale)
{
}

static gsize
gsk_vulkan_scissor_op_count_vertex_data (GskVulkanOp *op,
                                         gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_scissor_op_collect_vertex_data (GskVulkanOp         *op,
                                           GskVulkanRenderPass *pass,
                                           GskVulkanRender     *render,
                                           guchar              *data)
{
}

static void
gsk_vulkan_scissor_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                               GskVulkanRender *render)
{
}

static VkPipeline
gsk_vulkan_scissor_op_get_pipeline (GskVulkanOp *op)
{
  return NULL;
}

static void
gsk_vulkan_scissor_op_command (GskVulkanOp      *op,
                               GskVulkanRender  *render,
                               VkPipelineLayout  pipeline_layout,
                               VkCommandBuffer   command_buffer)
{
  GskVulkanScissorOp *self = (GskVulkanScissorOp *) op;

  vkCmdSetScissor (command_buffer,
                   0,
                   1,
                   &(VkRect2D) {
                     { self->rect.x, self->rect.y },
                     { self->rect.width, self->rect.height },
                   });
}

static const GskVulkanOpClass GSK_VULKAN_SCISSOR_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanScissorOp),
  gsk_vulkan_scissor_op_finish,
  gsk_vulkan_scissor_op_upload,
  gsk_vulkan_scissor_op_count_vertex_data,
  gsk_vulkan_scissor_op_collect_vertex_data,
  gsk_vulkan_scissor_op_reserve_descriptor_sets,
  gsk_vulkan_scissor_op_get_pipeline,
  gsk_vulkan_scissor_op_command
};

void
gsk_vulkan_scissor_op (GskVulkanRenderPass         *render_pass,
                       const cairo_rectangle_int_t *rect)
{
  GskVulkanScissorOp *self;

  self = (GskVulkanScissorOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_SCISSOR_OP_CLASS);

  self->rect = *rect;
}
