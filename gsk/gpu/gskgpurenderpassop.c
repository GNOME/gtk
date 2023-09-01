#include "config.h"

#include "gskgpurenderpassopprivate.h"

#include "gskglimageprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskgpunodeprocessorprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskrendernodeprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanimageprivate.h"
#endif

typedef struct _GskGpuRenderPassOp GskGpuRenderPassOp;

struct _GskGpuRenderPassOp
{
  GskGpuOp op;

  GskGpuImage *target;
  cairo_rectangle_int_t area;
  GskRenderPassType pass_type;
};

static void
gsk_gpu_render_pass_op_finish (GskGpuOp *op)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;

  g_object_unref (self->target);
}

static void
gsk_gpu_render_pass_op_print (GskGpuOp    *op,
                              GskGpuFrame *frame,
                              GString     *string,
                              guint        indent)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;

  gsk_gpu_print_op (string, indent, "begin-render-pass");
  gsk_gpu_print_image (string, self->target);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static VkImageLayout
gsk_gpu_render_pass_type_to_vk_image_layout (GskRenderPassType type)
{
  switch (type)
  {
    default:
      g_assert_not_reached ();
      G_GNUC_FALLTHROUGH;
    case GSK_RENDER_PASS_PRESENT:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case GSK_RENDER_PASS_OFFSCREEN:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
}

static void
gsk_gpu_render_pass_op_do_barriers (GskGpuRenderPassOp *self,
                                       VkCommandBuffer        command_buffer)
{
  GskGpuShaderOp *shader;
  GskGpuOp *op;
  gsize i;

  for (op = ((GskGpuOp *) self)->next;
       op->op_class->stage != GSK_GPU_STAGE_END_PASS;
       op = op->next)
    {
      if (op->op_class->stage != GSK_GPU_STAGE_SHADER)
        continue;

      shader = (GskGpuShaderOp *) op;

      for (i = 0; i < shader->n_images; i++)
        {
          gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (shader->images[i].image),
                                       command_buffer,
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       VK_ACCESS_SHADER_READ_BIT);
        }
    }
}

static GskGpuOp *
gsk_gpu_render_pass_op_vk_command (GskGpuOp        *op,
                                   GskGpuFrame     *frame,
                                   VkRenderPass     render_pass,
                                   VkFormat         format,
                                   VkCommandBuffer  command_buffer)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;
  VkRenderPass vk_render_pass;
  VkFormat vk_format;

  /* nesting frame passes not allowed */
  g_assert (render_pass == VK_NULL_HANDLE);

  gsk_gpu_render_pass_op_do_barriers (self, command_buffer);

  vk_format = gsk_vulkan_image_get_vk_format (GSK_VULKAN_IMAGE (self->target));
  vk_render_pass = gsk_vulkan_device_get_vk_render_pass (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                         vk_format,
                                                         gsk_vulkan_image_get_vk_image_layout (GSK_VULKAN_IMAGE (self->target)),
                                                         gsk_gpu_render_pass_type_to_vk_image_layout (self->pass_type));


  vkCmdSetViewport (command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                        .x = 0,
                        .y = 0,
                        .width = gsk_gpu_image_get_width (self->target),
                        .height = gsk_gpu_image_get_height (self->target),
                        .minDepth = 0,
                        .maxDepth = 1
                    });

  vkCmdBeginRenderPass (command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = vk_render_pass,
                            .framebuffer = gsk_vulkan_image_get_vk_framebuffer (GSK_VULKAN_IMAGE(self->target),
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
  while (op->op_class->stage != GSK_GPU_STAGE_END_PASS)
    {
      op = gsk_gpu_op_vk_command (op, frame, vk_render_pass, vk_format, command_buffer);
    }

  op = gsk_gpu_op_vk_command (op, frame, vk_render_pass, vk_format, command_buffer);

  return op;
}
#endif

static GskGpuOp *
gsk_gpu_render_pass_op_gl_command (GskGpuOp    *op,
                                   GskGpuFrame *frame,
                                   gsize        flip_y)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;
  gsize target_flip_y;

  gsk_gl_image_bind_framebuffer (GSK_GL_IMAGE (self->target));

  if (gsk_gl_image_is_flipped (GSK_GL_IMAGE (self->target)))
    target_flip_y = gsk_gpu_image_get_height (self->target);
  else
    target_flip_y = 0;

  glViewport (0, 0,
              gsk_gpu_image_get_width (self->target),
              gsk_gpu_image_get_height (self->target));

  if (target_flip_y)
    glScissor (self->area.x, target_flip_y - self->area.y - self->area.height, self->area.width, self->area.height);
  else
    glScissor (self->area.x, self->area.y, self->area.width, self->area.height);
  glClearColor (0, 0, 0, 0);
  glClear (GL_COLOR_BUFFER_BIT);

  op = op->next;
  while (op->op_class->stage != GSK_GPU_STAGE_END_PASS)
    {
      op = gsk_gpu_op_gl_command (op, frame, target_flip_y);
    }

  op = gsk_gpu_op_gl_command (op, frame, target_flip_y);

  return op;
}

