#include "config.h"

#include "gskgpucolorconvertopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpucolorconvertinstance.h"

typedef struct _GskGpuColorConvertOp GskGpuColorConvertOp;

struct _GskGpuColorConvertOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_color_convert_op_print_instance (GskGpuShaderOp *shader,
                                         gpointer        instance_,
                                         GString        *string)
{
  GskGpuColorconvertInstance *instance = (GskGpuColorconvertInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
  gsk_gpu_print_color_conversion (string, shader->variation);
}

static const GskGpuShaderOpClass GSK_GPU_COLOR_CONVERT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuColorConvertOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpucolorconvert",
  sizeof (GskGpuColorconvertInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_colorconvert_info,
#endif
  gsk_gpu_color_convert_op_print_instance,
  gsk_gpu_colorconvert_setup_attrib_locations,
  gsk_gpu_colorconvert_setup_vao
};

void
gsk_gpu_color_convert_op (GskGpuFrame            *frame,
                          GskGpuShaderClip        clip,
                          GdkColorState          *from,
                          GdkColorState          *to,
                          GskGpuDescriptors      *desc,
                          guint32                 descriptor,
                          const graphene_rect_t  *rect,
                          const graphene_point_t *offset,
                          const graphene_rect_t  *tex_rect)
{
  GskGpuColorconvertInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_COLOR_CONVERT_OP_CLASS,
                           gsk_gpu_color_conversion (from, to),
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  instance->tex_id = descriptor;
}

guint
gsk_gpu_color_conversion (GdkColorState *from,
                          GdkColorState *to)
{
  if (from == to)
    return 0;

  if (!GDK_IS_NAMED_COLOR_STATE (from) || !GDK_IS_NAMED_COLOR_STATE (to))
    {
      g_warning ("FIXME: Implement support for ICC color transforms");
      return 0;
    }

  return GDK_NAMED_COLOR_STATE_ID (from) |
        (GDK_NAMED_COLOR_STATE_ID (to) << 16);
}

guint
gsk_gpu_color_conversion_triple (GdkColorState *from1,
                                 GdkColorState *from2,
                                 GdkColorState *to)
{
  if (from1 == to && from2 == to)
    return 0;

  if (!GDK_IS_NAMED_COLOR_STATE (from1) ||
      !GDK_IS_NAMED_COLOR_STATE (from2) ||
      !GDK_IS_NAMED_COLOR_STATE (to))
    {
      g_warning ("FIXME: Implement support for ICC color transforms");
      return 0;
    }

  return GDK_NAMED_COLOR_STATE_ID (from1) |
         (GDK_NAMED_COLOR_STATE_ID (from2) << 5) |
         (GDK_NAMED_COLOR_STATE_ID (to) << 10);
}
