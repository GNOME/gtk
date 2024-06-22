#include "config.h"

#include "gskgpuconvertopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include "gpu/shaders/gskgpuconvertinstance.h"

typedef struct _GskGpuConvertOp GskGpuConvertOp;

struct _GskGpuConvertOp
{
  GskGpuShaderOp op;
};

#define VARIATION_OPACITY              (1u << 0)
#define VARIATION_ALPHA_ALL_CHANNELS   (1u << 1)
#define VARIATION_SOURCE_UNPREMULTIPLY (1u << 2)
#define VARIATION_TARGET_PREMULTIPLY   (1u << 3)
#define VARIATION_SOURCE_SHIFT 8u
#define VARIATION_TARGET_SHIFT 16u
#define VARIATION_COLOR_STATE_MASK 0xFFu

static guint
gsk_gpu_convert_encode_variation (GdkColorState *source,
                                  gboolean       source_premultiplied,
                                  GdkColorState *target,
                                  gboolean       target_premultiplied,
                                  gboolean       opacity)
{
  guint conversion;

  if (source == target)
    {
      if (source_premultiplied == target_premultiplied)
        {
          /* no changes should be caught before running a shader */
          g_assert (opacity);
          if (source_premultiplied)
            conversion = VARIATION_ALPHA_ALL_CHANNELS;
          else
            conversion = 0;
        }
      else
        {
          if (source_premultiplied)
            conversion = VARIATION_SOURCE_UNPREMULTIPLY;
          else
            conversion = VARIATION_TARGET_PREMULTIPLY;
        }
    }
  else
    {
      conversion = GDK_DEFAULT_COLOR_STATE_ID (source) << VARIATION_SOURCE_SHIFT;
      conversion |= GDK_DEFAULT_COLOR_STATE_ID (target) << VARIATION_TARGET_SHIFT;
      if (source_premultiplied)
        conversion |= VARIATION_SOURCE_UNPREMULTIPLY;
      if (target_premultiplied)
        conversion |= VARIATION_TARGET_PREMULTIPLY;
    }

  if (opacity)
    conversion |= VARIATION_OPACITY;

  return conversion;
}

static void
gsk_gpu_convert_op_print_instance (GskGpuShaderOp *shader,
                                         gpointer        instance_,
                                         GString        *string)
{
  GskGpuConvertInstance *instance = (GskGpuConvertInstance *) instance_;
  GdkColorState *source, *target;

  source = gdk_color_state_get_by_id ((shader->variation >> VARIATION_SOURCE_SHIFT) & VARIATION_COLOR_STATE_MASK);
  target = gdk_color_state_get_by_id ((shader->variation >> VARIATION_TARGET_SHIFT) & VARIATION_COLOR_STATE_MASK);
  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image_descriptor (string, shader->desc, instance->tex_id);
  g_string_append_printf (string, "%s%s -> %s%s ",
                          gdk_color_state_get_name (source),
                          (shader->variation & VARIATION_SOURCE_UNPREMULTIPLY) ? "(p)" : "",
                          gdk_color_state_get_name (target),
                          (shader->variation & VARIATION_TARGET_PREMULTIPLY) ? "(p)" : "");
}

static const GskGpuShaderOpClass GSK_GPU_CONVERT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuConvertOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuconvert",
  sizeof (GskGpuConvertInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_convert_info,
#endif
  gsk_gpu_convert_op_print_instance,
  gsk_gpu_convert_setup_attrib_locations,
  gsk_gpu_convert_setup_vao
};

void
gsk_gpu_convert_op (GskGpuFrame            *frame,
                    GskGpuShaderClip        clip,
                    GdkColorState          *source,
                    gboolean                source_premultiplied,
                    GdkColorState          *target,
                    gboolean                target_premultiplied,
                    float                   opacity,
                    GskGpuDescriptors      *desc,
                    guint32                 descriptor,
                    const graphene_rect_t  *rect,
                    const graphene_point_t *offset,
                    const graphene_rect_t  *tex_rect)
{
  GskGpuConvertInstance *instance;
  guint variation;

  variation = gsk_gpu_convert_encode_variation (source,
                                                source_premultiplied,
                                                target,
                                                target_premultiplied,
                                                opacity < 1.0);

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CONVERT_OP_CLASS,
                           variation,
                           clip,
                           desc,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_rect_to_float (tex_rect, offset, instance->tex_rect);
  instance->tex_id = descriptor;
  instance->opacity = opacity;
}

