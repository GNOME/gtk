#include "config.h"

#include "gskvulkanbluropprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/blur.vert.h"

typedef struct _GskVulkanBlurOp GskVulkanBlurOp;

struct _GskVulkanBlurOp
{
  GskVulkanShaderOp op;

  graphene_rect_t rect;
  graphene_rect_t tex_rect;
  float radius;

  guint32 image_descriptor;
};

static void
gsk_vulkan_blur_op_print (GskVulkanOp *op,
                          GString     *string,
                          guint        indent)
{
  GskVulkanBlurOp *self = (GskVulkanBlurOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append_printf (string, "blur %g",
                          self->radius);
  print_newline (string);
}

static void
gsk_vulkan_blur_op_collect_vertex_data (GskVulkanOp *op,
                                        guchar      *data)
{
  GskVulkanBlurOp *self = (GskVulkanBlurOp *) op;
  GskVulkanBlurInstance *instance = (GskVulkanBlurInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->rect, instance->rect);
  gsk_vulkan_rect_to_float (&self->tex_rect, instance->tex_rect);
  instance->tex_id = self->image_descriptor;
  instance->radius = self->radius;
}

static void
gsk_vulkan_blur_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                            GskVulkanRender *render)
{
  GskVulkanBlurOp *self = (GskVulkanBlurOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                   shader->images[0],
                                                                   GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_BLUR_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanBlurOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_blur_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_blur_op_collect_vertex_data,
    gsk_vulkan_blur_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "blur",
  1,
  &gsk_vulkan_blur_info,
};

void
gsk_vulkan_blur_op (GskVulkanRender         *render,
                    GskVulkanShaderClip      clip,
                    GskVulkanImage          *image,
                    const graphene_rect_t   *rect,
                    const graphene_point_t  *offset,
                    const graphene_rect_t   *tex_rect,
                    float                    radius)
{
  GskVulkanBlurOp *self;

  g_assert (radius > 0);

  self = (GskVulkanBlurOp *) gsk_vulkan_shader_op_alloc (render, &GSK_VULKAN_BLUR_OP_CLASS, clip, &image);

  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
  self->radius = radius;
}

