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
gsk_gpu_rounded_color_op_print_instance (GskGpuShaderOp *shader,
                                         gpointer        instance_,
                                         GString        *string)
{
  GskGpuRoundedcolorInstance *instance = (GskGpuRoundedcolorInstance *) instance_;

  gsk_gpu_print_rounded_rect (string, instance->outline);
  gsk_gpu_print_rgba (string, instance->color);
}

static const GskGpuShaderOpClass GSK_GPU_ROUNDED_COLOR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuRoundedColorOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuroundedcolor",
  gsk_gpu_roundedcolor_n_textures,
  sizeof (GskGpuRoundedcolorInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_roundedcolor_info,
#endif
  gsk_gpu_rounded_color_op_print_instance,
  gsk_gpu_roundedcolor_setup_attrib_locations,
  gsk_gpu_roundedcolor_setup_vao
};

void
gsk_gpu_rounded_color_op (GskGpuFrame            *frame,
                          GskGpuShaderClip        clip,
                          GdkColorState          *ccs,
                          float                   opacity,
                          const graphene_point_t *offset,
                          const GskRoundedRect   *outline,
                          const GdkColor         *color)
{
  GskGpuRoundedcolorInstance *instance;
  GdkColorState *alt;

  alt = gsk_gpu_color_states_find (ccs, color);

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_ROUNDED_COLOR_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE, alt, FALSE),
                           0,
                           clip,
                           NULL,
                           NULL,
                           &instance);

  gsk_rounded_rect_to_float (outline, offset, instance->outline);
  gsk_gpu_color_to_float (color, alt, opacity, instance->color);
}

