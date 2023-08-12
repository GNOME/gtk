#include "config.h"

#include "gskvulkanbufferprivate.h"

#include "gskvulkandeviceprivate.h"
#include "gskvulkanmemoryprivate.h"

struct _GskVulkanBuffer
{
  GskVulkanDevice *device;

  VkBuffer vk_buffer;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

static GskVulkanBuffer *
gsk_vulkan_buffer_new_internal (GskVulkanDevice   *device,
                                gsize              size,
                                VkBufferUsageFlags usage)
{
  VkMemoryRequirements requirements;
  GskVulkanBuffer *self;

  self = g_new0 (GskVulkanBuffer, 1);

  self->device = g_object_ref (device);

  GSK_VK_CHECK (vkCreateBuffer, gsk_vulkan_device_get_vk_device (device),
                                &(VkBufferCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                    .size = size,
                                    .flags = 0,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
                                },
                                NULL,
                                &self->vk_buffer);

  vkGetBufferMemoryRequirements (gsk_vulkan_device_get_vk_device (device),
                                 self->vk_buffer,
                                 &requirements);
  
  self->allocator = gsk_vulkan_device_find_allocator (device,
                                                      requirements.memoryTypeBits,
                                                      GSK_VULKAN_MEMORY_MAPPABLE,
                                                      GSK_VULKAN_MEMORY_MAPPABLE);
  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  GSK_VK_CHECK (vkBindBufferMemory, gsk_vulkan_device_get_vk_device (device),
                                    self->vk_buffer,
                                    self->allocation.vk_memory,
                                    self->allocation.offset);

  return self;
}

GskVulkanBuffer *
gsk_vulkan_buffer_new (GskVulkanDevice *device,
                       gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size,
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

GskVulkanBuffer *
gsk_vulkan_buffer_new_storage (GskVulkanDevice *device,
                               gsize            size)
{
  return gsk_vulkan_buffer_new_internal (device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

GskVulkanBuffer *
gsk_vulkan_buffer_new_map (GskVulkanDevice  *device,
                           gsize             size,
                           GskVulkanMapMode  mode)
{
  return gsk_vulkan_buffer_new_internal (device,
                                         size,
                                         (mode & GSK_VULKAN_READ ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0) |
                                         (mode & GSK_VULKAN_WRITE ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0));
}

void
gsk_vulkan_buffer_free (GskVulkanBuffer *self)
{
  vkDestroyBuffer (gsk_vulkan_device_get_vk_device (self->device),
                   self->vk_buffer,
                   NULL);

  gsk_vulkan_free (self->allocator, &self->allocation);

  g_object_unref (self->device);

  g_free (self);
}

VkBuffer
gsk_vulkan_buffer_get_buffer (GskVulkanBuffer *self)
{
  return self->vk_buffer;
}

gsize
gsk_vulkan_buffer_get_size (GskVulkanBuffer *self)
{
  return self->allocation.size;
}

guchar *
gsk_vulkan_buffer_get_data (GskVulkanBuffer *self)
{
  return self->allocation.map;
}

