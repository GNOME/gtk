#include "config.h"

#include "gskgpucolorizeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolorizeinstance.h"

typedef struct _GskGpuColorizeOp GskGpuColorizeOp;

struct _GskGpuColorizeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_colorize_op_print_instance (GskGpuShaderOp *shader,
                                    gpointer        instance_,
                                    GString        *string)
{
  GskGpuColorizeInstance *instance = (GskGpuColorizeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_rgba (string, instance->color);
}

static const GskGpuShaderOpClass GSK_GPU_COLORIZE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorizeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolorize",
  gsk_gpu_colorize_n_textures,
  sizeof (GskGpuColorizeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colorize_info,
#endif
  gsk_gpu_colorize_op_print_instance,
  gsk_gpu_colorize_setup_attrib_locations,
  gsk_gpu_colorize_setup_vao
};

void
gsk_gpu_colorize_op (GskGpuFrame             *frame,
                     GskGpuShaderClip         clip,
                     GdkColorState           *ccs,
                     float                    opacity,
                     const graphene_point_t  *offset,
                     const GskGpuShaderImage *image,
                     const GdkColor          *color)
{
  GskGpuColorizeInstance *instance;
  GdkColorState *alt;

  alt = gsk_gpu_color_states_find (ccs, color);

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLORIZE_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE, alt, FALSE),
                           0,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage ? image->coverage : image->bounds, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  gsk_gpu_color_to_float (color, alt, opacity, instance->color);
}
