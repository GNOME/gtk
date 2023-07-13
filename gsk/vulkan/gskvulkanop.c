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
                       VkPipelineLayout  pipeline_layout,
                       VkCommandBuffer   command_buffer)
{
  return op->op_class->command (op, render, pipeline_layout, command_buffer);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

gsize
gsk_vulkan_op_draw_count_vertex_data (GskVulkanOp *op,
                                      gsize        n_bytes)
{
  gsize vertex_stride;

  vertex_stride = op->op_class->vertex_input_state->pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  op->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

GskVulkanOp *
gsk_vulkan_op_draw_command_n (GskVulkanOp      *op,
                              GskVulkanRender *render,
                              VkPipelineLayout  pipeline_layout,
                              VkCommandBuffer   command_buffer,
                              gsize             instance_scale)
{
  GskVulkanOp *next;
  gsize stride = op->op_class->vertex_input_state->pVertexBindingDescriptions[0].stride;
  gsize i;

  i = 1;
  for (next = op->next; next && i < 10 * 1000; next = next->next)
    {
      if (next->op_class != op->op_class ||
          next->vertex_offset != op->vertex_offset + i * stride)
        break;

      i++;
    }

  vkCmdDraw (command_buffer,
             6 * instance_scale, i,
             0, op->vertex_offset / stride);

  return next;
}

GskVulkanOp *
gsk_vulkan_op_draw_command (GskVulkanOp      *op,
                            GskVulkanRender *render,
                            VkPipelineLayout  pipeline_layout,
                            VkCommandBuffer   command_buffer)
{
  return gsk_vulkan_op_draw_command_n (op, render, pipeline_layout, command_buffer, 1);
}

