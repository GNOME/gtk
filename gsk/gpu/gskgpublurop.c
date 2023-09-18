#include "config.h"

#include "gskgpubluropprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpublurinstance.h"

typedef struct _GskGpuBlurOp GskGpuBlurOp;

struct _GskGpuBlurOp
{
  GskGpuShaderOp op;

  GskGpuShaderImage image;
};

static void
gsk_gpu_blur_op_finish (GskGpuOp *op)
{
  GskGpuBlurOp *self = (GskGpuBlurOp *) op;

  g_object_unref (self->image.image);
}

static void
gsk_gpu_blur_op_print (GskGpuOp    *op,
                       GskGpuFrame *frame,
                       GString     *string,
                       guint        indent)
{
  GskGpuBlurOp *self = (GskGpuBlurOp *) op;
  GskGpuShaderOp *shader = (GskGpuShaderOp *) op;
  GskGpuBlurInstance *instance;

  instance = (GskGpuBlurInstance *) gsk_gpu_frame_get_vertex_data (frame, shader->vertex_offset);

  gsk_gpu_print_op (string, indent, "blur");
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, self->image.image);
  gsk_gpu_print_newline (string);
}

static const GskGpuShaderImage *
gsk_gpu_blur_op_get_images (GskGpuShaderOp *op,
                            gsize          *n_images)
{
  GskGpuBlurOp *self = (GskGpuBlurOp *) op;

  *n_images = 1;

  return &self->image;
}

static const GskGpuShaderOpClass GSK_GPU_BLUR_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBlurOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_blur_op_finish,
    gsk_gpu_blur_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpublur",
  sizeof (GskGpuBlurInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_blur_info,
#endif
  gsk_gpu_blur_op_get_images,
  gsk_gpu_blur_setup_vao
};

void
gsk_gpu_blur_op (GskGpuFrame            *frame,
                 GskGpuShaderClip        clip,
                 GskGpuImage            *image,
                 GskGpuSampler           sampler,
                 const graphene_rect_t  *rect,
                 const graphene_point_t *offset,
                 const graphene_rect_t  *tex_rect,
                 const graphene_vec2_t  *blur_direction,
                 const GdkRGBA          *blur_color)
{
  GskGpuBlurInstance *instance;
  GskGpuBlurOp *self;

  self = (GskGpuBlurOp *) gsk_gpu_shader_op_alloc (frame,
                                                   &GSK_GPU_BLUR_OP_CLASS,
                                                   clip,
                                                   &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  graphene_vec2_to_float (blur_direction, instance->blur_direction);
  gsk_gpu_rgba_to_float (blur_color, instance->blur_color);
  self->image.image = g_object_ref (image);
  self->image.sampler = sampler;
  self->image.descriptor = gsk_gpu_frame_get_image_descriptor (frame, image, sampler);
  instance->tex_id = self->image.descriptor;
}
