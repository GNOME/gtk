#include "config.h"

#include "gskvulkanframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskvulkandeviceprivate.h"

#include "gdk/gdkdisplayprivate.h"

struct _GskVulkanFrame
{
  GskGpuFrame parent_instance;

  VkFence vk_fence;
  VkCommandBuffer vk_command_buffer;
};

struct _GskVulkanFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_vulkan_frame_is_busy (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice device;

  device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  return vkGetFenceStatus (device, self->vk_fence) == VK_NOT_READY;
}

static void
gsk_vulkan_frame_setup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  GSK_VK_CHECK (vkAllocateCommandBuffers, vk_device,
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = vk_command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->vk_command_buffer);

  GSK_VK_CHECK (vkCreateFence, vk_device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->vk_fence);
}

static void
gsk_vulkan_frame_cleanup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice device;

  device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  GSK_VK_CHECK (vkWaitForFences, device,
                                 1,
                                 &self->vk_fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, device,
                               1,
                               &self->vk_fence);

  GSK_VK_CHECK (vkResetCommandBuffer, self->vk_command_buffer,
                                      0);

  GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->cleanup (frame);
}

static void
gsk_vulkan_frame_submit (GskGpuFrame *frame,
                         GskGpuOp    *op)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);

  GSK_VK_CHECK (vkBeginCommandBuffer, self->vk_command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                      });

  while (op)
    {
      op = gsk_gpu_op_vk_command (op, frame, VK_NULL_HANDLE, self->vk_command_buffer);
    }

  GSK_VK_CHECK (vkEndCommandBuffer, self->vk_command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gsk_vulkan_device_get_vk_queue (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame))),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->vk_command_buffer,
                               },
                               self->vk_fence);
}

static void
gsk_vulkan_frame_finalize (GObject *object)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (object);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  vkFreeCommandBuffers (vk_device,
                        vk_command_pool,
                        1, &self->vk_command_buffer);
  vkDestroyFence (vk_device,
                  self->vk_fence,
                  NULL);

  G_OBJECT_CLASS (gsk_vulkan_frame_parent_class)->finalize (object);
}

static void
gsk_vulkan_frame_class_init (GskVulkanFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_vulkan_frame_is_busy;
  gpu_frame_class->setup = gsk_vulkan_frame_setup;
  gpu_frame_class->cleanup = gsk_vulkan_frame_cleanup;
  gpu_frame_class->submit = gsk_vulkan_frame_submit;

  object_class->finalize = gsk_vulkan_frame_finalize;
}

static void
gsk_vulkan_frame_init (GskVulkanFrame *self)
{
}

VkFence
gsk_vulkan_frame_get_vk_fence (GskVulkanFrame *self)
{
  return self->vk_fence;
}
