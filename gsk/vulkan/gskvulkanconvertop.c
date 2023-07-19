#include "config.h"

#include "gskvulkanconvertopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/convert.vert.h"

typedef struct _GskVulkanConvertOp GskVulkanConvertOp;

struct _GskVulkanConvertOp
{
  GskVulkanShaderOp op;

  graphene_rect_t rect;
  graphene_rect_t tex_rect;

  guint32 image_descriptor;
};

static void
gsk_vulkan_convert_op_print (GskVulkanOp *op,
                             GString     *string,
                             guint        indent)
{
  GskVulkanConvertOp *self = (GskVulkanConvertOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append (string, "convert ");
  print_image (string, shader->images[0]);
  print_newline (string);
}

static void
gsk_vulkan_convert_op_collect_vertex_data (GskVulkanOp *op,
                                           guchar      *data)
{
  GskVulkanConvertOp *self = (GskVulkanConvertOp *) op;
  GskVulkanConvertInstance *instance = (GskVulkanConvertInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  instance->rect[0] = self->rect.origin.x;
  instance->rect[1] = self->rect.origin.y;
  instance->rect[2] = self->rect.size.width;
  instance->rect[3] = self->rect.size.height;
  instance->tex_rect[0] = self->tex_rect.origin.x;
  instance->tex_rect[1] = self->tex_rect.origin.y;
  instance->tex_rect[2] = self->tex_rect.size.width;
  instance->tex_rect[3] = self->tex_rect.size.height;
  instance->tex_id = self->image_descriptor;
  instance->postprocess = gsk_vulkan_image_get_postprocess (shader->images[0]);
}

static void
gsk_vulkan_convert_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                               GskVulkanRender *render)
{
  GskVulkanConvertOp *self = (GskVulkanConvertOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, shader->images[0], GSK_VULKAN_SAMPLER_NEAREST);
}

static const GskVulkanShaderOpClass GSK_VULKAN_CONVERT_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanConvertOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_convert_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_convert_op_collect_vertex_data,
    gsk_vulkan_convert_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "convert",
  1,
  &gsk_vulkan_convert_info,
};

void
gsk_vulkan_convert_op (GskVulkanRender        *render,
                       GskVulkanShaderClip     clip,
                       GskVulkanImage         *image,
                       const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       const graphene_rect_t  *tex_rect)
{
  GskVulkanConvertOp *self;

  self = (GskVulkanConvertOp *) gsk_vulkan_shader_op_alloc (render,
                                                            &GSK_VULKAN_CONVERT_OP_CLASS,
                                                            clip,
                                                            &image);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
}
