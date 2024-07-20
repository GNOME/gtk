#include "config.h"

#include "gskgpublendmodeopprivate.h"

#include "gskenumtypes.h"
#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpublendmodeinstance.h"

typedef struct _GskGpuBlendModeOp GskGpuBlendModeOp;

struct _GskGpuBlendModeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_blend_mode_op_print_instance (GskGpuShaderOp *shader,
                                      gpointer        instance_,
                                      GString        *string)
{
  GskGpuBlendmodeInstance *instance = (GskGpuBlendmodeInstance *) instance_;

  gsk_gpu_print_enum (string, GSK_TYPE_BLEND_MODE, shader->variation);
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_enum (string, GSK_TYPE_BLEND_MODE, shader->variation);
  gsk_gpu_print_image (string, shader->images[1]);
}

static const GskGpuShaderOpClass GSK_GPU_BLEND_MODE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBlendModeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpublendmode",
  gsk_gpu_blendmode_n_textures,
  sizeof (GskGpuBlendmodeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_blendmode_info,
#endif
  gsk_gpu_blend_mode_op_print_instance,
  gsk_gpu_blendmode_setup_attrib_locations,
  gsk_gpu_blendmode_setup_vao
};

void
gsk_gpu_blend_mode_op (GskGpuFrame             *frame,
                       GskGpuShaderClip         clip,
                       const graphene_rect_t   *rect,
                       const graphene_point_t  *offset,
                       float                    opacity,
                       GskBlendMode             blend_mode,
                       const GskGpuShaderImage *bottom,
                       const GskGpuShaderImage *top)
{
  GskGpuBlendmodeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BLEND_MODE_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           blend_mode,
                           clip,
                           (GskGpuImage *[2]) { bottom->image, top->image },
                           (GskGpuSampler[2]) { bottom->sampler, top->sampler },
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  instance->opacity = opacity;
  gsk_gpu_rect_to_float (bottom->bounds, offset, instance->bottom_rect);
  gsk_gpu_rect_to_float (top->bounds, offset, instance->top_rect);
}
