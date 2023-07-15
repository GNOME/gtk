#include "config.h"

#include "gskvulkancolormatrixopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/color-matrix.vert.h"

typedef struct _GskVulkanColorMatrixOp GskVulkanColorMatrixOp;

struct _GskVulkanColorMatrixOp
{
  GskVulkanShaderOp op;

  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;

  guint32 image_descriptor;
};

static void
gsk_vulkan_color_matrix_op_print (GskVulkanOp *op,
                                  GString     *string,
                                  guint        indent)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append (string, "color-matrix ");
  print_newline (string);
}

static void
gsk_vulkan_color_matrix_op_collect_vertex_data (GskVulkanOp *op,
                                                guchar      *data)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;
  GskVulkanColorMatrixInstance *instance = (GskVulkanColorMatrixInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  instance->rect[0] = self->rect.origin.x;
  instance->rect[1] = self->rect.origin.y;
  instance->rect[2] = self->rect.size.width;
  instance->rect[3] = self->rect.size.height;
  instance->tex_rect[0] = self->tex_rect.origin.x;
  instance->tex_rect[1] = self->tex_rect.origin.y;
  instance->tex_rect[2] = self->tex_rect.size.width;
  instance->tex_rect[3] = self->tex_rect.size.height;
  graphene_matrix_to_float (&self->color_matrix, instance->color_matrix);
  graphene_vec4_to_float (&self->color_offset, instance->color_offset);
  instance->tex_id = self->image_descriptor;
}

static void
gsk_vulkan_color_matrix_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                    GskVulkanRender *render)
{
  GskVulkanColorMatrixOp *self = (GskVulkanColorMatrixOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                   shader->images[0],
                                                                   GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_COLOR_MATRIX_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanColorMatrixOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_color_matrix_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_color_matrix_op_collect_vertex_data,
    gsk_vulkan_color_matrix_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "color-matrix",
  1,
  &gsk_vulkan_color_matrix_info,
};

void
gsk_vulkan_color_matrix_op (GskVulkanRender         *render,
                            GskVulkanShaderClip      clip,
                            GskVulkanImage          *image,
                            const graphene_rect_t   *rect,
                            const graphene_point_t  *offset,
                            const graphene_rect_t   *tex_rect,
                            const graphene_matrix_t *color_matrix,
                            const graphene_vec4_t   *color_offset)
{
  GskVulkanColorMatrixOp *self;

  self = (GskVulkanColorMatrixOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_COLOR_MATRIX_OP_CLASS, clip, &image);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
  self->color_matrix = *color_matrix;
  self->color_offset = *color_offset;
}

void
gsk_vulkan_color_matrix_op_opacity (GskVulkanRender        *render,
                                    GskVulkanShaderClip     clip,
                                    GskVulkanImage         *image,
                                    const graphene_rect_t  *rect,
                                    const graphene_point_t *offset,
                                    const graphene_rect_t  *tex_rect,
                                    float                   opacity)
{
  graphene_matrix_t color_matrix;
  graphene_vec4_t color_offset;

  graphene_matrix_init_from_float (&color_matrix,
                                   (float[16]) {
                                       1.0, 0.0, 0.0, 0.0,
                                       0.0, 1.0, 0.0, 0.0,
                                       0.0, 0.0, 1.0, 0.0,
                                       0.0, 0.0, 0.0, opacity
                                   });
  graphene_vec4_init (&color_offset, 0.0, 0.0, 0.0, 0.0);

  gsk_vulkan_color_matrix_op (render,
                              clip,
                              image,
                              rect,
                              offset,
                              tex_rect,
                              &color_matrix,
                              &color_offset);
}

