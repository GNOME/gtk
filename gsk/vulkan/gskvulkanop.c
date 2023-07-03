#include "config.h"

#include "gskvulkanopprivate.h"

GskVulkanOp *
gsk_vulkan_op_alloc (GskVulkanRenderPass    *render_pass,
                     const GskVulkanOpClass *op_class)
{
  GskVulkanOp *op;

  op = gsk_vulkan_render_pass_alloc_op (render_pass,
                                        op_class->size);
  op->op_class = op_class;

  return op;
}

void
gsk_vulkan_op_finish (GskVulkanOp *op)
{
  op->op_class->finish (op);
}

void
gsk_vulkan_op_upload (GskVulkanOp         *op,
                      GskVulkanRenderPass *pass,
                      GskVulkanRender     *render,
                      GskVulkanUploader   *uploader)
{
  op->op_class->upload (op, pass, render, uploader);
}

gsize
gsk_vulkan_op_count_vertex_data (GskVulkanOp *op,
                                 gsize        n_bytes)
{
  return op->op_class->count_vertex_data (op, n_bytes);
}

void
gsk_vulkan_op_collect_vertex_data (GskVulkanOp         *op,
                                   GskVulkanRenderPass *pass,
                                   GskVulkanRender     *render,
                                   guchar              *data)
{
  op->op_class->collect_vertex_data (op, pass, render, data);
}

void
gsk_vulkan_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                       GskVulkanRender *render)
{
  op->op_class->reserve_descriptor_sets (op, render);
}

void
gsk_vulkan_op_command (GskVulkanOp      *op,
                       GskVulkanRender  *render,
                       VkPipelineLayout  pipeline_layout,
                       VkCommandBuffer   command_buffer)
{
  op->op_class->command (op, render, pipeline_layout, command_buffer);
}

