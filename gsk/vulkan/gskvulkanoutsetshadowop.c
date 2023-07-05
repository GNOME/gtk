#include "config.h"

#include "gskvulkanoutsetshadowopprivate.h"

#include "gskvulkanprivate.h"
#include "gsk/gskroundedrectprivate.h"

#include "vulkan/resources/outset-shadow.vert.h"

typedef struct _GskVulkanOutsetShadowOp GskVulkanOutsetShadowOp;

struct _GskVulkanOutsetShadowOp
{
  GskVulkanOp op;

  GskRoundedRect outline;
  GdkRGBA color;
  graphene_point_t offset;
  float spread;
  float blur_radius;

  gsize vertex_offset;
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
gsk_vulkan_outset_shadow_op_upload (GskVulkanOp           *op,
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
gsk_vulkan_outset_shadow_op_count_vertex_data (GskVulkanOp *op,
                                               gsize        n_bytes)
{
  GskVulkanOutsetShadowOp *self = (GskVulkanOutsetShadowOp *) op;
  gsize vertex_stride;

  vertex_stride = gsk_vulkan_outset_shadow_info.pVertexBindingDescriptions[0].stride;
  n_bytes = round_up (n_bytes, vertex_stride);
  self->vertex_offset = n_bytes;
  n_bytes += vertex_stride;
  return n_bytes;
}

static void
gsk_vulkan_outset_shadow_op_collect_vertex_data (GskVulkanOp         *op,
                                                 GskVulkanRenderPass *pass,
                                                 GskVulkanRender     *render,
                                                 guchar              *data)
{
  GskVulkanOutsetShadowOp *self = (GskVulkanOutsetShadowOp *) op;
  GskVulkanOutsetShadowInstance *instance = (GskVulkanOutsetShadowInstance *) (data + self->vertex_offset);

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

static void
gsk_vulkan_outset_shadow_op_command (GskVulkanOp      *op,
                                     GskVulkanRender *render,
                                     VkPipelineLayout  pipeline_layout,
                                     VkCommandBuffer   command_buffer)
{
  GskVulkanOutsetShadowOp *self = (GskVulkanOutsetShadowOp *) op;

  vkCmdDraw (command_buffer,
             6, 1,
             0, self->vertex_offset / gsk_vulkan_outset_shadow_info.pVertexBindingDescriptions[0].stride);
}

static const GskVulkanOpClass GSK_VULKAN_OUTSET_SHADOW_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanOutsetShadowOp),
  "outset-shadow",
  &gsk_vulkan_outset_shadow_info,
  gsk_vulkan_outset_shadow_op_finish,
  gsk_vulkan_outset_shadow_op_print,
  gsk_vulkan_outset_shadow_op_upload,
  gsk_vulkan_outset_shadow_op_count_vertex_data,
  gsk_vulkan_outset_shadow_op_collect_vertex_data,
  gsk_vulkan_outset_shadow_op_reserve_descriptor_sets,
  gsk_vulkan_outset_shadow_op_command
};

void
gsk_vulkan_outset_shadow_op (GskVulkanRenderPass     *render_pass,
                             const char              *clip_type,
                             const GskRoundedRect    *outline,
                             const graphene_point_t  *offset,
                             const GdkRGBA           *color,
                             const graphene_point_t  *shadow_offset,
                             float                    spread,
                             float                    blur_radius)
{
  GskVulkanOutsetShadowOp *self;

  self = (GskVulkanOutsetShadowOp *) gsk_vulkan_op_alloc (render_pass, &GSK_VULKAN_OUTSET_SHADOW_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  self->outline = *outline;
  gsk_rounded_rect_offset (&self->outline, offset->x, offset->y);
  self->color = *color;
  self->offset = *shadow_offset;
  self->spread = spread;
  self->blur_radius = blur_radius;
}

