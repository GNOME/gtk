#include "config.h"

#include "gskvulkanstrokeopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "gskpathprivate.h"

#include "vulkan/resources/stroke.vert.h"

typedef struct _GskVulkanStrokeOp GskVulkanStrokeOp;

struct _GskVulkanStrokeOp
{
  GskVulkanShaderOp op;

  graphene_point_t offset;
  graphene_rect_t rect;
  GskPath *path;
  float line_width;
  GskLineCap line_cap;
  GskLineJoin line_join;
  float miter_limit;
  GdkRGBA color;

  gsize buffer_offset;
};

static void
gsk_vulkan_stroke_op_finish (GskVulkanOp *op)
{
  GskVulkanStrokeOp *self = (GskVulkanStrokeOp *) op;

  gsk_path_unref (self->path);
}

static void
gsk_vulkan_stroke_op_print (GskVulkanOp *op,
                            GString     *string,
                            guint        indent)
{
  GskVulkanStrokeOp *self = (GskVulkanStrokeOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append_printf (string, "stroke ");
  print_newline (string);
}

static void
gsk_vulkan_stroke_op_collect_vertex_data (GskVulkanOp *op,
                                          guchar      *data)
{
  GskVulkanStrokeOp *self = (GskVulkanStrokeOp *) op;
  GskVulkanStrokeInstance *instance = (GskVulkanStrokeInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->rect, instance->rect);
  gsk_vulkan_rgba_to_float (&self->color, instance->color);
  gsk_vulkan_point_to_float (&self->offset, instance->offset);
  instance->points_id = self->buffer_offset;
  instance->line_width = self->line_width;
  instance->line_cap = self->line_cap;
  instance->line_join = self->line_join;
  instance->miter_limit = self->miter_limit;
}

static void
gsk_vulkan_stroke_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                              GskVulkanRender *render)
{
  GskVulkanStrokeOp *self = (GskVulkanStrokeOp *) op;
  const GskContour *contour;
  gsize size, i, n;
  guchar *mem;

  size = sizeof (guint32);
  n = gsk_path_get_n_contours (self->path);
  for (i = 0; i < n; i++)
    {
      contour = gsk_path_get_contour (self->path, i);
      size += gsk_contour_get_shader_size (contour);
    }

  mem = gsk_vulkan_render_get_buffer_memory (render,
                                             size,
                                             G_ALIGNOF (float),
                                             &self->buffer_offset);

  *(guint32*) mem = n;
  mem += sizeof (guint32);

  for (i = 0; i < n; i++)
    {
      contour = gsk_path_get_contour (self->path, i);
      gsk_contour_to_shader (contour, mem);
      mem += gsk_contour_get_shader_size (contour);
    }
}

static const GskVulkanShaderOpClass GSK_VULKAN_STROKE_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanStrokeOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_stroke_op_finish,
    gsk_vulkan_stroke_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_stroke_op_collect_vertex_data,
    gsk_vulkan_stroke_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "stroke",
  0,
  &gsk_vulkan_stroke_info,
};

void
gsk_vulkan_stroke_op (GskVulkanRender        *render,
                      GskVulkanShaderClip     clip,
                      const graphene_point_t *offset,
                      const graphene_rect_t  *rect,
                      GskPath                *path,
                      float                   line_width,
                      GskLineCap              line_cap,
                      GskLineJoin             line_join,
                      float                   miter_limit,
                      const GdkRGBA          *color)
{
  GskVulkanStrokeOp *self;

  self = (GskVulkanStrokeOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_STROKE_OP_CLASS, clip, NULL);

  self->offset = *offset;
  self->rect = *rect;
  self->path = gsk_path_ref (path);
  self->line_width = line_width;
  self->line_cap = line_cap;
  self->line_join = line_join;
  self->miter_limit = miter_limit;
  self->color = *color;
}
