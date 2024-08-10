#include "config.h"

#include "gskgpulineargradientopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"

#include "gpu/shaders/gskgpulineargradientinstance.h"

#define VARIATION_SUPERSAMPLING (1 << 0)
#define VARIATION_REPEATING     (1 << 1)

typedef struct _GskGpuLinearGradientOp GskGpuLinearGradientOp;

struct _GskGpuLinearGradientOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_linear_gradient_op_print_instance (GskGpuShaderOp *shader,
                                           gpointer        instance_,
                                           GString        *string)
{
  GskGpuLineargradientInstance *instance = (GskGpuLineargradientInstance *) instance_;

  if (shader->variation & VARIATION_REPEATING)
    gsk_gpu_print_string (string, "repeating");
  gsk_gpu_print_rect (string, instance->rect);
}

static const GskGpuShaderOpClass GSK_GPU_LINEAR_GRADIENT_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuLinearGradientOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpulineargradient",
  gsk_gpu_lineargradient_n_textures,
  sizeof (GskGpuLineargradientInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_lineargradient_info,
#endif
  gsk_gpu_linear_gradient_op_print_instance,
  gsk_gpu_lineargradient_setup_attrib_locations,
  gsk_gpu_lineargradient_setup_vao
};

void
gsk_adjust_hue (GdkColorState       *ics,
                GskHueInterpolation  interp,
                const float          color1[4],
                float                color2[4])
{
  float h1, h2;
  float d;

  if (!gdk_color_state_equal (ics, GDK_COLOR_STATE_OKLCH))
    return;

  h1 = color1[2];
  h2 = color2[2];
  d = h2 - h1;

  while (d > 360)
    {
      h2 -= 360;
      d = h2 - h1;
    }
  while (d < -360)
    {
      h2 += 360;
      d = h2 - h1;
    }

  g_assert (fabsf (d) <= 360);

  switch (interp)
    {
    case GSK_HUE_INTERPOLATION_SHORTER:
      {
        if (d > 180)
          h2 -= 360;
        else if (d < -180)
          h2 += 360;
      }
      g_assert (fabsf (h2 - h1) <= 180);
      break;

   case GSK_HUE_INTERPOLATION_LONGER:
      {
        if (0 < d && d < 180)
          h2 -= 360;
        else if (-180 < d && d <= 0)
          h2 += 360;
      g_assert (fabsf (h2 - h1) >= 180);
      }
      break;

    case GSK_HUE_INTERPOLATION_INCREASING:
      if (h2 < h1)
        h2 += 360;
      d = h2 - h1;
      g_assert (h1 <= h2);
      break;

    case GSK_HUE_INTERPOLATION_DECREASING:
      if (h1 < h2)
        h2 -= 360;
      d = h2 - h1;
      g_assert (h1 >= h2);
      break;

    default:
      g_assert_not_reached ();
    }

  color2[2] = h2;
}

void
gsk_gpu_linear_gradient_op (GskGpuFrame            *frame,
                            GskGpuShaderClip        clip,
                            GdkColorState          *ccs,
                            float                   opacity,
                            const graphene_point_t *offset,
                            GdkColorState          *ics,
                            GskHueInterpolation     hue_interp,
                            gboolean                repeating,
                            const graphene_rect_t  *rect,
                            const graphene_point_t *start,
                            const graphene_point_t *end,
                            const GskColorStop2    *stops,
                            gsize                   n_stops)
{
  GskGpuLineargradientInstance *instance;

  g_assert (n_stops > 1);
  g_assert (n_stops <= 7);

  /* Note: we pass TRUE for alt-premultiplied because the
   * vertex shader applies the alpha to the colors.
   */
  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_LINEAR_GRADIENT_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE, ics, TRUE),
                           (repeating ? VARIATION_REPEATING : 0) |
                           (gsk_gpu_frame_should_optimize (frame, GSK_GPU_OPTIMIZE_GRADIENTS) ? VARIATION_SUPERSAMPLING : 0),
                           clip,
                           NULL,
                           NULL,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_point_to_float (start, offset, instance->startend);
  gsk_gpu_point_to_float (end, offset, &instance->startend[2]);
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 6)].color, ics, opacity, instance->color6);
  instance->offsets1[2] = stops[MIN (n_stops - 1, 6)].offset;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 5)].color, ics, opacity, instance->color5);
  instance->offsets1[1] = stops[MIN (n_stops - 1, 5)].offset;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 4)].color, ics, opacity, instance->color4);
  instance->offsets1[0] = stops[MIN (n_stops - 1, 4)].offset;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 3)].color, ics, opacity, instance->color3);
  instance->offsets0[3] = stops[MIN (n_stops - 1, 3)].offset;
  gsk_gpu_color_to_float (&stops[MIN (n_stops - 1, 2)].color, ics, opacity, instance->color2);
  instance->offsets0[2] = stops[MIN (n_stops - 1, 2)].offset;
  gsk_gpu_color_to_float (&stops[1].color, ics, opacity, instance->color1);
  instance->offsets0[1] = stops[1].offset;
  gsk_gpu_color_to_float (&stops[0].color, ics, opacity, instance->color0);
  instance->offsets0[0] = stops[0].offset;

  gsk_adjust_hue (ics, hue_interp, instance->color0, instance->color1);
  gsk_adjust_hue (ics, hue_interp, instance->color1, instance->color2);
  gsk_adjust_hue (ics, hue_interp, instance->color2, instance->color3);
  gsk_adjust_hue (ics, hue_interp, instance->color3, instance->color4);
  gsk_adjust_hue (ics, hue_interp, instance->color4, instance->color5);
  gsk_adjust_hue (ics, hue_interp, instance->color5, instance->color6);
}
