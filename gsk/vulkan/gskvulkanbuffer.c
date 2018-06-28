#include "config.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanmemoryprivate.h"
#include "gskvulkanpipelineprivate.h"

struct _GskVulkanBuffer
{
  GdkVulkanContext *vulkan;

  gsize size;

  VkBuffer vk_buffer;

  GskVulkanMemory *memory;
};

static GskVulkanBuffer *
gsk_vulkan_buffer_new_internal (GdkVulkanContext  *context,
                                gsize              size,
                                VkBufferUsageFlags usage)
{
  VkMemoryRequirements requirements;
  GskVulkanBuffer *self;

  self = g_slice_new0 (GskVulkanBuffer);

  self->vulkan = g_object_ref (context);
  self->size = size;

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

  self->memory = gsk_vulkan_memory_new (context,
                                        requirements.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        size);

  GSK_VK_CHECK (vkBindBufferMemory, gdk_vulkan_context_get_device (context),
                                    self->vk_buffer,
                                    gsk_vulkan_memory_get_device_memory (self->memory),
                                    0);
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
gsk_vulkan_buffer_new_staging (GdkVulkanContext  *context,
                               gsize              size)
{
  return gsk_vulkan_buffer_new_internal (context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

GskVulkanBuffer *
gsk_vulkan_buffer_new_download (GdkVulkanContext  *context,
                                gsize              size)
{
  return gsk_vulkan_buffer_new_internal (context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}
void
gsk_vulkan_buffer_free (GskVulkanBuffer *self)
{
  vkDestroyBuffer (gdk_vulkan_context_get_device (self->vulkan),
                   self->vk_buffer,
                   NULL);

  gsk_vulkan_memory_free (self->memory);

  g_object_unref (self->vulkan);

  g_slice_free (GskVulkanBuffer, self);
}

VkBuffer
gsk_vulkan_buffer_get_buffer (GskVulkanBuffer *self)
{
  return self->vk_buffer;
}

guchar *
gsk_vulkan_buffer_map (GskVulkanBuffer *self)
{
  return gsk_vulkan_memory_map (self->memory);
}

void
gsk_vulkan_buffer_unmap (GskVulkanBuffer *self)
{
  gsk_vulkan_memory_unmap (self->memory);
}
