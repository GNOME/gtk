#include "config.h"

#include "gskvulkanmemoryprivate.h"

#include "gskvulkanprivate.h"

struct _GskVulkanMemory
{
  GdkVulkanContext *vulkan;

  gsize size;

  VkMemoryType vk_memory_type;
  VkDeviceMemory vk_memory;
};

GskVulkanMemory *
gsk_vulkan_memory_new (GdkVulkanContext      *context,
                       uint32_t               allowed_types,
                       VkMemoryPropertyFlags  flags,
                       gsize                  size)
{
  VkPhysicalDeviceMemoryProperties properties;
  GskVulkanMemory *self;
  uint32_t i;

  self = g_new0 (GskVulkanMemory, 1);

  self->vulkan = g_object_ref (context);
  self->size = size;

  vkGetPhysicalDeviceMemoryProperties (gdk_vulkan_context_get_physical_device (context),
                                       &properties);

  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if (!(allowed_types & (1 << i)))
        continue;

      if ((properties.memoryTypes[i].propertyFlags & flags) == flags)
        break;
  }

  g_assert (i < properties.memoryTypeCount);

  self->vk_memory_type = properties.memoryTypes[i];
  GSK_VK_CHECK (vkAllocateMemory, gdk_vulkan_context_get_device (context),
                                  &(VkMemoryAllocateInfo) {
                                      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      .allocationSize = size,
                                      .memoryTypeIndex = i
                                  },
                                  NULL,
                                  &self->vk_memory);

  return self;
}

void
gsk_vulkan_memory_free (GskVulkanMemory *self)
{
  vkFreeMemory (gdk_vulkan_context_get_device (self->vulkan),
                self->vk_memory,
                NULL);

  g_object_unref (self->vulkan);

  g_free (self);
}

VkDeviceMemory
gsk_vulkan_memory_get_device_memory (GskVulkanMemory *self)
{
  return self->vk_memory;
}

gboolean
gsk_vulkan_memory_can_map (GskVulkanMemory *self,
                           gboolean         fast)
{
  if (!(self->vk_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
    return FALSE;

  /* FIXME: no support implemented for this */
  if (!(self->vk_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    return FALSE;

  if (!fast)
    return TRUE;

  if (!(self->vk_memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
    return FALSE;

  return TRUE;
}

guchar *
gsk_vulkan_memory_map (GskVulkanMemory *self)
{
  void *data;

  g_assert (gsk_vulkan_memory_can_map (self, FALSE));

  GSK_VK_CHECK (vkMapMemory, gdk_vulkan_context_get_device (self->vulkan),
                             self->vk_memory,
                             0,
                             self->size,
                             0,
                             &data);

  return data;
}

void
gsk_vulkan_memory_unmap (GskVulkanMemory *self)
{
  vkUnmapMemory (gdk_vulkan_context_get_device (self->vulkan),
                 self->vk_memory);
}

