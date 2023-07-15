#include "config.h"

#include "gskvulkaninsetshadowopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include "vulkan/resources/inset-shadow.vert.h"

typedef struct _GskVulkanInsetShadowOp GskVulkanInsetShadowOp;

struct _GskVulkanInsetShadowOp
{
  GskVulkanShaderOp op;

  GskRoundedRect outline;
  GdkRGBA color;
  graphene_point_t offset;
  float spread;
  float blur_radius;
};

static void
gsk_vulkan_inset_shadow_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_inset_shadow_op_print (GskVulkanOp *op,
                                  GString     *string,
                                  guint        indent)
{
  GskVulkanInsetShadowOp *self = (GskVulkanInsetShadowOp *) op;

  print_indent (string, indent);
  print_rounded_rect (string, &self->outline);
  g_string_append (string, "inset-shadow ");
  if (self->blur_radius > 0)
    g_string_append_printf (string, "blur %gpx ", self->blur_radius);
  print_newline (string);
}

static void
gsk_vulkan_inset_shadow_op_collect_vertex_data (GskVulkanOp *op,
                                                guchar      *data)
{
  GskVulkanInsetShadowOp *self = (GskVulkanInsetShadowOp *) op;
  GskVulkanInsetShadowInstance *instance = (GskVulkanInsetShadowInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_rounded_rect_to_float (&self->outline, graphene_point_zero (), instance->outline);
  gsk_vulkan_rgba_to_float (&self->color, instance->color);
  gsk_vulkan_point_to_float (&self->offset, instance->offset);
  instance->spread = self->spread;
  instance->blur_radius = self->blur_radius;
}

static void
gsk_vulkan_inset_shadow_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                    GskVulkanRender *render)
{
}

static const GskVulkanShaderOpClass GSK_VULKAN_INSET_SHADOW_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanInsetShadowOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_inset_shadow_op_finish,
    gsk_vulkan_inset_shadow_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_inset_shadow_op_collect_vertex_data,
    gsk_vulkan_inset_shadow_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "inset-shadow",
  0,
  &gsk_vulkan_inset_shadow_info,
};

void
gsk_vulkan_inset_shadow_op (GskVulkanRender         *render,
                            GskVulkanShaderClip      clip,
                            const GskRoundedRect    *outline,
                            const graphene_point_t  *offset,
                            const GdkRGBA           *color,
                            const graphene_point_t  *shadow_offset,
                            float                    spread,
                            float                    blur_radius)
{
  GskVulkanInsetShadowOp *self;

  self = (GskVulkanInsetShadowOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_INSET_SHADOW_OP_CLASS, clip, NULL);

  self->outline = *outline;
  gsk_rounded_rect_offset (&self->outline, offset->x, offset->y);
  self->color = *color;
  self->offset = *shadow_offset;
  self->spread = spread;
  self->blur_radius = blur_radius;
}

