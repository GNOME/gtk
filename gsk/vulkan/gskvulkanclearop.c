#include "config.h"

#include "gskvulkanclearopprivate.h"

#include "gskvulkanprivate.h"

typedef struct _GskVulkanClearOp GskVulkanClearOp;

struct _GskVulkanClearOp
{
  GskVulkanOp op;

  cairo_rectangle_int_t rect;
  GdkRGBA color;
};

static void
gsk_vulkan_clear_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_clear_op_print (GskVulkanOp *op,
                           GString     *string,
                           guint        indent)
{
  GskVulkanClearOp *self = (GskVulkanClearOp *) op;

  print_indent (string, indent);
  print_int_rect (string, &self->rect);
  g_string_append_printf (string, "clear ");
  print_rgba (string, &self->color);
  print_newline (string);
}

static gsize
gsk_vulkan_clear_op_count_vertex_data (GskVulkanOp *op,
                                       gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_clear_op_collect_vertex_data (GskVulkanOp *op,
                                         guchar      *data)
{
}

static void
gsk_vulkan_clear_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                             GskVulkanRender *render)
{
}

static void
gsk_vulkan_init_clear_value (VkClearValue  *value,
                             const GdkRGBA *rgba)
{
  gsk_vulkan_rgba_to_float (rgba, value->color.float32);
}

static GskVulkanOp *
gsk_vulkan_clear_op_command (GskVulkanOp      *op,
                             GskVulkanRender  *render,
                             VkRenderPass      render_pass,
                             VkCommandBuffer   command_buffer)
{
  GskVulkanClearOp *self = (GskVulkanClearOp *) op;
  VkClearValue clear_value;

  gsk_vulkan_init_clear_value (&clear_value, &self->color);

  vkCmdClearAttachments (command_buffer,
                         1,
                         &(VkClearAttachment) {
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           0,
                           clear_value,
                         },
                         1,
                         &(VkClearRect) {
                           {
                             { self->rect.x, self->rect.y },
                             { self->rect.width, self->rect.height },
                           },
                           0,
                           1
                         });

  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_SCISSOR_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanClearOp),
  GSK_VULKAN_STAGE_COMMAND,
  gsk_vulkan_clear_op_finish,
  gsk_vulkan_clear_op_print,
  gsk_vulkan_clear_op_count_vertex_data,
  gsk_vulkan_clear_op_collect_vertex_data,
  gsk_vulkan_clear_op_reserve_descriptor_sets,
  gsk_vulkan_clear_op_command
};

void
gsk_vulkan_clear_op (GskVulkanRender             *render,
                     const cairo_rectangle_int_t *rect,
                     const GdkRGBA               *color)
{
  GskVulkanClearOp *self;

  self = (GskVulkanClearOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_SCISSOR_OP_CLASS);

  self->rect = *rect;
  self->color = *color;
}
