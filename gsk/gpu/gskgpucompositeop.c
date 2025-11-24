#include "config.h"

#include "gskgpucompositeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gskenumtypes.h"

#include "gpu/shaders/gskgpucompositeinstance.h"

typedef struct _GskGpuCompositeOp GskGpuCompositeOp;

struct _GskGpuCompositeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_composite_op_print_instance (GskGpuShaderOp *shader,
                                     gpointer        instance_,
                                     GString        *string)
{
  GskGpuCompositeInstance *instance = (GskGpuCompositeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_image (string, shader->images[1]);
}

static const GskGpuShaderOpClass GSK_GPU_COMPOSITE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuCompositeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command,
#ifdef GDK_WINDOWING_WIN32
    gsk_gpu_shader_op_d3d12_command
#endif
  },
  "gskgpucomposite",
  gsk_gpu_composite_n_textures,
  sizeof (GskGpuCompositeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_composite_info,
#endif
#ifdef GDK_WINDOWING_WIN32
  &gsk_gpu_composite_input_layout,
#endif
  gsk_gpu_composite_op_print_instance,
  gsk_gpu_composite_setup_attrib_locations,
  gsk_gpu_composite_setup_vao
};

void
gsk_gpu_composite_op (GskGpuFrame             *frame,
                      GskGpuShaderClip         clip,
                      const graphene_rect_t   *rect,
                      const graphene_point_t  *offset,
                      float                    opacity,
                      GskPorterDuff            op,
                      const GskGpuShaderImage *source,
                      const GskGpuShaderImage *mask)
{
  GskGpuCompositeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COMPOSITE_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           op,
                           clip,
                           (GskGpuImage *[2]) { source->image, mask->image },
                           (GskGpuSampler[2]) { source->sampler, mask->sampler },
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (source->bounds, offset, instance->source_rect);
  gsk_gpu_rect_to_float (mask->bounds, offset, instance->mask_rect);
  instance->opacity = opacity;
}
