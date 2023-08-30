#include "config.h"

#include "gskgpupatternprivate.h"

#include "gskrendernode.h"

static gboolean
gsk_gpu_pattern_create_for_color_node (GskGpuBufferWriter *writer,
                                       GskRenderNode      *node)
{
  const GdkRGBA *rgba;

  rgba = gsk_color_node_get_color (node);

  gsk_gpu_buffer_writer_append_uint (writer, GSK_GPU_PATTERN_COLOR);
  gsk_gpu_buffer_writer_append_float (writer, rgba->red);
  gsk_gpu_buffer_writer_append_float (writer, rgba->green);
  gsk_gpu_buffer_writer_append_float (writer, rgba->blue);
  gsk_gpu_buffer_writer_append_float (writer, rgba->alpha);

  return TRUE;
}

static const struct
{
  gboolean              (* create_for_node)                     (GskGpuBufferWriter     *writer,
                                                                 GskRenderNode          *node);
} nodes_vtable[] = {
  [GSK_NOT_A_RENDER_NODE] = {
    NULL,
  },
  [GSK_CONTAINER_NODE] = {
    NULL,
  },
  [GSK_CAIRO_NODE] = {
    NULL,
  },
  [GSK_COLOR_NODE] = {
    gsk_gpu_pattern_create_for_color_node,
  },
  [GSK_LINEAR_GRADIENT_NODE] = {
    NULL,
  },
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = {
    NULL,
  },
  [GSK_RADIAL_GRADIENT_NODE] = {
    NULL,
  },
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = {
    NULL,
  },
  [GSK_CONIC_GRADIENT_NODE] = {
    NULL,
  },
  [GSK_BORDER_NODE] = {
    NULL,
  },
  [GSK_TEXTURE_NODE] = {
    NULL,
  },
  [GSK_INSET_SHADOW_NODE] = {
    NULL,
  },
  [GSK_OUTSET_SHADOW_NODE] = {
    NULL,
  },
  [GSK_TRANSFORM_NODE] = {
    NULL,
  },
  [GSK_OPACITY_NODE] = {
    NULL,
  },
  [GSK_COLOR_MATRIX_NODE] = {
    NULL,
  },
  [GSK_REPEAT_NODE] = {
    NULL,
  },
  [GSK_CLIP_NODE] = {
    NULL,
  },
  [GSK_ROUNDED_CLIP_NODE] = {
    NULL,
  },
  [GSK_SHADOW_NODE] = {
    NULL,
  },
  [GSK_BLEND_NODE] = {
    NULL,
  },
  [GSK_CROSS_FADE_NODE] = {
    NULL,
  },
  [GSK_TEXT_NODE] = {
    NULL,
  },
  [GSK_BLUR_NODE] = {
    NULL,
  },
  [GSK_DEBUG_NODE] = {
    NULL,
  },
  [GSK_GL_SHADER_NODE] = {
    NULL,
  },
  [GSK_TEXTURE_SCALE_NODE] = {
    NULL,
  },
  [GSK_MASK_NODE] = {
    NULL,
  },
  [GSK_FILL_NODE] = {
    NULL,
  },
  [GSK_STROKE_NODE] = {
    NULL,
  },
};

gboolean
gsk_gpu_pattern_create_for_node (GskGpuBufferWriter *writer,
                                 GskRenderNode      *node)
{
  GskRenderNodeType node_type;

  node_type = gsk_render_node_get_node_type (node);
  if (node_type >= G_N_ELEMENTS (nodes_vtable))
    {
      g_critical ("unkonwn node type %u for %s", node_type, g_type_name_from_instance ((GTypeInstance *) node));
      return FALSE;
    }

  if (nodes_vtable[node_type].create_for_node == NULL)
    return FALSE;

  return nodes_vtable[node_type].create_for_node (writer, node);
}
