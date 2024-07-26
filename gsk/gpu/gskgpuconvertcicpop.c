#include "config.h"

#include "gskgpuconvertcicpopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include "gpu/shaders/gskgpuconvertcicpinstance.h"

typedef struct _GskGpuConvertCicpOp GskGpuConvertCicpOp;

struct _GskGpuConvertCicpOp
{
  GskGpuShaderOp op;
};

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_STRAIGHT_ALPHA       (1u << 1)
#define VARIATION_REVERSE              (1u << 2)

static void
gsk_gpu_convert_cicp_op_print_instance (GskGpuShaderOp *shader,
                                        gpointer        instance_,
                                        GString        *string)
{
  GskGpuConvertcicpInstance *instance = (GskGpuConvertcicpInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  if (shader->variation & VARIATION_STRAIGHT_ALPHA)
    gsk_gpu_print_string (string, "straight");
  if (shader->variation & VARIATION_REVERSE)
    gsk_gpu_print_string (string, "reverse");
  g_string_append_printf (string, "cicp %u/%u/%u/%u",
                          instance->color_primaries,
                          instance->transfer_function,
                          0, 1);
}

static const GskGpuShaderOpClass GSK_GPU_CONVERT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuConvertCicpOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuconvertcicp",
  gsk_gpu_convertcicp_n_textures,
  sizeof (GskGpuConvertcicpInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_convertcicp_info,
#endif
  gsk_gpu_convert_cicp_op_print_instance,
  gsk_gpu_convertcicp_setup_attrib_locations,
  gsk_gpu_convertcicp_setup_vao
};

void
gsk_gpu_convert_from_cicp_op (GskGpuFrame             *frame,
                              GskGpuShaderClip         clip,
                              const GdkCicp           *cicp,
                              GskGpuColorStates        color_states,
                              float                    opacity,
                              gboolean                 straight_alpha,
                              const graphene_point_t  *offset,
                              const GskGpuShaderImage *image)
{
  GskGpuConvertcicpInstance *instance;

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
  instance->color_primaries = cicp->color_primaries;
  instance->transfer_function = cicp->transfer_function;
}

void
gsk_gpu_convert_to_cicp_op (GskGpuFrame             *frame,
                            GskGpuShaderClip         clip,
                            const GdkCicp           *cicp,
                            GskGpuColorStates        color_states,
                            float                    opacity,
                            gboolean                 straight_alpha,
                            const graphene_point_t  *offset,
                            const GskGpuShaderImage *image)
{
  GskGpuConvertcicpInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CONVERT_OP_CLASS,
                           color_states,
                           (opacity < 1.0 ? VARIATION_OPACITY : 0) |
                             (straight_alpha ? VARIATION_STRAIGHT_ALPHA : 0) |
                             VARIATION_REVERSE,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  instance->opacity = opacity;
  instance->color_primaries = cicp->color_primaries;
  instance->transfer_function = cicp->transfer_function;
}
