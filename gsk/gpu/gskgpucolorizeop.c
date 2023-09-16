#include "config.h"

#include "gskgpucolorizeopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolorizeinstance.h"

typedef struct _GskGpuColorizeOp GskGpuColorizeOp;

struct _GskGpuColorizeOp
{
  GskGpuShaderOp op;

  GskGpuShaderImage image;
};

static void
gsk_gpu_colorize_op_finish (GskGpuOp *op)
{
  GskGpuColorizeOp *self = (GskGpuColorizeOp *) op;

  g_object_unref (self->image.image);
}

static void
gsk_gpu_colorize_op_print (GskGpuOp    *op,
                           GskGpuFrame *frame,
                           GString     *string,
                           guint        indent)
{
  GskGpuColorizeOp *self = (GskGpuColorizeOp *) op;
  GskGpuShaderOp *shader = (GskGpuShaderOp *) op;
  GskGpuColorizeInstance *instance;

  instance = (GskGpuColorizeInstance *) gsk_gpu_frame_get_vertex_data (frame, shader->vertex_offset);

  gsk_gpu_print_op (string, indent, "colorize");
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, self->image.image);
  gsk_gpu_print_rgba (string, instance->color);
  gsk_gpu_print_newline (string);
}

static const GskGpuShaderImage *
gsk_gpu_colorize_op_get_images (GskGpuShaderOp *op,
                                gsize          *n_images)
{
  GskGpuColorizeOp *self = (GskGpuColorizeOp *) op;

  *n_images = 1;

  return &self->image;
}

static const GskGpuShaderOpClass GSK_GPU_COLORIZE_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorizeOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_colorize_op_finish,
    gsk_gpu_colorize_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolorize",
  sizeof (GskGpuColorizeInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colorize_info,
#endif
  gsk_gpu_colorize_op_get_images,
  gsk_gpu_colorize_setup_vao
};

void
gsk_gpu_colorize_op (GskGpuFrame            *frame,
                     GskGpuShaderClip        clip,
                     GskGpuImage            *image,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const graphene_rect_t  *tex_rect,
                     const GdkRGBA          *color)
{
  GskGpuColorizeInstance *instance;
  GskGpuColorizeOp *self;

  self = (GskGpuColorizeOp *) gsk_gpu_shader_op_alloc (frame,
                                                       &GSK_GPU_COLORIZE_OP_CLASS,
                                                       clip,
                                                       &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  self->image.image = g_object_ref (image);
  self->image.sampler = GSK_GPU_SAMPLER_DEFAULT;
  self->image.descriptor = gsk_gpu_frame_get_image_descriptor (frame, image, GSK_GPU_SAMPLER_DEFAULT);
  instance->tex_id = self->image.descriptor;
  gsk_gpu_rgba_to_float (color, instance->color);
}
