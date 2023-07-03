#include "config.h"

#include "gskvulkancoloropprivate.h"

#include "vulkan/resources/color.vert.h"

typedef struct _GskVulkanColorOp GskVulkanColorOp;

struct _GskVulkanColorOp
{
  GskVulkanOp op;

  graphene_rect_t rect;
  GdkRGBA color;

  gsize vertex_offset;
};

static void
gsk_vulkan_color_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_color_op_upload (GskVulkanOp           *op,
                            GskVulkanRenderPass   *pass,
                            GskVulkanRender       *render,
                            GskVulkanUploader     *uploader)
{
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

static gsize
gsk_vulkan_color_op_count_vertex_data (GskVulkanOp *op,
                                       gsize        n_bytes)
{
  GskVulkanColorOp *self = (GskVulkanColorOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_color_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_color_op_collect_vertex_data (GskVulkanOp         *op,
                                         GskVulkanRenderPass *pass,
                                         GskVulkanRender     *render,
                                         guchar              *data)
{
  GskVulkanColorOp *self = (GskVulkanColorOp *) op;
  GskVulkanColorInstance *instance = (GskVulkanColorInstance *) (data + self->vertex_offset);

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

static void
gsk_vulkan_color_op_command (GskVulkanOp      *op,
                             GskVulkanRender  *render,
                             VkPipelineLayout  pipeline_layout,
                             VkCommandBuffer   command_buffer)
{
  GskVulkanColorOp *self = (GskVulkanColorOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_color_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_COLOR_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanColorOp),
  "color",
  &gsk_vulkan_color_info,
  gsk_vulkan_color_op_finish,
  gsk_vulkan_color_op_upload,
  gsk_vulkan_color_op_count_vertex_data,
  gsk_vulkan_color_op_collect_vertex_data,
  gsk_vulkan_color_op_reserve_descriptor_sets,
  gsk_vulkan_color_op_command
};

void
gsk_vulkan_color_op (GskVulkanRenderPass    *render_pass,
                     const char             *clip_type,
                     const graphene_rect_t  *rect,
                     const graphene_point_t *offset,
                     const GdkRGBA          *color)
{
  GskVulkanColorOp *self;

  self = (GskVulkanColorOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_COLOR_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  self->color = *color;
}
