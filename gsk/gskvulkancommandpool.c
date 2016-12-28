#include "config.h"

#include "gskvulkancommandpoolprivate.h"
#include "gskvulkanpipelineprivate.h"

struct _GskVulkanCommandPool
{
  GdkVulkanContext *vulkan;

  VkCommandPool vk_command_pool;
};

GskVulkanCommandPool *
gsk_vulkan_command_pool_new (GdkVulkanContext *context)
{
  GskVulkanCommandPool *self;

  self = g_slice_new0 (GskVulkanCommandPool);

  self->vulkan = g_object_ref (context);

  GSK_VK_CHECK (vkCreateCommandPool, gdk_vulkan_context_get_device (context),
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = gdk_vulkan_context_get_queue_family_index (self->vulkan),
                                         .flags = 0
                                     },
                                     NULL,
                                     &self->vk_command_pool);

  return self;
}

void
gsk_vulkan_command_pool_free (GskVulkanCommandPool *self)
{
  vkDestroyCommandPool (gdk_vulkan_context_get_device (self->vulkan),
                        self->vk_command_pool,
                        NULL);

  g_slice_free (GskVulkanCommandPool, self);
}

void
gsk_vulkan_command_pool_reset (GskVulkanCommandPool *self)
{
  GSK_VK_CHECK (vkResetCommandPool, gdk_vulkan_context_get_device (self->vulkan),
                                    self->vk_command_pool,
                                    0);
}

VkCommandBuffer
gsk_vulkan_command_pool_get_buffer (GskVulkanCommandPool *self)
{
  VkCommandBuffer command_buffer;

  GSK_VK_CHECK (vkAllocateCommandBuffers, gdk_vulkan_context_get_device (self->vulkan),
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = self->vk_command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &command_buffer);

  GSK_VK_CHECK (vkBeginCommandBuffer, command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = 0
                                      });

  return command_buffer;
}

void
gsk_vulkan_command_pool_submit_buffer (GskVulkanCommandPool *self,
                                       VkCommandBuffer       command_buffer,
                                       VkFence               fence)
{
  GSK_VK_CHECK (vkEndCommandBuffer, command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (self->vulkan),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
/*
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  },
*/
                                  .pWaitDstStageMask = (VkPipelineStageFlags []) {
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  },
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = (VkCommandBuffer[1]) {
                                      command_buffer
                                  },
/*
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = (VkSemaphore[1]) {
                                      gdk_vulkan_context_get_draw_semaphore (self->vulkan)
                                  }
*/
                               },
                               fence);
}

