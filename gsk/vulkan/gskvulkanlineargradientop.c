#include "config.h"

#include "gskvulkanlineargradientopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/linear.vert.h"

typedef struct _GskVulkanLinearGradientOp GskVulkanLinearGradientOp;

struct _GskVulkanLinearGradientOp
{
  GskVulkanShaderOp op;

  graphene_rect_t rect;
  graphene_point_t start;
  graphene_point_t end;
  gboolean repeating;
  GskColorStop *stops;
  gsize n_stops;

  gsize buffer_offset;
};

static void
gsk_vulkan_linear_gradient_op_finish (GskVulkanOp *op)
{
  GskVulkanLinearGradientOp *self = (GskVulkanLinearGradientOp *) op;

  g_free (self->stops);
}

static void
gsk_vulkan_linear_gradient_op_print (GskVulkanOp *op,
                                     GString     *string,
                                     guint        indent)
{
  GskVulkanLinearGradientOp *self = (GskVulkanLinearGradientOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append_printf (string, "linear-gradient (%zu stops)", self->n_stops);
  print_newline (string);
}

static void
gsk_vulkan_linear_gradient_op_collect_vertex_data (GskVulkanOp *op,
                                                   guchar      *data)
{
  GskVulkanLinearGradientOp *self = (GskVulkanLinearGradientOp *) op;
  GskVulkanLinearInstance *instance = (GskVulkanLinearInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->rect, instance->rect);
  gsk_vulkan_point_to_float (&self->start, instance->start);
  gsk_vulkan_point_to_float (&self->end, instance->end);
  instance->repeating = self->repeating;
  instance->stop_offset = self->buffer_offset;
  instance->stop_count = self->n_stops;
}

static void
gsk_vulkan_linear_gradient_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                       GskVulkanRender *render)
{
  GskVulkanLinearGradientOp *self = (GskVulkanLinearGradientOp *) op;
  guchar *mem;

  mem = gsk_vulkan_render_get_buffer_memory (render,
                                             self->n_stops * sizeof (GskColorStop),
                                             G_ALIGNOF (GskColorStop),
                                             &self->buffer_offset);
  memcpy (mem, self->stops, self->n_stops * sizeof (GskColorStop));
}

static const GskVulkanShaderOpClass GSK_VULKAN_LINEAR_GRADIENT_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanLinearGradientOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_linear_gradient_op_finish,
    gsk_vulkan_linear_gradient_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_linear_gradient_op_collect_vertex_data,
    gsk_vulkan_linear_gradient_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "linear",
  0,
  &gsk_vulkan_linear_info,
};

void
gsk_vulkan_linear_gradient_op (GskVulkanRender        *render,
                               GskVulkanShaderClip     clip,
                               const graphene_rect_t  *rect,
                               const graphene_point_t *offset,
                               const graphene_point_t *start,
                               const graphene_point_t *end,
                               gboolean                repeating,
                               const GskColorStop     *stops,
                               gsize                   n_stops)
{
  GskVulkanLinearGradientOp *self;

  self = (GskVulkanLinearGradientOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_LINEAR_GRADIENT_OP_CLASS, clip, NULL);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  self->start = GRAPHENE_POINT_INIT (start->x + offset->x, start->y + offset->y);
  self->end = GRAPHENE_POINT_INIT (end->x + offset->x, end->y + offset->y);
  self->repeating = repeating;
  self->stops = g_memdup (stops, sizeof (GskColorStop) * n_stops);
  self->n_stops = n_stops;
}
