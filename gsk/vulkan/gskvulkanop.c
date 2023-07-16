#include "config.h"

#include "gskvulkanopprivate.h"

GskVulkanOp *
gsk_vulkan_op_alloc (GskVulkanRender        *render,
                     const GskVulkanOpClass *op_class)
{
  GskVulkanOp *op;

  op = gsk_vulkan_render_alloc_op (render, op_class->size);
  op->op_class = op_class;

  return op;
}

void
gsk_vulkan_op_finish (GskVulkanOp *op)
{
  op->op_class->finish (op);
}

void
gsk_vulkan_op_print (GskVulkanOp *op,
                     GString     *string,
                     guint        indent)
{
  op->op_class->print (op, string, indent);
}

gsize
gsk_vulkan_op_count_vertex_data (GskVulkanOp *op,
                                 gsize        n_bytes)
{
  return op->op_class->count_vertex_data (op, n_bytes);
}

void
gsk_vulkan_op_collect_vertex_data (GskVulkanOp *op,
                                   guchar      *data)
{
  op->op_class->collect_vertex_data (op, data);
}

void
gsk_vulkan_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                       GskVulkanRender *render)
{
  op->op_class->reserve_descriptor_sets (op, render);
}

GskVulkanOp *
gsk_vulkan_op_command (GskVulkanOp      *op,
                       GskVulkanRender  *render,
                       VkRenderPass      render_pass,
                       VkCommandBuffer   command_buffer)
{
  return op->op_class->command (op, render, render_pass, command_buffer);
}

