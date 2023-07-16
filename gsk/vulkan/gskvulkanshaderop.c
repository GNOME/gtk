#include "config.h"

#include "gskvulkanshaderopprivate.h"

void
gsk_vulkan_shader_op_finish (GskVulkanOp *op)
{
  GskVulkanShaderOpClass *shader_op_class = ((GskVulkanShaderOpClass *) op->op_class);
  GskVulkanShaderOp *self = (GskVulkanShaderOp *) op;
  gsize i;

  for (i = 0; i < shader_op_class->n_images; i++)
    g_object_unref (self->images[i]);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

gsize
gsk_vulkan_shader_op_count_vertex_data (GskVulkanOp *op,
                                        gsize        n_bytes)
{
  GskVulkanShaderOp *self = (GskVulkanShaderOp *) op;
  GskVulkanShaderOpClass *shader_op_class = ((GskVulkanShaderOpClass *) op->op_class);
  gsize vertex_stride;

  vertex_stride = shader_op_class->vertex_input_state->pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

GskVulkanOp *
gsk_vulkan_shader_op_command_n (GskVulkanOp      *op,
                                GskVulkanRender  *render,
                                VkRenderPass      render_pass,
                                VkCommandBuffer   command_buffer,
                                gsize             instance_scale)
{
  GskVulkanShaderOp *self = (GskVulkanShaderOp *) op;
  GskVulkanShaderOpClass *shader_op_class = ((GskVulkanShaderOpClass *) op->op_class);
  GskVulkanOp *next;
  gsize stride = shader_op_class->vertex_input_state->pVertexBindingDescriptions[0].stride;
  gsize i;

  i = 1;
  for (next = op->next; next && i < 10 * 1000; next = next->next)
    {
      GskVulkanShaderOp *next_shader = (GskVulkanShaderOp *) next;
  
      if (next->op_class != op->op_class ||
          next_shader->vertex_offset != self->vertex_offset + i * stride)
        break;

      i++;
    }

  vkCmdBindPipeline (command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_render_get_pipeline (render,
                                                     op->op_class,
                                                     self->clip,
                                                     render_pass));

  vkCmdDraw (command_buffer,
             6 * instance_scale, i,
             0, self->vertex_offset / stride);

  return next;
}

GskVulkanOp *
gsk_vulkan_shader_op_command (GskVulkanOp      *op,
                              GskVulkanRender *render,
                              VkRenderPass      render_pass,
                              VkCommandBuffer   command_buffer)
{
  return gsk_vulkan_shader_op_command_n (op, render, render_pass, command_buffer, 1);
}

GskVulkanShaderOp *
gsk_vulkan_shader_op_alloc (GskVulkanRender              *render,
                            const GskVulkanShaderOpClass *op_class,
                            GskVulkanShaderClip           clip,
                            GskVulkanImage              **images)
{
  GskVulkanShaderOp *self;
  gsize i;

  self = (GskVulkanShaderOp *) gsk_vulkan_op_alloc (render, &op_class->parent_class);

  self->clip = clip;

  for (i = 0; i < op_class->n_images; i++)
    self->images[i] = g_object_ref (images[i]);

  return self;
}
