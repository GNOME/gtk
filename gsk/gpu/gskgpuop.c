#include "config.h"

#include "gskgpuopprivate.h"

#include "gskgpuframeprivate.h"

GskGpuOp *
gsk_gpu_op_alloc (GskGpuFrame         *frame,
                  const GskGpuOpClass *op_class)
{
  GskGpuOp *op;

  op = gsk_gpu_frame_alloc_op (frame, op_class->size);
  op->op_class = op_class;

  return op;
}

void
gsk_gpu_op_finish (GskGpuOp *op)
{
  op->op_class->finish (op);
}

void
gsk_gpu_op_print (GskGpuOp    *op,
                  GskGpuFrame *frame,
                  GString     *string,
                  guint        indent)
{
  op->op_class->print (op, frame, string, indent);
}

#ifdef GDK_RENDERING_VULKAN
GskGpuOp *
gsk_gpu_op_vk_command (GskGpuOp        *op,
                       GskGpuFrame     *frame,
                       VkRenderPass     render_pass,
                       VkFormat                format,
                       VkCommandBuffer  command_buffer)
{
  return op->op_class->vk_command (op, frame, render_pass, format, command_buffer);
}
#endif

GskGpuOp *
gsk_gpu_op_gl_command (GskGpuOp    *op,
                       GskGpuFrame *frame)
{
  return op->op_class->gl_command (op, frame);
}

