#include "config.h"

#include "gskvulkantextureopprivate.h"

#include "gskvulkanprivate.h"

#include "vulkan/resources/texture.vert.h"

typedef struct _GskVulkanTextureOp GskVulkanTextureOp;

struct _GskVulkanTextureOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskVulkanRenderSampler sampler;
  graphene_rect_t rect;
  graphene_rect_t tex_rect;

  guint32 image_descriptor;
};

static void
gsk_vulkan_texture_op_finish (GskVulkanOp *op)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_texture_op_print (GskVulkanOp *op,
                             GString     *string,
                             guint        indent)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  print_indent (string, indent);
  print_rect (string, &self->rect);
  g_string_append (string, "texture ");
  print_image (string, self->image);
  print_newline (string);
}

static void
gsk_vulkan_texture_op_collect_vertex_data (GskVulkanOp *op,
                                           guchar      *data)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;
  GskVulkanTextureInstance *instance = (GskVulkanTextureInstance *) (data + op->vertex_offset);

  instance->rect[0] = self->rect.origin.x;
  instance->rect[1] = self->rect.origin.y;
  instance->rect[2] = self->rect.size.width;
  instance->rect[3] = self->rect.size.height;
  instance->tex_rect[0] = self->tex_rect.origin.x;
  instance->tex_rect[1] = self->tex_rect.origin.y;
  instance->tex_rect[2] = self->tex_rect.size.width;
  instance->tex_rect[3] = self->tex_rect.size.height;
  instance->tex_id = self->image_descriptor;
}

static void
gsk_vulkan_texture_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                               GskVulkanRender *render)
{
  GskVulkanTextureOp *self = (GskVulkanTextureOp *) op;

  self->image_descriptor = gsk_vulkan_render_get_image_descriptor (render, self->image, self->sampler);
}

static const GskVulkanOpClass GSK_VULKAN_TEXTURE_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanTextureOp),
  GSK_VULKAN_STAGE_COMMAND,
  "texture",
  &gsk_vulkan_texture_info,
  gsk_vulkan_texture_op_finish,
  gsk_vulkan_texture_op_print,
  gsk_vulkan_op_draw_upload,
  gsk_vulkan_op_draw_count_vertex_data,
  gsk_vulkan_texture_op_collect_vertex_data,
  gsk_vulkan_texture_op_reserve_descriptor_sets,
  gsk_vulkan_op_draw_command
};

void
gsk_vulkan_texture_op (GskVulkanRender        *render,
                       const char             *clip_type,
                       GskVulkanImage         *image,
                       GskVulkanRenderSampler  sampler,
                       const graphene_rect_t  *rect,
                       const graphene_point_t *offset,
                       const graphene_rect_t  *tex_rect)
{
  GskVulkanTextureOp *self;

  self = (GskVulkanTextureOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_TEXTURE_OP_CLASS);

  ((GskVulkanOp *) self)->clip_type = g_intern_string (clip_type);
  self->image = g_object_ref (image);
  self->sampler = sampler;
  graphene_rect_offset_r (rect, offset->x, offset->y, &self->rect);
  gsk_vulkan_normalize_tex_coords (&self->tex_rect, rect, tex_rect);
}
