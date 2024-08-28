#include "config.h"

#include "gskgpucrossfadeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucrossfadeinstance.h"

typedef struct _GskGpuCrossFadeOp GskGpuCrossFadeOp;

struct _GskGpuCrossFadeOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_cross_fade_op_print_instance (GskGpuShaderOp *shader,
                                      gpointer        instance_,
                                      GString        *string)
{
  GskGpuCrossfadeInstance *instance = (GskGpuCrossfadeInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  gsk_gpu_print_image (string, shader->images[1]);
  g_string_append_printf (string, "%g%%", 100 * instance->opacity_progress[1]);
}

static const GskGpuShaderOpClass GSK_GPU_CROSS_FADE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuCrossFadeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucrossfade",
  gsk_gpu_crossfade_n_textures,
  sizeof (GskGpuCrossfadeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_crossfade_info,
#endif
  gsk_gpu_cross_fade_op_print_instance,
  gsk_gpu_crossfade_setup_attrib_locations,
  gsk_gpu_crossfade_setup_vao
};

void
gsk_gpu_cross_fade_op (GskGpuFrame             *frame,
                       GskGpuShaderClip         clip,
                       const graphene_rect_t   *rect,
                       const graphene_point_t  *offset,
                       float                    opacity,
                       float                    progress,
                       const GskGpuShaderImage *start,
                       const GskGpuShaderImage *end)
{
  GskGpuCrossfadeInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CROSS_FADE_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           0,
                           clip,
                           (GskGpuImage *[2]) { start->image, end->image },
                           (GskGpuSampler[2]) { start->sampler, end->sampler },
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  instance->opacity_progress[0] = opacity;
  instance->opacity_progress[1] = progress;
  gsk_gpu_rect_to_float (start->bounds, offset, instance->start_rect);
  gsk_gpu_rect_to_float (end->bounds, offset, instance->end_rect);
}
