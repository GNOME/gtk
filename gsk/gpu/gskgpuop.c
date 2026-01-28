#include "config.h"

#include "gskgpuopprivate.h"

#include "gskgpuframeprivate.h"

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
gsk_gpu_op_vk_command (GskGpuOp              *op,
                       GskGpuFrame           *frame,
                       GskVulkanCommandState *state)
{
  return op->op_class->vk_command (op, frame, state);
}
#endif

GskGpuOp *
gsk_gpu_op_gl_command (GskGpuOp          *op,
                       GskGpuFrame       *frame,
                       GskGLCommandState *state)
{
  return op->op_class->gl_command (op, frame, state);
}

