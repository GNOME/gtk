#include "config.h"

#include "gskgpuconvertopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include "gpu/shaders/gskgpuconvertinstance.h"

typedef struct _GskGpuConvertOp GskGpuConvertOp;

struct _GskGpuConvertOp
{
  GskGpuShaderOp op;
};

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_STRAIGHT_ALPHA       (1u << 1)

static void
gsk_gpu_convert_op_print_instance (GskGpuShaderOp *shader,
                                   gpointer        instance_,
                                   GString        *string)
{
  GskGpuConvertInstance *instance = (GskGpuConvertInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  if (shader->variation & VARIATION_STRAIGHT_ALPHA)
    gsk_gpu_print_string (string, "straight");
}

static const GskGpuShaderOpClass GSK_GPU_CONVERT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuConvertOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuconvert",
  gsk_gpu_convert_n_textures,
  sizeof (GskGpuConvertInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_convert_info,
#endif
  gsk_gpu_convert_op_print_instance,
  gsk_gpu_convert_setup_attrib_locations,
  gsk_gpu_convert_setup_vao
};

void
gsk_gpu_convert_op (GskGpuFrame             *frame,
                    GskGpuShaderClip         clip,
                    GskGpuColorStates        color_states,
                    float                    opacity,
                    gboolean                 straight_alpha,
                    const graphene_point_t  *offset,
                    const GskGpuShaderImage *image)
{
  GskGpuConvertInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CONVERT_OP_CLASS,
                           color_states,
                           (opacity < 1.0 ? VARIATION_OPACITY : 0) |
                           (straight_alpha ? VARIATION_STRAIGHT_ALPHA : 0),
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  instance->opacity = opacity;
}

