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

#ifdef GDK_WINDOWING_WIN32
GskGpuOp *
gsk_gpu_op_d3d12_command (GskGpuOp             *op,
                          GskGpuFrame          *frame,
                          GskD3d12CommandState *state)
{
  if (!op->op_class->d3d12_command)
    {
      GString *string = g_string_new ("");
      gsk_gpu_op_print (op, frame, string, 0);
      g_warning ("FIXME: Implement %s", string->str);
      g_string_free (string, TRUE);
      return op->next;
    }
  return op->op_class->d3d12_command (op, frame, state);
}
#endif
