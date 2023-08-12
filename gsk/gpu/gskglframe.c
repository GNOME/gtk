#include "config.h"

#include "gskglframeprivate.h"

#include "gskgpuopprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkglcontextprivate.h"

struct _GskGLFrame
{
  GskGpuFrame parent_instance;
};

struct _GskGLFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskGLFrame, gsk_gl_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_gl_frame_is_busy (GskGpuFrame *frame)
{
  return FALSE;
}

static void
gsk_gl_frame_submit (GskGpuFrame *frame,
                     GskGpuOp    *op)
{
  while (op)
    {
      op = gsk_gpu_op_gl_command (op, frame);
    }
}

static void
gsk_gl_frame_class_init (GskGLFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);

  gpu_frame_class->is_busy = gsk_gl_frame_is_busy;
  gpu_frame_class->submit = gsk_gl_frame_submit;
}

static void
gsk_gl_frame_init (GskGLFrame *self)
{
}

