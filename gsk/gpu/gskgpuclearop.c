#include "config.h"

#include "gskgpuclearopprivate.h"

#include "gskgpuopprivate.h"
#include "gskgpuprintprivate.h"

#include "gdk/gdkcolorstateprivate.h"

typedef struct _GskGpuClearOp GskGpuClearOp;

struct _GskGpuClearOp
{
  GskGpuOp op;

  cairo_rectangle_int_t rect;
  float color[4];
};

static void
gsk_gpu_clear_op_finish (GskGpuOp *op)
{
}

static void
gsk_gpu_clear_op_print (GskGpuOp    *op,
                        GskGpuFrame *frame,
                        GString     *string,
                        guint        indent)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;

  gsk_gpu_print_op (string, indent, "clear");
  gsk_gpu_print_int_rect (string, &self->rect);
  gsk_gpu_print_rgba (string, self->color);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_clear_op_vk_command (GskGpuOp              *op,
                             GskGpuFrame           *frame,
                             GskVulkanCommandState *state)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;
  VkClearValue clear_value;

  memcpy (clear_value.color.float32, self->color, sizeof (float) * 4);

  vkCmdClearAttachments (state->vk_command_buffer,
                         1,
                         &(VkClearAttachment) {
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           0,
                           clear_value,
                         },
                         1,
                         &(VkClearRect) {
                           {
                             { self->rect.x, self->rect.y },
                             { self->rect.width, self->rect.height },
                           },
                           0,
                           1
                         });

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_clear_op_gl_command (GskGpuOp          *op,
                             GskGpuFrame       *frame,
                             GskGLCommandState *state)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;
  int scissor[4];

  glGetIntegerv (GL_SCISSOR_BOX, scissor);

  if (state->flip_y)
    glScissor (self->rect.x, state->flip_y - self->rect.y - self->rect.height, self->rect.width, self->rect.height);
  else
    glScissor (self->rect.x, self->rect.y, self->rect.width, self->rect.height);

  glClearColor (self->color[0], self->color[1], self->color[2], self->color[3]);
  glClear (GL_COLOR_BUFFER_BIT);

  glScissor (scissor[0], scissor[1], scissor[2], scissor[3]);

  return op->next;
}

#ifdef GDK_WINDOWING_WIN32
static GskGpuOp *
gsk_gpu_clear_op_d3d12_command (GskGpuOp             *op,
                                GskGpuFrame          *frame,
                                GskD3d12CommandState *state)
{
  GskGpuClearOp *self = (GskGpuClearOp *) op;

  ID3D12GraphicsCommandList_ClearRenderTargetView (state->command_list,
                                                   state->rtv,
                                                   self->color,
                                                   1,
                                                   (&(D3D12_RECT) {
                                                       .left = self->rect.x,
                                                       .top = self->rect.y,
                                                       .right = self->rect.x + self->rect.width,
                                                       .bottom = self->rect.y + self->rect.height
                                                   }));

  return op->next;
}
#endif

static const GskGpuOpClass GSK_GPU_CLEAR_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuClearOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_clear_op_finish,
  gsk_gpu_clear_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_clear_op_vk_command,
#endif
  gsk_gpu_clear_op_gl_command,
#ifdef GDK_WINDOWING_WIN32
  gsk_gpu_clear_op_d3d12_command,
#endif
};

void
gsk_gpu_clear_op (GskGpuFrame                 *frame,
                  const cairo_rectangle_int_t *rect,
                  const float                  color[4])
{
  GskGpuClearOp *self;

  self = (GskGpuClearOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_CLEAR_OP_CLASS);

  self->rect = *rect;
  memcpy (self->color, color, sizeof (float) * 4);
}
