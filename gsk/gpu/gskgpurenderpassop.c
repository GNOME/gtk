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
  GskGpuLoadOp load_op;
  float clear_color[4];
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
  gsk_gpu_print_int_rect (string, &self->area);
  switch (self->load_op)
    {
      case GSK_GPU_LOAD_OP_LOAD:
        gsk_gpu_print_string (string, "load");
        break;
      case GSK_GPU_LOAD_OP_CLEAR:
        gsk_gpu_print_rgba (string, self->clear_color);
        break;
      case GSK_GPU_LOAD_OP_DONT_CARE:
        gsk_gpu_print_string (string, "dont-care");
        break;
      default:
        g_assert_not_reached ();
        break;
    }
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
    case GSK_RENDER_PASS_PRESENT:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case GSK_RENDER_PASS_OFFSCREEN:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case GSK_RENDER_PASS_EXPORT:
      return VK_IMAGE_LAYOUT_GENERAL;
  }
}

static void
gsk_gpu_render_pass_op_do_barriers (GskGpuRenderPassOp     *self,
                                    GskVulkanCommandState  *state)
{
  GskGpuShaderOp *shader;
  GskGpuOp *op;

  for (op = ((GskGpuOp *) self)->next;
       op->op_class->stage != GSK_GPU_STAGE_END_PASS;
       op = op->next)
    {
      if (op->op_class->stage != GSK_GPU_STAGE_SHADER)
        continue;

      shader = (GskGpuShaderOp *) op;

      if (shader->images[0])
        gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (shader->images[0]),
                                     state->semaphores,
                                     state->vk_command_buffer,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_ACCESS_SHADER_READ_BIT);
      if (shader->images[1])
        gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (shader->images[1]),
                                     state->semaphores,
                                     state->vk_command_buffer,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_ACCESS_SHADER_READ_BIT);
    }
}

