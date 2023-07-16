#include "config.h"

#include "gskvulkanfillopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "gskpath.h"

#include "vulkan/resources/fill.vert.h"

typedef struct _GskVulkanFillOp GskVulkanFillOp;

struct _GskVulkanFillOp
{
  GskVulkanShaderOp op;

  graphene_point_t offset;
  graphene_rect_t rect;
  GskPath *path;
  GskFillRule fill_rule;
  GdkRGBA color;

  gsize n_points;
  gsize buffer_offset;
};

static void
gsk_vulkan_fill_op_finish (GskVulkanOp *op)
{
  GskVulkanFillOp *self = (GskVulkanFillOp *) op;

  gsk_path_unref (self->path);
}

static void
gsk_vulkan_fill_op_print (GskVulkanOp *op,
                          GString     *string,
                          guint        indent)
{
  GskVulkanFillOp *self = (GskVulkanFillOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append_printf (string, "fill ");
  print_newline (string);
}

static void
gsk_vulkan_fill_op_collect_vertex_data (GskVulkanOp *op,
                                        guchar      *data)
{
  GskVulkanFillOp *self = (GskVulkanFillOp *) op;
  GskVulkanFillInstance *instance = (GskVulkanFillInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->rect, instance->rect);
  gsk_vulkan_rgba_to_float (&self->color, instance->color);
  gsk_vulkan_point_to_float (&self->offset, instance->offset);
  instance->points_id = self->buffer_offset;
  instance->n_points = self->n_points;
  instance->fill_rule = self->fill_rule;
}

static gboolean
gsk_vulkan_fill_op_path_foreach_cb (GskPathOperation        op,
                                    const graphene_point_t *pts,
                                    gsize                   n_pts,
                                    float                   weight,
                                    gpointer                array)
{
  g_array_append_val (array, pts[0].x);
  g_array_append_val (array, pts[0].y);
  return TRUE;
}

static void
gsk_vulkan_fill_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                            GskVulkanRender *render)
{
  GskVulkanFillOp *self = (GskVulkanFillOp *) op;
  GArray *points;
  guchar *mem;

  points = g_array_new (FALSE, FALSE, sizeof(float));
  gsk_path_foreach (self->path,
                    0,
                    gsk_vulkan_fill_op_path_foreach_cb,
                    points);
  self->n_points = points->len / 2;
  mem = gsk_vulkan_render_get_buffer_memory (render,
                                             points->len * sizeof (float),
                                             G_ALIGNOF (float),
                                             &self->buffer_offset);
  memcpy (mem, points->data, points->len * sizeof (float));
  g_array_free (points, TRUE);
}

static const GskVulkanShaderOpClass GSK_VULKAN_FILL_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanFillOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_fill_op_finish,
    gsk_vulkan_fill_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_fill_op_collect_vertex_data,
    gsk_vulkan_fill_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "fill",
  0,
  &gsk_vulkan_fill_info,
};

void
gsk_vulkan_fill_op (GskVulkanRender        *render,
                    GskVulkanShaderClip     clip,
                    const graphene_point_t *offset,
                    const graphene_rect_t  *rect,
                    GskPath                *path,
                    GskFillRule             fill_rule,
                    const GdkRGBA          *color)
{
  GskVulkanFillOp *self;

  self = (GskVulkanFillOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_FILL_OP_CLASS, clip, NULL);

  self->offset = *offset;
  self->rect = *rect;
  self->path = gsk_path_ref (path);
  self->fill_rule = fill_rule;
  self->color = *color;
}
