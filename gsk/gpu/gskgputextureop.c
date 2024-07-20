#include "config.h"

#include "gskgputextureopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgputextureinstance.h"

typedef struct _GskGpuTextureOp GskGpuTextureOp;

struct _GskGpuTextureOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_texture_op_print_instance (GskGpuShaderOp *shader,
                                   gpointer        instance_,
                                   GString        *string)
{
  GskGpuTextureInstance *instance = (GskGpuTextureInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
}

static const GskGpuShaderOpClass GSK_GPU_TEXTURE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuTextureOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgputexture",
  gsk_gpu_texture_n_textures,
  sizeof (GskGpuTextureInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_texture_info,
#endif
  gsk_gpu_texture_op_print_instance,
  gsk_gpu_texture_setup_attrib_locations,
  gsk_gpu_texture_setup_vao
};

void
gsk_gpu_texture_op (GskGpuFrame             *frame,
                    GskGpuShaderClip         clip,
                    const graphene_point_t  *offset,
                    const GskGpuShaderImage *image)
{
  GskGpuTextureInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_TEXTURE_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           0,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage ? image->coverage : image->bounds, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
}
