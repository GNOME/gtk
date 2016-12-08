#include "config.h"

#include "gskvulkanrenderprivate.h"

#include "gskrendererprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderpassprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

static void
gsk_vulkan_render_compute_mvp (GskVulkanRender *self)
{
  GdkWindow *window = gsk_renderer_get_window (self->renderer);
  graphene_matrix_t modelview, projection;
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (gdk_drawing_context_get_clip (gsk_renderer_get_drawing_context (self->renderer)),
                            &extents);

  self->scale_factor = gsk_renderer_get_scale_factor (self->renderer);
  self->size.width = gdk_window_get_width (window) * self->scale_factor;
  self->size.height = gdk_window_get_height (window) * self->scale_factor;
  self->scissor.offset.x = extents.x * self->scale_factor;
  self->scissor.offset.y = extents.y * self->scale_factor;
  self->scissor.extent.width = extents.width * self->scale_factor;
  self->scissor.extent.height = extents.height * self->scale_factor;

  graphene_matrix_init_scale (&modelview, self->scale_factor, self->scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              0, self->size.width,
                              0, self->size.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);
  graphene_matrix_multiply (&modelview, &projection, &self->mvp);
}

void
gsk_vulkan_render_init (GskVulkanRender    *self,
                        GskRenderer        *renderer,
                        GdkVulkanContext   *context,
                        VkCommandPool       command_pool)
{
  self->vulkan = context;
  self->renderer = renderer;
  self->command_pool = command_pool;

  self->render_passes = NULL;
  self->cleanup_images = NULL;

  gsk_vulkan_render_compute_mvp (self);

  GSK_VK_CHECK (vkAllocateCommandBuffers, gdk_vulkan_context_get_device (self->vulkan),
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = self->command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->command_buffer);

  GSK_VK_CHECK (vkBeginCommandBuffer, self->command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = 0
                                      });
}

void
gsk_vulkan_render_add_cleanup_image (GskVulkanRender *self,
                                     GskVulkanImage  *image)
{
  self->cleanup_images = g_slist_prepend (self->cleanup_images, image);
}

void
gsk_vulkan_render_add_node (GskVulkanRender *self,
                            GskRenderNode   *node)
{
  GskVulkanRenderPass *pass = gsk_vulkan_render_pass_new (self->vulkan);

  self->render_passes = g_slist_prepend (self->render_passes, pass);

  gsk_vulkan_render_pass_add_node (pass, self, node);
}

void
gsk_vulkan_render_upload (GskVulkanRender *self)
{
  GSList *l;

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_upload (l->data, self);
    }
}

static gsize
gsk_vulkan_renderer_count_vertices (GskVulkanRender *self)
{
  gsize count;
  GSList *l;

  count = 0;
  for (l = self->render_passes; l; l = l->next)
    {
      count += gsk_vulkan_render_pass_count_vertices (l->data);
    }

  return count;
}

static GskVulkanBuffer *
gsk_vulkan_render_collect_vertices (GskVulkanRender *self)
{
  GskVulkanBuffer *buffer;
  GskVulkanVertex *vertices;
  GSList *l;
  gsize offset, count;
  
  offset = 0;
  count = gsk_vulkan_renderer_count_vertices (self);
  buffer = gsk_vulkan_buffer_new (self->vulkan, sizeof (GskVulkanVertex) * count);
  vertices = (GskVulkanVertex *) gsk_vulkan_buffer_map (buffer);

  for (l = self->render_passes; l; l = l->next)
    {
      offset += gsk_vulkan_render_pass_collect_vertices (l->data, vertices, offset, count - offset);
      g_assert (offset <= count);
    }

  gsk_vulkan_buffer_unmap (buffer);

  return buffer;
}

void
gsk_vulkan_render_draw (GskVulkanRender   *self,
                        GskVulkanPipeline *pipeline,
                        VkRenderPass       render_pass,
                        VkFramebuffer      framebuffer,
                        VkDescriptorSet    descriptor_set,
                        VkSampler          sampler)
{
  GskVulkanBuffer *buffer;
  GSList *l;

  buffer = gsk_vulkan_render_collect_vertices (self);

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_update_descriptor_sets (l->data, descriptor_set, sampler);
    }

  vkCmdSetViewport (self->command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                      .x = 0,
                      .y = 0,
                      .width = self->size.width,
                      .height = self->size.height,
                      .minDepth = 0,
                      .maxDepth = 1
                    });

  vkCmdSetScissor (self->command_buffer,
                   0,
                   1,
                   &self->scissor);

  vkCmdBeginRenderPass (self->command_buffer,
                        &(VkRenderPassBeginInfo) {
                            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                            .renderPass = render_pass,
                            .framebuffer = framebuffer,
                            .renderArea = { 
                                { 0, 0 },
                                { self->size.width, self->size.height }
                            },
                            .clearValueCount = 1,
                            .pClearValues = (VkClearValue [1]) {
                                { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                            }
                        },
                        VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline (self->command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                     gsk_vulkan_pipeline_get_pipeline (pipeline));

  vkCmdBindDescriptorSets (self->command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           gsk_vulkan_pipeline_get_pipeline_layout (pipeline),
                           0,
                           1,
                           &descriptor_set,
                           0,
                           NULL);

  vkCmdBindVertexBuffers (self->command_buffer,
                          0,
                          1,
                          (VkBuffer[1]) {
                              gsk_vulkan_buffer_get_buffer (buffer)
                          },
                          (VkDeviceSize[1]) { 0 });

  vkCmdPushConstants (self->command_buffer,
                      gsk_vulkan_pipeline_get_pipeline_layout (pipeline),
                      VK_SHADER_STAGE_VERTEX_BIT,
                      0,
                      sizeof (graphene_matrix_t),
                      &self->mvp);

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_draw (l->data, self);
    }

  vkCmdEndRenderPass (self->command_buffer);

  gsk_vulkan_buffer_free (buffer);
}

void
gsk_vulkan_render_submit (GskVulkanRender *self,
                          VkFence          fence)
{
  GSK_VK_CHECK (vkEndCommandBuffer, self->command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (self->vulkan),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  },
                                  .pWaitDstStageMask = (VkPipelineStageFlags []) {
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  },
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->command_buffer,
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  }
                               },
                               fence);

  GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                 1,
                                 &fence,
                                 VK_TRUE,
                                 INT64_MAX);
  GSK_VK_CHECK (vkResetFences, gdk_vulkan_context_get_device (self->vulkan),
                               1,
                               &fence);
}

void
gsk_vulkan_render_finish (GskVulkanRender *self)
{
  GSK_VK_CHECK (vkResetCommandPool, gdk_vulkan_context_get_device (self->vulkan),
                                    self->command_pool,
                                    0);

  g_slist_free_full (self->render_passes, (GDestroyNotify) gsk_vulkan_render_pass_free);
  g_slist_free_full (self->cleanup_images, (GDestroyNotify) gsk_vulkan_image_free);
}
