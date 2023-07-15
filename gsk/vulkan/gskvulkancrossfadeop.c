#include "config.h"

#include "gskvulkancrossfadeopprivate.h"

#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "vulkan/resources/cross-fade.vert.h"

typedef struct _GskVulkanCrossFadeOp GskVulkanCrossFadeOp;

struct _GskVulkanCrossFadeOp
{
  GskVulkanShaderOp op;

  graphene_rect_t bounds;
  float progress;

  struct {
    graphene_rect_t rect;
    graphene_rect_t tex_rect;
    guint32 image_descriptor;
  } start, end;
};

static void
gsk_vulkan_cross_fade_op_print (GskVulkanOp *op,
                                GString     *string,
                                guint        indent)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->bounds);
  g_string_append_printf (string, "cross-fade %d%% ", (int) (self->progress * 100 + 0.5));
  print_newline (string);
}

static void
gsk_vulkan_cross_fade_op_collect_vertex_data (GskVulkanOp *op,
                                              guchar      *data)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;
  GskVulkanCrossFadeInstance *instance = (GskVulkanCrossFadeInstance *) (data + ((GskVulkanShaderOp *) op)->vertex_offset);

  gsk_vulkan_rect_to_float (&self->bounds, instance->rect);
  gsk_vulkan_rect_to_float (&self->start.rect, instance->start_rect);
  gsk_vulkan_rect_to_float (&self->end.rect, instance->end_rect);
  gsk_vulkan_rect_to_float (&self->start.tex_rect, instance->start_tex_rect);
  gsk_vulkan_rect_to_float (&self->end.tex_rect, instance->end_tex_rect);

  instance->start_tex_id = self->start.image_descriptor;
  instance->end_tex_id = self->end.image_descriptor;
  instance->progress = self->progress;
}

static void
gsk_vulkan_cross_fade_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                  GskVulkanRender *render)
{
  GskVulkanCrossFadeOp *self = (GskVulkanCrossFadeOp *) op;
  GskVulkanShaderOp *shader = (GskVulkanShaderOp *) op;

  self->start.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                         shader->images[0],
                                                                         GSK_VULKAN_SAMPLER_DEFAULT);
  self->end.image_descriptor = gsk_vulkan_render_get_image_descriptor (render,
                                                                       shader->images[1],
                                                                       GSK_VULKAN_SAMPLER_DEFAULT);
}

static const GskVulkanShaderOpClass GSK_VULKAN_CROSS_FADE_OP_CLASS = {
  {
    GSK_VULKAN_OP_SIZE (GskVulkanCrossFadeOp),
    GSK_VULKAN_STAGE_SHADER,
    gsk_vulkan_shader_op_finish,
    gsk_vulkan_cross_fade_op_print,
    gsk_vulkan_shader_op_count_vertex_data,
    gsk_vulkan_cross_fade_op_collect_vertex_data,
    gsk_vulkan_cross_fade_op_reserve_descriptor_sets,
    gsk_vulkan_shader_op_command
  },
  "cross-fade",
  2,
  &gsk_vulkan_cross_fade_info,
};

void
gsk_vulkan_cross_fade_op (GskVulkanRender        *render,
                          GskVulkanShaderClip     clip,
                          const graphene_rect_t  *bounds,
                          const graphene_point_t *offset,
                          float                   progress,
                          GskVulkanImage         *start_image,
                          const graphene_rect_t  *start_rect,
                          const graphene_rect_t  *start_tex_rect,
                          GskVulkanImage         *end_image,
                          const graphene_rect_t  *end_rect,
                          const graphene_rect_t  *end_tex_rect)
{
  GskVulkanCrossFadeOp *self;

  self = (GskVulkanCrossFadeOp *) gsk_vulkan_shader_op_alloc (render,
                                                              &GSK_VULKAN_CROSS_FADE_OP_CLASS,
                                                              clip,
                                                              (GskVulkanImage *[2]) {
                                                                  start_image,
                                                                  end_image
                                                              });

  graphene_rect_offset_r (bounds, offset->x, offset->y, &self->bounds);
  self->progress = progress;

  graphene_rect_offset_r (start_rect, offset->x, offset->y, &self->start.rect);
  gsk_vulkan_normalize_tex_coords (&self->start.tex_rect, bounds, start_tex_rect);

  graphene_rect_offset_r (end_rect, offset->x, offset->y, &self->end.rect);
  gsk_vulkan_normalize_tex_coords (&self->end.tex_rect, bounds, end_tex_rect);
}
