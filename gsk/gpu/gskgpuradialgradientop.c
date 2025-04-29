#include "config.h"

#include "gskgpuradialgradientopprivate.h"
#include "gskgpulineargradientopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpuradialgradientinstance.h"

#define VARIATION_SUPERSAMPLING (1 << 0)
#define VARIATION_CONCENTRIC (1 << 1)
#define VARIATION_REPEAT(repeat) ((repeat) << 2)

typedef struct _GskGpuRadialGradientOp GskGpuRadialGradientOp;

struct _GskGpuRadialGradientOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_radial_gradient_op_print_instance (GskGpuShaderOp *shader,
                                           gpointer        instance_,
                                           GString        *string)
{
  GskGpuRadialgradientInstance *instance = (GskGpuRadialgradientInstance *) instance_;
  const char *repeat_modes[] = { "none", "pad", "repeat", "reflect" };

  gsk_gpu_print_string (string, repeat_modes[shader->variation >> 2]);
  gsk_gpu_print_rect (string, instance->rect);
}

static const GskGpuShaderOpClass GSK_GPU_RADIAL_GRADIENT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuRadialGradientOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command,
#ifdef GDK_WINDOWING_WIN32
    gsk_gpu_shader_op_d3d12_command,
#endif
  },
  "gskgpuradialgradient",
  gsk_gpu_radialgradient_n_textures,
  sizeof (GskGpuRadialgradientInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_radialgradient_info,
#endif
#ifdef GDK_WINDOWING_WIN32
  &gsk_gpu_radialgradient_input_layout,
#endif
  gsk_gpu_radial_gradient_op_print_instance,
  gsk_gpu_radialgradient_setup_attrib_locations,
  gsk_gpu_radialgradient_setup_vao
};

void
gsk_gpu_radial_gradient_op (GskGpuFrame             *frame,
                            GskGpuShaderClip         clip,
                            GdkColorState           *ccs,
                            float                    opacity,
                            const graphene_point_t  *offset,
                            GdkColorState           *ics,
                            GskHueInterpolation      hue_interp,
                            GskRepeat                repeat,
                            const graphene_rect_t   *rect,
                            const graphene_point_t  *start_center,
                            const graphene_point_t  *start_radius,
                            const graphene_point_t  *end_center,
                            const graphene_point_t  *end_radius,
                            const GskGradientStop   *stops,
                            gsize                    n_stops)
{
  GskGpuRadialgradientInstance *instance;

  g_assert (n_stops > 1);
  g_assert (n_stops <= 7);

  /* Note: we pass TRUE for alt-premultiplied because the
   * vertex shader applies the alpha to the colors.
   */
  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_RADIAL_GRADIENT_OP_CLASS,
                           ccs ? gsk_gpu_color_states_create (ccs, TRUE, ics, TRUE)
                               : gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           VARIATION_REPEAT(repeat) |
                           (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_GRADIENTS) ? VARIATION_SUPERSAMPLING : 0) |
                           (graphene_point_equal (start_center, end_center) ? VARIATION_CONCENTRIC : 0),
                           clip,
                           NULL,
                           NULL,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_point_to_float (start_center, offset, instance->start_circle);
  gsk_gpu_point_to_float (start_radius, graphene_point_zero(), &instance->start_circle[2]);
  gsk_gpu_point_to_float (end_center, offset, instance->end_circle);
  gsk_gpu_point_to_float (end_radius, graphene_point_zero(), &instance->end_circle[2]);
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 6)].color, ics, opacity, instance->color6);
  instance->offsets1[2] = stops[MIN (n_stops - 1, 6)].offset;
  instance->hints1[2] = stops[MIN (n_stops - 1, 6)].transition_hint;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 5)].color, ics, opacity, instance->color5);
  instance->offsets1[1] = stops[MIN (n_stops - 1, 5)].offset;
  instance->hints1[1] = stops[MIN (n_stops - 1, 5)].transition_hint;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 4)].color, ics, opacity, instance->color4);
  instance->offsets1[0] = stops[MIN (n_stops - 1, 4)].offset;
  instance->hints1[0] = stops[MIN (n_stops - 1, 4)].transition_hint;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 3)].color, ics, opacity, instance->color3);
  instance->offsets0[3] = stops[MIN (n_stops - 1, 3)].offset;
  instance->hints0[3] = stops[MIN (n_stops - 1, 3)].transition_hint;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 2)].color, ics, opacity, instance->color2);
  instance->offsets0[2] = stops[MIN (n_stops - 1, 2)].offset;
  instance->hints0[2] = stops[MIN (n_stops - 1, 2)].transition_hint;
  gsk_gpu_color_to_float (&stops[1].color, ics, opacity, instance->color1);
  instance->offsets0[1] = stops[1].offset;
  instance->hints0[1] = stops[1].transition_hint;
  gsk_gpu_color_to_float (&stops[0].color, ics, opacity, instance->color0);
  instance->offsets0[0] = stops[0].offset;
  instance->hints0[0] = stops[0].transition_hint;

  gsk_adjust_hue (ics, hue_interp, instance->color0, instance->color1);
  gsk_adjust_hue (ics, hue_interp, instance->color1, instance->color2);
  gsk_adjust_hue (ics, hue_interp, instance->color2, instance->color3);
  gsk_adjust_hue (ics, hue_interp, instance->color3, instance->color4);
  gsk_adjust_hue (ics, hue_interp, instance->color4, instance->color5);
  gsk_adjust_hue (ics, hue_interp, instance->color5, instance->color6);
}