static VkAttachmentLoadOp
gsk_gpu_load_op_to_vk_load_op (GskGpuLoadOp op)
{
  switch (op)
    {
      case GSK_GPU_LOAD_OP_LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
      case GSK_GPU_LOAD_OP_CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
      case GSK_GPU_LOAD_OP_DONT_CARE:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      default:
        g_return_val_if_reached (VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    }
}
static GskGpuOp *
gsk_gpu_render_pass_op_vk_command (GskGpuOp              *op,
                                   GskGpuFrame           *frame,
                                   GskVulkanCommandState *state)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;

  /* nesting frame passes not allowed */
  g_assert (state->vk_render_pass == VK_NULL_HANDLE);

  gsk_gpu_render_pass_op_do_barriers (self, state);

  state->vk_format = gsk_vulkan_image_get_vk_format (GSK_VULKAN_IMAGE (self->target));
  state->vk_render_pass = gsk_vulkan_device_get_vk_render_pass (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                                state->vk_format,
                                                                gsk_gpu_load_op_to_vk_load_op (self->load_op),
                                                                gsk_vulkan_image_get_vk_image_layout (GSK_VULKAN_IMAGE (self->target)),
                                                                gsk_gpu_render_pass_type_to_vk_image_layout (self->pass_type));


  vkCmdSetViewport (state->vk_command_buffer,
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

  vkCmdBeginRenderPass (state->vk_command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = state->vk_render_pass,
                            .framebuffer = gsk_vulkan_image_get_vk_framebuffer (GSK_VULKAN_IMAGE(self->target),
                                                                                state->vk_render_pass),
                            .renderArea = { 
                                { self->area.x, self->area.y },
                                { self->area.width, self->area.height }
                            },
                            .clearValueCount = self->load_op == GSK_GPU_LOAD_OP_CLEAR ? 1 : 0,
                            .pClearValues = self->load_op == GSK_GPU_LOAD_OP_CLEAR ? (VkClearValue [1]) {
                                {
                                    .color = {
                                        .float32 = {
                                            self->clear_color[0],
                                            self->clear_color[1],
                                            self->clear_color[2],
                                            self->clear_color[3]
                                        }
                                    }
                                } 
                            } : NULL,
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  op = op->next;
  while (op->op_class->stage != GSK_GPU_STAGE_END_PASS)
    {
      op = gsk_gpu_op_vk_command (op, frame, state);
    }

  op = gsk_gpu_op_vk_command (op, frame, state);

  return op;
}
#endif

static GskGpuOp *
gsk_gpu_render_pass_op_gl_command (GskGpuOp          *op,
                                   GskGpuFrame       *frame,
                                   GskGLCommandState *state)
{
  GskGpuRenderPassOp *self = (GskGpuRenderPassOp *) op;

  /* nesting frame passes not allowed */
  g_assert (state->flip_y == 0);

  gsk_gl_image_bind_framebuffer (GSK_GL_IMAGE (self->target));

  if (gsk_gl_image_is_flipped (GSK_GL_IMAGE (self->target)))
    state->flip_y = gsk_gpu_image_get_height (self->target);
  else
    state->flip_y = 0;

  glViewport (0, 0,
              gsk_gpu_image_get_width (self->target),
              gsk_gpu_image_get_height (self->target));

  if (state->flip_y)
    glScissor (self->area.x, state->flip_y - self->area.y - self->area.height, self->area.width, self->area.height);
  else
    glScissor (self->area.x, self->area.y, self->area.width, self->area.height);
  if (self->load_op == GSK_GPU_LOAD_OP_CLEAR)
    {
      glClearColor (self->clear_color[0], self->clear_color[1], self->clear_color[2], self->clear_color[3]);
      glClear (GL_COLOR_BUFFER_BIT);
    }

  op = op->next;
  while (op->op_class->stage != GSK_GPU_STAGE_END_PASS)
    {
      op = gsk_gpu_op_gl_command (op, frame, state);
    }

  op = gsk_gpu_op_gl_command (op, frame, state);

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
gsk_gpu_render_pass_end_op_vk_command (GskGpuOp              *op,
                                       GskGpuFrame           *frame,
                                       GskVulkanCommandState *state)
{
  GskGpuFramePassEndOp *self = (GskGpuFramePassEndOp *) op;

  vkCmdEndRenderPass (state->vk_command_buffer);

  if ((gsk_gpu_image_get_flags (self->target) & GSK_GPU_IMAGE_CAN_MIPMAP) &&
      (gsk_gpu_image_get_width (self->target) > 1 ||
       gsk_gpu_image_get_height (self->target) > 1))
    {
      vkCmdPipelineBarrier (state->vk_command_buffer,
                            gsk_vulkan_image_get_vk_pipeline_stage (GSK_VULKAN_IMAGE (self->target)),
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0,
                            0, NULL,
                            0, NULL,
                            1, &(VkImageMemoryBarrier) {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = gsk_vulkan_image_get_vk_access (GSK_VULKAN_IMAGE (self->target)),
                                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                .oldLayout = gsk_vulkan_image_get_vk_image_layout (GSK_VULKAN_IMAGE (self->target)),
                                .newLayout = gsk_gpu_render_pass_type_to_vk_image_layout (self->pass_type),
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = gsk_vulkan_image_get_vk_image (GSK_VULKAN_IMAGE (self->target)),
                                .subresourceRange = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 1,
                                  .levelCount = VK_REMAINING_MIP_LEVELS,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1
                                },
                            });
        }

  gsk_vulkan_image_set_vk_image_layout (GSK_VULKAN_IMAGE (self->target),
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        gsk_gpu_render_pass_type_to_vk_image_layout (self->pass_type),
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

  state->vk_render_pass = VK_NULL_HANDLE;
  state->vk_format = VK_FORMAT_UNDEFINED;

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_render_pass_end_op_gl_command (GskGpuOp          *op,
                                       GskGpuFrame       *frame,
                                       GskGLCommandState *state)
{
  state->flip_y = 0;

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
                              GskGpuLoadOp                 load_op,
                              float                        clear_color[4],
                              GskRenderPassType            pass_type)
{
  GskGpuRenderPassOp *self;

  g_assert (load_op != GSK_GPU_LOAD_OP_CLEAR || clear_color != NULL);

  self = (GskGpuRenderPassOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_RENDER_PASS_OP_CLASS);

  self->target = g_object_ref (image);
  self->area = *area;
  self->load_op = load_op;
  if (self->load_op == GSK_GPU_LOAD_OP_CLEAR)
    gsk_gpu_vec4_to_float (clear_color, self->clear_color);
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
