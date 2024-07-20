#include "config.h"

#include "gskgpucolormatrixopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolormatrixinstance.h"

typedef struct _GskGpuColorMatrixOp GskGpuColorMatrixOp;

struct _GskGpuColorMatrixOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_color_matrix_op_print_instance (GskGpuShaderOp *shader,
                                        gpointer        instance_,
                                        GString        *string)
{
  GskGpuColormatrixInstance *instance = (GskGpuColormatrixInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
}

static const GskGpuShaderOpClass GSK_GPU_COLOR_MATRIX_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorMatrixOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolormatrix",
  gsk_gpu_colormatrix_n_textures,
  sizeof (GskGpuColormatrixInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colormatrix_info,
#endif
  gsk_gpu_color_matrix_op_print_instance,
  gsk_gpu_colormatrix_setup_attrib_locations,
  gsk_gpu_colormatrix_setup_vao
};

void
gsk_gpu_color_matrix_op (GskGpuFrame             *frame,
                         GskGpuShaderClip         clip,
                         GskGpuColorStates        color_states,
                         const graphene_point_t  *offset,
                         const GskGpuShaderImage *image,
                         const graphene_matrix_t *color_matrix,
                         const graphene_vec4_t   *color_offset)
{
  GskGpuColormatrixInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLOR_MATRIX_OP_CLASS,
                           color_states,
                           0,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  graphene_matrix_to_float (color_matrix, instance->color_matrix);
  graphene_vec4_to_float (color_offset, instance->color_offset);
}

