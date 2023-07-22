#include "config.h"

#include "gskvulkanrenderpassopprivate.h"

#include "gskrendernodeprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

typedef struct _GskVulkanRenderPassOp GskVulkanRenderPassOp;

struct _GskVulkanRenderPassOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  cairo_rectangle_int_t area;

  VkImageLayout initial_layout;
  VkImageLayout final_layout;
};

static void
gsk_vulkan_render_pass_op_finish (GskVulkanOp *op)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_render_pass_op_print (GskVulkanOp *op,
                                 GString     *string,
                                 guint        indent)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "begin-render-pass %zux%zu ",
                          gsk_vulkan_image_get_width (self->image),
                          gsk_vulkan_image_get_height (self->image));
  print_newline (string);
}

static gsize
gsk_vulkan_render_pass_op_count_vertex_data (GskVulkanOp *op,
                                             gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_render_pass_op_collect_vertex_data (GskVulkanOp *op,
                                               guchar      *data)
{
}

static void
gsk_vulkan_render_pass_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                   GskVulkanRender *render)
{
}

static void
gsk_vulkan_render_pass_op_do_barriers (GskVulkanRenderPassOp *self,
                                       VkCommandBuffer        command_buffer)
{
  GskVulkanShaderOp *shader;
  GskVulkanOp *op;
  gsize i;

  for (op = ((GskVulkanOp *) self)->next;
       op->op_class->stage != GSK_VULKAN_STAGE_END_PASS;
       op = op->next)
    {
      if (op->op_class->stage != GSK_VULKAN_STAGE_SHADER)
        continue;

      shader = (GskVulkanShaderOp *) op;

      for (i = 0; i < ((GskVulkanShaderOpClass *) op->op_class)->n_images; i++)
        {
          gsk_vulkan_image_transition (shader->images[i],
                                       command_buffer,
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       VK_ACCESS_SHADER_READ_BIT);
        }
    }
}

static GskVulkanOp *
gsk_vulkan_render_pass_op_command (GskVulkanOp      *op,
                                   GskVulkanRender  *render,
                                   VkRenderPass      render_pass,
                                   VkCommandBuffer   command_buffer)
{
  GskVulkanRenderPassOp *self = (GskVulkanRenderPassOp *) op;
  VkRenderPass vk_render_pass;

  /* nesting render passes not allowed */
  g_assert (render_pass == VK_NULL_HANDLE);

  gsk_vulkan_render_pass_op_do_barriers (self, command_buffer);

  vk_render_pass = gsk_vulkan_render_get_render_pass (render,
                                                      gsk_vulkan_image_get_vk_format (self->image),
                                                      self->initial_layout,
                                                      self->final_layout);


  vkCmdSetViewport (command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                        .x = 0,
                        .y = 0,
                        .width = gsk_vulkan_image_get_width (self->image),
                        .height = gsk_vulkan_image_get_height (self->image),
                        .minDepth = 0,
                        .maxDepth = 1
                    });

  vkCmdBeginRenderPass (command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = vk_render_pass,
                            .framebuffer = gsk_vulkan_image_get_framebuffer (self->image,
                                                                             vk_render_pass),
                            .renderArea = { 
                                { self->area.x, self->area.y },
                                { self->area.width, self->area.height }
                            },
                            .clearValueCount = 1,
                            .pClearValues = (VkClearValue [1]) {
                                { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                            }
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  op = op->next;
  while (op->op_class->stage != GSK_VULKAN_STAGE_END_PASS)
    {
      op = gsk_vulkan_op_command (op, render, vk_render_pass, command_buffer);
    }

  op = gsk_vulkan_op_command (op, render, vk_render_pass, command_buffer);

  return op;
}

static const GskVulkanOpClass GSK_VULKAN_RENDER_PASS_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanRenderPassOp),
  GSK_VULKAN_STAGE_BEGIN_PASS,
  gsk_vulkan_render_pass_op_finish,
  gsk_vulkan_render_pass_op_print,
  gsk_vulkan_render_pass_op_count_vertex_data,
  gsk_vulkan_render_pass_op_collect_vertex_data,
  gsk_vulkan_render_pass_op_reserve_descriptor_sets,
  gsk_vulkan_render_pass_op_command
};

typedef struct _GskVulkanRenderPassEndOp GskVulkanRenderPassEndOp;

struct _GskVulkanRenderPassEndOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  VkImageLayout final_layout;
};

static void
gsk_vulkan_render_pass_end_op_finish (GskVulkanOp *op)
{
  GskVulkanRenderPassEndOp *self = (GskVulkanRenderPassEndOp *) op;

  g_object_unref (self->image);
}

static void
gsk_vulkan_render_pass_end_op_print (GskVulkanOp *op,
                                     GString     *string,
                                     guint        indent)
{
  GskVulkanRenderPassEndOp *self = (GskVulkanRenderPassEndOp *) op;

  print_indent (string, indent);
  g_string_append_printf (string, "end-render-pass ");
  print_image (string, self->image);
  print_newline (string);
}

static gsize
gsk_vulkan_render_pass_end_op_count_vertex_data (GskVulkanOp *op,
                                                 gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_render_pass_end_op_collect_vertex_data (GskVulkanOp *op,
                                                   guchar      *data)
{
}

static void
gsk_vulkan_render_pass_end_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                                       GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_render_pass_end_op_command (GskVulkanOp      *op,
                                       GskVulkanRender  *render,
                                       VkRenderPass      render_pass,
                                       VkCommandBuffer   command_buffer)
{
  GskVulkanRenderPassEndOp *self = (GskVulkanRenderPassEndOp *) op;

  vkCmdEndRenderPass (command_buffer);

  gsk_vulkan_image_set_vk_image_layout (self->image,
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        self->final_layout,
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  return op->next;
}

static const GskVulkanOpClass GSK_VULKAN_RENDER_PASS_END_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanRenderPassEndOp),
  GSK_VULKAN_STAGE_END_PASS,
  gsk_vulkan_render_pass_end_op_finish,
  gsk_vulkan_render_pass_end_op_print,
  gsk_vulkan_render_pass_end_op_count_vertex_data,
  gsk_vulkan_render_pass_end_op_collect_vertex_data,
  gsk_vulkan_render_pass_end_op_reserve_descriptor_sets,
  gsk_vulkan_render_pass_end_op_command
};

void
gsk_vulkan_render_pass_begin_op (GskVulkanRender             *render,
                                 GskVulkanImage              *image,
                                 const cairo_rectangle_int_t *area,
                                 VkImageLayout                initial_layout,
                                 VkImageLayout                final_layout)
{
  GskVulkanRenderPassOp *self;

  self = (GskVulkanRenderPassOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_RENDER_PASS_OP_CLASS);

  self->image = g_object_ref (image);
  self->initial_layout = initial_layout;
  self->final_layout = final_layout;
  self->area = *area;
}

void
gsk_vulkan_render_pass_end_op (GskVulkanRender *render,
                               GskVulkanImage  *image,
                               VkImageLayout    final_layout)
{
  GskVulkanRenderPassEndOp *self;

  self = (GskVulkanRenderPassEndOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_RENDER_PASS_END_OP_CLASS);

  self->image = g_object_ref (image);
  self->final_layout = final_layout;
}

GskVulkanImage *
gsk_vulkan_render_pass_op_offscreen (GskVulkanRender       *render,
                                     const graphene_vec2_t *scale,
                                     const graphene_rect_t *viewport,
                                     GskRenderNode         *node)
{
  GskVulkanRenderPass *render_pass;
  GdkVulkanContext *context;
  GskVulkanImage *image;
  int width, height;

  width = ceil (graphene_vec2_get_x (scale) * viewport->size.width);
  height = ceil (graphene_vec2_get_y (scale) * viewport->size.height);

  context = gsk_vulkan_render_get_context (render);
  image = gsk_vulkan_image_new_for_offscreen (context,
                                              gdk_vulkan_context_get_offscreen_format (context,
                                                  gsk_render_node_get_preferred_depth (node)),
                                              width, height);

  gsk_vulkan_render_pass_begin_op (render,
                                   image,
                                   &(cairo_rectangle_int_t) { 0, 0, width, height },
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  render_pass = gsk_vulkan_render_pass_new ();
  gsk_vulkan_render_pass_add (render_pass,
                              render,
                              width, height,
                              &(cairo_rectangle_int_t) { 0, 0, width, height },
                              node,
                              viewport);
  gsk_vulkan_render_pass_free (render_pass);

  gsk_vulkan_render_pass_end_op (render,
                                 image,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  g_object_unref (image);

  return image;
}
