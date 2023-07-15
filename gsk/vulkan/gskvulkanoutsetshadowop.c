#include "config.h"

#include "gskvulkanoutsetshadowopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include "vulkan/resources/outset-shadow.vert.h"

typedef struct _GskVulkanOutsetShadowOp GskVulkanOutsetShadowOp;

struct _GskVulkanOutsetShadowOp
{
  GskVulkanShaderOp op;

  GskRoundedRect outline;
  GdkRGBA color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_vulkan_outset_shadow_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_outset_shadow_op_print (GskVulkanOp *op,
                                   GString     *string,
                                   guint        indent)
{
  GskVulkanOutsetShadowOp *self = (GskVulkanOutsetShadowOp *) op;

  print_indent (string, indent);
  print_rounded_rect (string, &self->outline);
  g_string_append (string, "outset-shadow ");
  if (self->blur_radius > 0)
    g_string_append_printf (string, "blur %gpx ", self->blur_radius);
  print_newline (string);
}

static void
gsk_vulkan_outset_shadow_op_collect_vertex_data (GskVulkanOp *op,
                                                 guchar      *data)
{
  GskVulkanOutsetShadowOp *self = (GskVulkanOutsetShadowOp *) op;
  GskVulkanOutsetShadowInstance *instance = (GskVulkanOutsetShadowInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_rounded_rect_to_float (&self->outline, graphene_point_zero (), instance->outline);
  gsk_vulkan_rgba_to_float (&self->color, instance->color);
  gsk_vulkan_point_to_float (&self->offset, instance->offset);
  instance->spread = self->spread;
  instance->blur_radius = self->blur_radius;
}

static void
gsk_vulkan_outset_shadow_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                     GskVulkanRender *render)
{
}

static const GskVulkanShaderOpClass GSK_VULKAN_OUTSET_SHADOW_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanOutsetShadowOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_outset_shadow_op_finish,
    gsk_vulkan_outset_shadow_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_outset_shadow_op_collect_vertex_data,
    gsk_vulkan_outset_shadow_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "outset-shadow",
  0,
  &gsk_vulkan_outset_shadow_info,
};

void
gsk_vulkan_outset_shadow_op (GskVulkanRender         *render,
                             GskVulkanShaderClip      clip,
                             const GskRoundedRect    *outline,
                             const graphene_point_t  *offset,
                             const GdkRGBA           *color,
                             const graphene_point_t  *shadow_offset,
                             float                    spread,
                             float                    blur_radius)
{
  GskVulkanOutsetShadowOp *self;

  self = (GskVulkanOutsetShadowOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_OUTSET_SHADOW_OP_CLASS, clip, NULL);

  self->outline = *outline;
  gsk_rounded_rect_offset (&self->outline, offset->x, offset->y);
  self->color = *color;
  self->offset = *shadow_offset;
  self->spread = spread;
  self->blur_radius = blur_radius;
}