static const GskGpuOpClass GSK_GPU_RENDER_PASS_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuRenderPassOp),
  GSK_GPU_STAGE_BEGIN_PASS,
  gsk_gpu_render_pass_op_finish,
  gsk_gpu_render_pass_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_render_pass_op_vk_command,
#endif
  gsk_gpu_render_pass_op_gl_command
};

typedef struct _GskGpuFramePassEndOp GskGpuFramePassEndOp;

struct _GskGpuFramePassEndOp
{
  GskGpuOp op;

  GskGpuImage *target;
  GskRenderPassType pass_type;
};

static void
gsk_gpu_render_pass_end_op_finish (GskGpuOp *op)
{
  GskGpuFramePassEndOp *self = (GskGpuFramePassEndOp *) op;

  g_object_unref (self->target);
}

static void
gsk_gpu_render_pass_end_op_print (GskGpuOp    *op,
                                  GskGpuFrame *frame,
                                  GString     *string,
                                  guint        indent)
{
  GskGpuFramePassEndOp *self = (GskGpuFramePassEndOp *) op;

  gsk_gpu_print_op (string, indent, "end-render-pass");
  gsk_gpu_print_image (string, self->target);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_render_pass_end_op_vk_command (GskGpuOp        *op,
                                       GskGpuFrame     *frame,
                                       VkRenderPass     render_pass,
                                       VkFormat         format,
                                       VkCommandBuffer  command_buffer)
{
  GskGpuFramePassEndOp *self = (GskGpuFramePassEndOp *) op;

  vkCmdEndRenderPass (command_buffer);

  gsk_vulkan_image_set_vk_image_layout (GSK_VULKAN_IMAGE (self->target),
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        gsk_gpu_render_pass_type_to_vk_image_layout (self->pass_type),
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_render_pass_end_op_gl_command (GskGpuOp    *op,
                                       GskGpuFrame *frame,
                                       gsize        flip_y)
{
  /* nothing to do here */

  return op->next;
}

static const GskGpuOpClass GSK_GPU_RENDER_PASS_END_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuFramePassEndOp),
  GSK_GPU_STAGE_END_PASS,
  gsk_gpu_render_pass_end_op_finish,
  gsk_gpu_render_pass_end_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_render_pass_end_op_vk_command,
#endif
  gsk_gpu_render_pass_end_op_gl_command
};

void
gsk_gpu_render_pass_begin_op (GskGpuFrame                 *frame,
                              GskGpuImage                 *image,
                              const cairo_rectangle_int_t *area,
                              GskRenderPassType            pass_type)
{
  GskGpuRenderPassOp *self;

  self = (GskGpuRenderPassOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_RENDER_PASS_OP_CLASS);

  self->target = g_object_ref (image);
  self->area = *area;
  self->pass_type = pass_type;
}

void
gsk_gpu_render_pass_end_op (GskGpuFrame       *frame,
                            GskGpuImage       *image,
                            GskRenderPassType  pass_type)
{
  GskGpuFramePassEndOp *self;

  self = (GskGpuFramePassEndOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_RENDER_PASS_END_OP_CLASS);

  self->target = g_object_ref (image);
  self->pass_type = pass_type;
}

GskGpuImage *
gsk_gpu_render_pass_op_offscreen (GskGpuFrame           *frame,
                                  const graphene_vec2_t *scale,
                                  const graphene_rect_t *viewport,
                                  GskRenderNode         *node)
{
  GskGpuImage *image;
  int width, height;

  width = ceil (graphene_vec2_get_x (scale) * viewport->size.width);
  height = ceil (graphene_vec2_get_y (scale) * viewport->size.height);

  image = gsk_gpu_device_create_offscreen_image (gsk_gpu_frame_get_device (frame),
                                                 gsk_render_node_get_preferred_depth (node),
                                                 width, height);

  gsk_gpu_render_pass_begin_op (frame,
                                image,
                                &(cairo_rectangle_int_t) { 0, 0, width, height },
                                GSK_RENDER_PASS_OFFSCREEN);

  gsk_gpu_node_processor_process (frame,
                                  image,
                                  &(cairo_rectangle_int_t) { 0, 0, width, height },
                                  node,
                                  viewport);

  gsk_gpu_render_pass_end_op (frame,
                              image,
                              GSK_RENDER_PASS_OFFSCREEN);

  return image;
}
