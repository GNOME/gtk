#include "config.h"

#include "gskvulkancoloropprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/color.vert.h"

typedef struct _GskVulkanColorOp GskVulkanColorOp;

struct _GskVulkanColorOp
{
  GskVulkanShaderOp op;

  graphene_rect_t rect;
  GdkRGBA color;
};

static void
gsk_vulkan_color_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_color_op_print (GskVulkanOp *op,
                           GString     *string,
                           guint        indent)
{
  GskVulkanColorOp *self = (GskVulkanColorOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append (string, "color ");
  print_rgba (string, &self->color);
  print_newline (string);
}

static void
gsk_vulkan_color_op_collect_vertex_data (GskVulkanOp *op,
                                         guchar      *data)
{
  GskVulkanColorOp *self = (GskVulkanColorOp *) op;
  GskVulkanColorInstance *instance = (GskVulkanColorInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  instance->rect[0] = self->rect.origin.x;
  instance->rect[1] = self->rect.origin.y;
  instance->rect[2] = self->rect.size.width;
  instance->rect[3] = self->rect.size.height;
  instance->color[0] = self->color.red;
  instance->color[1] = self->color.green;
  instance->color[2] = self->color.blue;
  instance->color[3] = self->color.alpha;
}

static void
gsk_vulkan_color_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                             GskVulkanRender *render)
{
}

static const GskVulkanShaderOpClass GSK_VULKAN_COLOR_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanColorOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_color_op_finish,
    gsk_vulkan_color_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_color_op_collect_vertex_data,
    gsk_vulkan_color_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "color",
  0,
  &gsk_vulkan_color_info,
};

void
gsk_vulkan_color_op (GskVulkanRender        *render,
                     GskVulkanShaderClip     clip,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const GdkRGBA          *color)
{
  GskVulkanColorOp *self;

  self = (GskVulkanColorOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_COLOR_OP_CLASS, clip, NULL);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  self->color = *color;
}
