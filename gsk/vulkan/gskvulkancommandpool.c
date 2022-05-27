#include "config.h"

#include "gskvulkancommandpoolprivate.h"
#include "gskvulkanpipelineprivate.h"

struct _GskVulkanCommandPool
{
  GdkVulkanContext *vulkan;

  VkCommandPool vk_command_pool;
  GPtrArray *buffers;
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

  self->buffers = g_ptr_array_new ();

  return self;
}

static void
gsk_vulkan_command_pool_free_buffers (GskVulkanCommandPool *self)
{
  if (self->buffers->len != 0)
    vkFreeCommandBuffers (gdk_vulkan_context_get_device (self->vulkan),
                          self->vk_command_pool,
                          self->buffers->len,
                          (VkCommandBuffer *) self->buffers->pdata);

  g_ptr_array_set_size (self->buffers, 0);
}

void
gsk_vulkan_command_pool_free (GskVulkanCommandPool *self)
{
  gsk_vulkan_command_pool_free_buffers (self);
  g_ptr_array_unref (self->buffers);

  vkDestroyCommandPool (gdk_vulkan_context_get_device (self->vulkan),
                        self->vk_command_pool,
                        NULL);

  g_slice_free (GskVulkanCommandPool, self);
}

void
gsk_vulkan_command_pool_reset (GskVulkanCommandPool *self)
{
  gsk_vulkan_command_pool_free_buffers (self);

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
  g_ptr_array_add (self->buffers, command_buffer);

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
                                       gsize                 wait_semaphore_count,
                                       VkSemaphore          *wait_semaphores,
                                       gsize                 signal_semaphore_count,
                                       VkSemaphore          *signal_semaphores,
                                       VkFence               fence)
{
  VkPipelineStageFlags *wait_semaphore_flags = NULL;

  GSK_VK_CHECK (vkEndCommandBuffer, command_buffer);

  if (wait_semaphore_count > 0)
    {
      wait_semaphore_flags = alloca (sizeof (VkPipelineStageFlags) * wait_semaphore_count);
      for (int i = 0; i < wait_semaphore_count; i++)
        wait_semaphore_flags[i] = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

  GSK_VK_CHECK (vkQueueSubmit, gdk_vulkan_context_get_queue (self->vulkan),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .waitSemaphoreCount = wait_semaphore_count,
                                  .pWaitSemaphores = wait_semaphores,
                                  .pWaitDstStageMask = wait_semaphore_flags,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = (VkCommandBuffer[1]) {
                                      command_buffer
                                  },
                                  .signalSemaphoreCount = signal_semaphore_count,
                                  .pSignalSemaphores = signal_semaphores,
                               },
                               fence);
}

