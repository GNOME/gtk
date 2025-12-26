#include "config.h"

#include "gskgpuarithmeticopprivate.h"

#include "gskenumtypes.h"
#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpuarithmeticinstance.h"

typedef struct _GskGpuArithmeticOp GskGpuArithmeticOp;

struct _GskGpuArithmeticOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_arithmetic_op_print_instance (GskGpuShaderOp *shader,
                                      gpointer        instance_,
                                      GString        *string)
{
//  GskGpuArithmeticInstance *instance = (GskGpuArithmeticInstance *) instance_;
}

static const GskGpuShaderOpClass GSK_GPU_BLEND_MODE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuArithmeticOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuarithmetic",
  gsk_gpu_arithmetic_n_textures,
  sizeof (GskGpuArithmeticInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_arithmetic_info,
#endif
  gsk_gpu_arithmetic_op_print_instance,
  gsk_gpu_arithmetic_setup_attrib_locations,
  gsk_gpu_arithmetic_setup_vao
};

void
gsk_gpu_arithmetic_op (GskGpuFrame             *frame,
                       GskGpuShaderClip         clip,
                       const graphene_rect_t   *rect,
                       const graphene_point_t  *offset,
                       float                    opacity,
                       float                    factors[4],
                       const GskGpuShaderImage *first,
                       const GskGpuShaderImage *second)
{
  GskGpuArithmeticInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLEND_MODE_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           0,
                           clip,
                           (GskGpuImage *[2]) { first->image, second->image },
                           (GskGpuSampler[2]) { first->sampler, second->sampler },
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  instance->opacity = opacity;
  gsk_gpu_vec4_to_float (factors, instance->factors);
  gsk_gpu_rect_to_float (first->bounds, offset, instance->first_rect);
  gsk_gpu_rect_to_float (second->bounds, offset, instance->second_rect);
}
