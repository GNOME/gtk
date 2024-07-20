#include "config.h"

#include "gskgpumaskopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gskenumtypes.h"

#include "gpu/shaders/gskgpumaskinstance.h"

typedef struct _GskGpuMaskOp GskGpuMaskOp;

struct _GskGpuMaskOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_mask_op_print_instance (GskGpuShaderOp *shader,
                                gpointer        instance_,
                                GString        *string)
{
  GskGpuMaskInstance *instance = (GskGpuMaskInstance *) instance_;

  gsk_gpu_print_enum (string, GSK_TYPE_MASK_MODE, shader->variation);
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_image (string, shader->images[1]);
}

static const GskGpuShaderOpClass GSK_GPU_MASK_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuMaskOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpumask",
  gsk_gpu_mask_n_textures,
  sizeof (GskGpuMaskInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_mask_info,
#endif
  gsk_gpu_mask_op_print_instance,
  gsk_gpu_mask_setup_attrib_locations,
  gsk_gpu_mask_setup_vao
};

void
gsk_gpu_mask_op (GskGpuFrame             *frame,
                 GskGpuShaderClip         clip,
                 const graphene_rect_t   *rect,
                 const graphene_point_t  *offset,
                 float                    opacity,
                 GskMaskMode              mask_mode,
                 const GskGpuShaderImage *source,
                 const GskGpuShaderImage *mask)
{
  GskGpuMaskInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_MASK_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           mask_mode,
                           clip,
                           (GskGpuImage *[2]) { source->image, mask->image },
                           (GskGpuSampler[2]) { source->sampler, mask->sampler },
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (source->bounds, offset, instance->source_rect);
  gsk_gpu_rect_to_float (mask->bounds, offset, instance->mask_rect);
  instance->opacity = opacity;
}
