#include "config.h"

#include "gskgpubluropprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gdk/gdkrgbaprivate.h"

#include "gpu/shaders/gskgpublurinstance.h"

#define VARIATION_COLORIZE 1

typedef struct _GskGpuBlurOp GskGpuBlurOp;

struct _GskGpuBlurOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_blur_op_print_instance (GskGpuShaderOp *shader,
                                gpointer        instance_,
                                GString        *string)
{
  GskGpuBlurInstance *instance = (GskGpuBlurInstance *) instance_;

  g_string_append_printf (string, "%g,%g ", instance->blur_direction[0], instance->blur_direction[1]);
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  if (shader->variation & VARIATION_COLORIZE)
    gsk_gpu_print_rgba (string, instance->blur_color);
}

static const GskGpuShaderOpClass GSK_GPU_BLUR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBlurOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpublur",
  gsk_gpu_blur_n_textures,
  sizeof (GskGpuBlurInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_blur_info,
#endif
  gsk_gpu_blur_op_print_instance,
  gsk_gpu_blur_setup_attrib_locations,
  gsk_gpu_blur_setup_vao
};

void
gsk_gpu_blur_op (GskGpuFrame             *frame,
                 GskGpuShaderClip         clip,
                 GdkColorState           *ccs,
                 float                    opacity,
                 const graphene_point_t  *offset,
                 const GskGpuShaderImage *image,
                 const graphene_vec2_t   *blur_direction)
{
  GskGpuBlurInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLUR_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           0,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  graphene_vec2_to_float (blur_direction, instance->blur_direction);
}

void
gsk_gpu_blur_shadow_op (GskGpuFrame             *frame,
                        GskGpuShaderClip         clip,
                        GdkColorState           *ccs,
                        float                    opacity,
                        const graphene_point_t  *offset,
                        const GskGpuShaderImage *image,
                        const graphene_vec2_t   *blur_direction,
                        const GdkColor          *shadow_color)
{
  GskGpuBlurInstance *instance;
  GdkColorState *alt;

  alt = gsk_gpu_color_states_find (ccs, shadow_color);

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLUR_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE, alt, FALSE),
                           VARIATION_COLORIZE,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  graphene_vec2_to_float (blur_direction, instance->blur_direction);
  gsk_gpu_color_to_float (shadow_color, alt, opacity, instance->blur_color);
}

