#include "config.h"

#include "gskgpuborderopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpushaderopprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include "gpu/shaders/gskgpuborderinstance.h"

typedef struct _GskGpuBorderOp GskGpuBorderOp;

struct _GskGpuBorderOp
{
  GskGpuShaderOp op;
};

static gboolean
color_equal (const float *color1,
             const float *color2)
{
  return gdk_rgba_equal (&(GdkRGBA) { color1[0], color1[1], color1[2], color1[3] },
                         &(GdkRGBA) { color2[0], color2[1], color2[2], color2[3] });
}

static void
gsk_gpu_border_op_print_instance (GskGpuShaderOp *shader,
                                  gpointer        instance_,
                                  GString        *string)
{
  GskGpuBorderInstance *instance = (GskGpuBorderInstance *) instance_;

  gsk_gpu_print_rounded_rect (string, instance->outline);

  gsk_gpu_print_rgba (string, (const float *) &instance->border_colors[0]);
  if (!color_equal (&instance->border_colors[12], &instance->border_colors[0]) ||
      !color_equal (&instance->border_colors[8], &instance->border_colors[0]) ||
      !color_equal (&instance->border_colors[4], &instance->border_colors[0]))
    {
      gsk_gpu_print_rgba (string, &instance->border_colors[4]);
      gsk_gpu_print_rgba (string, &instance->border_colors[8]);
      gsk_gpu_print_rgba (string, &instance->border_colors[12]);
    }
  g_string_append_printf (string, "%g ", instance->border_widths[0]);
  if (instance->border_widths[0] != instance->border_widths[1] ||
      instance->border_widths[0] != instance->border_widths[2] ||
      instance->border_widths[0] != instance->border_widths[3])
    {
      g_string_append_printf (string, "%g %g %g ",
                              instance->border_widths[1],
                              instance->border_widths[2],
                              instance->border_widths[3]);
    }
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_border_op_vk_command (GskGpuOp              *op,
                              GskGpuFrame           *frame,
                              GskVulkanCommandState *state)
{
  return gsk_gpu_shader_op_vk_command_n (op, frame, state, 8);
}
#endif

static GskGpuOp *
gsk_gpu_border_op_gl_command (GskGpuOp          *op,
                              GskGpuFrame       *frame,
                              GskGLCommandState *state)
{
  return gsk_gpu_shader_op_gl_command_n (op, frame, state, 8);
}

static const GskGpuShaderOpClass GSK_GPU_BORDER_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuBorderOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_border_op_vk_command,
#endif
    gsk_gpu_border_op_gl_command
  },
  "gskgpuborder",
  gsk_gpu_border_n_textures,
  sizeof (GskGpuBorderInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_border_info,
#endif
  gsk_gpu_border_op_print_instance,
  gsk_gpu_border_setup_attrib_locations,
  gsk_gpu_border_setup_vao
};

void
gsk_gpu_border_op (GskGpuFrame            *frame,
                   GskGpuShaderClip        clip,
                   GdkColorState          *ccs,
                   float                   opacity,
                   const graphene_point_t *offset,
                   const GskRoundedRect   *outline,
                   const graphene_point_t *inside_offset,
                   const float             widths[4],
                   const GdkColor          colors[4])
{
  GskGpuBorderInstance *instance;
  guint i;
  GdkColorState *alt;

  if (GDK_IS_DEFAULT_COLOR_STATE (colors[0].color_state) &&
      gdk_color_state_equal (colors[0].color_state, colors[1].color_state) &&
      gdk_color_state_equal (colors[0].color_state, colors[2].color_state) &&
      gdk_color_state_equal (colors[0].color_state, colors[3].color_state))
    alt = colors[0].color_state;
  else
    alt = ccs;

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_BORDER_OP_CLASS,
                           gsk_gpu_color_states_create (ccs, TRUE, alt, FALSE),
                           0,
                           clip,
                           NULL,
                           NULL,
                           &instance);

  gsk_rounded_rect_to_float (outline, offset, instance->outline);

  for (i = 0; i < 4; i++)
    {
      instance->border_widths[i] = widths[i];
      gsk_gpu_color_to_float (&colors[i], alt, opacity, &instance->border_colors[4 * i]);
    }
  instance->offset[0] = inside_offset->x;
  instance->offset[1] = inside_offset->y;
}

