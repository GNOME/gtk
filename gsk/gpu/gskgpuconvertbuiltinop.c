#include "config.h"

#include "gskgpuconvertbuiltinopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gdk/gdkcolorstateprivate.h"

#include "gpu/shaders/gskgpuconvertbuiltininstance.h"

typedef struct _GskGpuConvertBuiltinOp GskGpuConvertBuiltinOp;

struct _GskGpuConvertBuiltinOp
{
  GskGpuShaderOp op;
};

#define VARIATION_COLOR_SPACE_MASK     (0xFF)
#define VARIATION_OPACITY              (1u << 8)
#define VARIATION_PREMULTIPLY          (1u << 9)
#define VARIATION_REVERSE              (1u << 10)

static void
gsk_gpu_convert_builtin_op_print_instance (GskGpuShaderOp *shader,
                                           gpointer        instance_,
                                           GString        *string)
{
  GskGpuConvertbuiltinInstance *instance = (GskGpuConvertbuiltinInstance *) instance_;
  GdkColorState *builtin = (GdkColorState *) &gdk_builtin_color_states [shader->variation & VARIATION_COLOR_SPACE_MASK];

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_image (string, shader->images[0]);
  if (shader->variation & VARIATION_REVERSE)
    gsk_gpu_print_string (string, "reverse");
  gsk_gpu_print_string (string, gdk_color_state_get_name (builtin));
}

static const GskGpuShaderOpClass GSK_GPU_CONVERT_BUILTIN_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuConvertBuiltinOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpuconvertbuiltin",
  gsk_gpu_convertbuiltin_n_textures,
  sizeof (GskGpuConvertbuiltinInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_convertbuiltin_info,
#endif
  gsk_gpu_convert_builtin_op_print_instance,
  gsk_gpu_convertbuiltin_setup_attrib_locations,
  gsk_gpu_convertbuiltin_setup_vao
};

static GdkColorState *
gsk_gpu_get_shader_color_state (GdkColorState *builtin)
{
  switch (GDK_BUILTIN_COLOR_STATE_ID (builtin))
    {
    case GDK_BUILTIN_COLOR_STATE_ID_OKLAB:
    case GDK_BUILTIN_COLOR_STATE_ID_OKLCH:
      return GDK_COLOR_STATE_SRGB_LINEAR;

    case GDK_BUILTIN_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
      return NULL;
    }
}

void
gsk_gpu_convert_from_builtin_op (GskGpuFrame             *frame,
                                 GskGpuShaderClip         clip,
                                 GdkColorState           *ccs,
                                 GdkColorState           *builtin,
                                 float                    opacity,
                                 const graphene_point_t  *offset,
                                 const GskGpuShaderImage *image)
{
  GskGpuConvertbuiltinInstance *instance;

  g_assert (GDK_IS_BUILTIN_COLOR_STATE (builtin));

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CONVERT_BUILTIN_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE,
                                                        gsk_gpu_get_shader_color_state (builtin), FALSE),
                           GDK_BUILTIN_COLOR_STATE_ID (builtin) |
                           (opacity < 1.0 ? VARIATION_OPACITY : 0),
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  instance->opacity = opacity;
}

void
gsk_gpu_convert_to_builtin_op (GskGpuFrame             *frame,
                               GskGpuShaderClip         clip,
                               GdkColorState           *builtin,
                               gboolean                 builtin_premultiplied,
                               GdkColorState           *source_cs,
                               float                    opacity,
                               const graphene_point_t  *offset,
                               const GskGpuShaderImage *image)
{
  GskGpuConvertbuiltinInstance *instance;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_CONVERT_BUILTIN_OP_CLASS,
                           gsk_gpu_color_states_create (source_cs, TRUE,
                                                        gsk_gpu_get_shader_color_state (builtin), FALSE),
                           GDK_BUILTIN_COLOR_STATE_ID (builtin) |
                           (opacity < 1.0 ? VARIATION_OPACITY : 0) |
                           (builtin_premultiplied ? VARIATION_PREMULTIPLY : 0) |
                           VARIATION_REVERSE,
                           clip,
                           (GskGpuImage *[1]) { image->image },
                           (GskGpuSampler[1]) { image->sampler },
                           &instance);

  gsk_gpu_rect_to_float (image->coverage, offset, instance->rect);
  gsk_gpu_rect_to_float (image->bounds, offset, instance->tex_rect);
  instance->opacity = opacity;
}
