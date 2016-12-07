#include "config.h"

#include "gskvulkanrenderprivate.h"

#include "gskvulkanpipelineprivate.h"

void
gsk_vulkan_render_init (GskVulkanRender    *self,
                        GdkVulkanContext   *context,
                        VkCommandPool       command_pool)
{
  self->vulkan = context;
  self->command_pool = command_pool;

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
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan),
                                  },
                                  .pWaitDstStageMask = (VkPipelineStageFlags []) {
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  },
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->command_buffer,
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
}
