#include "config.h"

#include "gskvulkanborderopprivate.h"

#include "vulkan/resources/border.vert.h"

#include "gsk/gskroundedrectprivate.h"

typedef struct _GskVulkanBorderOp GskVulkanBorderOp;

struct _GskVulkanBorderOp
{
  GskVulkanOp op;

  GskRoundedRect outline;
  float widths[4];
  GdkRGBA colors[4];

  gsize vertex_offset;
};

static void
gsk_vulkan_border_op_finish (GskVulkanOp *op)
{
}

static void
gsk_vulkan_border_op_upload (GskVulkanOp           *op,
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
gsk_vulkan_border_op_count_vertex_data (GskVulkanOp *op,
                                        gsize        n_bytes)
{
  GskVulkanBorderOp *self = (GskVulkanBorderOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_border_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_border_op_collect_vertex_data (GskVulkanOp         *op,
                                          GskVulkanRenderPass *pass,
                                          GskVulkanRender     *render,
                                          guchar              *data)
{
  GskVulkanBorderOp *self = (GskVulkanBorderOp *) op;
  GskVulkanBorderInstance *instance = (GskVulkanBorderInstance *) (data + self->vertex_offset);
  guint i;

  gsk_rounded_rect_to_float (&self->outline, graphene_point_zero (), instance->rect);
  for (i = 0; i < 4; i++)
    {
      instance->border_widths[i] = self->widths[i];
      gsk_vulkan_rgba_to_float (&self->colors[i], (gpointer) &instance->border_colors[4 * i]);
    }
}

static void
gsk_vulkan_border_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                              GskVulkanRender *render)
{
}

static VkPipeline
gsk_vulkan_border_op_get_pipeline (GskVulkanOp *op)
{
  return VK_NULL_HANDLE;
}

static void
gsk_vulkan_border_op_command (GskVulkanOp      *op,
                              GskVulkanRender *render,
                              VkPipelineLayout  pipeline_layout,
                              VkCommandBuffer   command_buffer)
{
  GskVulkanBorderOp *self = (GskVulkanBorderOp *) op;

  vkCmdDraw (command_buffer,
             6 * 8, 1,
             0, self->vertex_offset / gsk_vulkan_border_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_BORDER_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanBorderOp),
  "border",
  &gsk_vulkan_border_info,
  gsk_vulkan_border_op_finish,
  gsk_vulkan_border_op_upload,
  gsk_vulkan_border_op_count_vertex_data,
  gsk_vulkan_border_op_collect_vertex_data,
  gsk_vulkan_border_op_reserve_descriptor_sets,
  gsk_vulkan_border_op_get_pipeline,
  gsk_vulkan_border_op_command
};

void
gsk_vulkan_border_op (GskVulkanRenderPass     *render_pass,
                      const char              *clip_type,
                      const GskRoundedRect    *outline,
                      const graphene_point_t  *offset,
                      const float              widths[4],
                      const GdkRGBA            colors[4])
{
  GskVulkanBorderOp *self;
  guint i;

  self = (GskVulkanBorderOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_BORDER_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  self->outline = *outline;
  gsk_rounded_rect_offset (&self->outline, offset->x, offset->y);
  for (i = 0; i < 4; i++)
    {
      self->widths[i] = widths[i];
      self->colors[i] = colors[i];
    }
}

