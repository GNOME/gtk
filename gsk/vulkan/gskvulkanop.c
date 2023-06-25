#include "config.h"

#include "gskvulkanopprivate.h"

void
gsk_vulkan_op_init (GskVulkanOp            *op,
                    const GskVulkanOpClass *op_class)
{
  op->op_class = op_class;
}

void
gsk_vulkan_op_finish (GskVulkanOp *op)
{
  op->op_class->finish (op);
}

void
gsk_vulkan_op_upload (GskVulkanOp           *op,
                      GskVulkanRenderPass   *pass,
                      GskVulkanRender       *render,
                      GskVulkanUploader     *uploader,
                      const graphene_rect_t *clip,
                      const graphene_vec2_t *scale)
{
  op->op_class->upload (op, pass, render, uploader, clip, scale);
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

GskVulkanPipeline *
gsk_vulkan_op_get_pipeline (GskVulkanOp *op)
{
  return op->op_class->get_pipeline (op);
}

void
gsk_vulkan_op_command (GskVulkanOp      *op,
                       GskVulkanRender  *render,
                       VkPipelineLayout  pipeline_layout,
                       VkCommandBuffer   command_buffer)
{
  op->op_class->command (op, render, pipeline_layout, command_buffer);
}

