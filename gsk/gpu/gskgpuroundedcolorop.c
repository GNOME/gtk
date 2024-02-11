#include "config.h"

#include "gskgpuroundedcoloropprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpushaderopprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include "gpu/shaders/gskgpuroundedcolorinstance.h"

typedef struct _GskGpuRoundedColorOp GskGpuRoundedColorOp;

struct _GskGpuRoundedColorOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_rounded_color_op_print (GskGpuOp    *op,
                                GskGpuFrame *frame,
                                GString     *string,
                                guint        indent)
{
  GskGpuShaderOp *shader = (GskGpuShaderOp *) op;
  GskGpuRoundedcolorInstance *instance;

  instance = (GskGpuRoundedcolorInstance *) gsk_gpu_frame_get_vertex_data (frame, shader->vertex_offset);

  gsk_gpu_print_op (string, indent, "rounded-color");
  gsk_gpu_print_shader_info (string, shader->clip);
  gsk_gpu_print_rounded_rect (string, instance->outline);
  gsk_gpu_print_rgba (string, instance->color);
  gsk_gpu_print_newline (string);
}

static const GskGpuShaderOpClass GSK_GPU_ROUNDED_COLOR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuRoundedColorOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_rounded_color_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuroundedcolor",
  sizeof (GskGpuRoundedcolorInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_roundedcolor_info,
#endif
  gsk_gpu_roundedcolor_setup_attrib_locations,
  gsk_gpu_roundedcolor_setup_vao
};

void
gsk_gpu_rounded_color_op (GskGpuFrame            *frame,
                          GskGpuShaderClip        clip,
                          const GskRoundedRect   *outline,
                          const graphene_point_t *offset,
                          const GdkRGBA          *color)
{
  GskGpuRoundedcolorInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_ROUNDED_COLOR_OP_CLASS,
                           0,
                           clip,
                           NULL,
                           &instance);

  gsk_rounded_rect_to_float (outline, offset, instance->outline);
  gsk_gpu_rgba_to_float (color, instance->color);
}

