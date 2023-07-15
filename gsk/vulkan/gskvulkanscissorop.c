#include "config.h"

#include "gskvulkanscissoropprivate.h"

#include "gskvulkanprivate.h"

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
gsk_vulkan_scissor_op_print (GskVulkanOp *op,
                             GString     *string,
                             guint        indent)
{
  GskVulkanScissorOp *self = (GskVulkanScissorOp *) op;

  print_indent (string, indent);
  print_int_rect (string, &self->rect);
  g_string_append_printf (string, "scissor ");
  print_newline (string);
}

static gsize
gsk_vulkan_scissor_op_count_vertex_data (GskVulkanOp *op,
                                         gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_scissor_op_collect_vertex_data (GskVulkanOp *op,
                                           guchar      *data)
{
}

static void
gsk_vulkan_scissor_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                               GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_scissor_op_command (GskVulkanOp      *op,
                               GskVulkanRender  *render,
                               VkRenderPass      render_pass,
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

  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_SCISSOR_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanScissorOp),
  GSK_VULKAN_STAGE_COMMAND,
  gsk_vulkan_scissor_op_finish,
  gsk_vulkan_scissor_op_print,
  gsk_vulkan_scissor_op_count_vertex_data,
  gsk_vulkan_scissor_op_collect_vertex_data,
  gsk_vulkan_scissor_op_reserve_descriptor_sets,
  gsk_vulkan_scissor_op_command
};

void
gsk_vulkan_scissor_op (GskVulkanRender             *render,
                       const cairo_rectangle_int_t *rect)
{
  GskVulkanScissorOp *self;

  self = (GskVulkanScissorOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_SCISSOR_OP_CLASS);

  self->rect = *rect;
}
