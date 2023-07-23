#include "config.h"

#include "gskvulkanbufferprivate.h"

#include "gskvulkanmemoryprivate.h"
#include "gskvulkanprivate.h"

struct _GskVulkanBuffer
{
  GdkVulkanContext *vulkan;

  VkBuffer vk_buffer;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

static GskVulkanBuffer *
gsk_vulkan_buffer_new_internal (GdkVulkanContext  *context,
                                gsize              size,
                                VkBufferUsageFlags usage)
{
  VkMemoryRequirements requirements;
  GskVulkanBuffer *self;

  self = g_new0 (GskVulkanBuffer, 1);

  self->vulkan = g_object_ref (context);

  GSK_VK_CHECK (vkCreateBuffer, gdk_vulkan_context_get_device (context),
                                &(VkBufferCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                    .size = size,
                                    .flags = 0,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
                                },
                                NULL,
                                &self->vk_buffer);

  vkGetBufferMemoryRequirements (gdk_vulkan_context_get_device (context),
                                 self->vk_buffer,
                                 &requirements);
  
  self->allocator = gsk_vulkan_find_allocator (context,
                                               requirements.memoryTypeBits,
                                               GSK_VULKAN_MEMORY_MAPPABLE,
                                               GSK_VULKAN_MEMORY_MAPPABLE);
  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  GSK_VK_CHECK (vkBindBufferMemory, gdk_vulkan_context_get_device (context),
                                    self->vk_buffer,
                                    self->allocation.vk_memory,
                                    self->allocation.offset);

  return self;
}

GskVulkanBuffer *
gsk_vulkan_buffer_new (GdkVulkanContext  *context,
                       gsize              size)
{
  return gsk_vulkan_buffer_new_internal (context, size,
                                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

GskVulkanBuffer *
gsk_vulkan_buffer_new_storage (GdkVulkanContext  *context,
                               gsize              size)
{
  return gsk_vulkan_buffer_new_internal (context, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

GskVulkanBuffer *
gsk_vulkan_buffer_new_map (GdkVulkanContext  *context,
                           gsize              size,
                           GskVulkanMapMode   mode)
{
  return gsk_vulkan_buffer_new_internal (context,
                                         size,
                                         (mode & GSK_VULKAN_READ ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0) |
                                         (mode & GSK_VULKAN_WRITE ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0));
}

void
gsk_vulkan_buffer_free (GskVulkanBuffer *self)
{
  vkDestroyBuffer (gdk_vulkan_context_get_device (self->vulkan),
                   self->vk_buffer,
                   NULL);

  gsk_vulkan_free (self->allocator, &self->allocation);

  g_object_unref (self->vulkan);

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

