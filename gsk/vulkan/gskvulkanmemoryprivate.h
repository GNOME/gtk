#ifndef __GSK_VULKAN_MEMORY_PRIVATE_H__
#define __GSK_VULKAN_MEMORY_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanMemory GskVulkanMemory;

GskVulkanMemory *       gsk_vulkan_memory_new                           (GdkVulkanContext       *context,
                                                                         uint32_t                allowed_types,
                                                                         VkMemoryPropertyFlags   properties,
                                                                         gsize                   size);
void                    gsk_vulkan_memory_free                          (GskVulkanMemory        *memory);

VkDeviceMemory          gsk_vulkan_memory_get_device_memory             (GskVulkanMemory        *self);

guchar *                gsk_vulkan_memory_map                           (GskVulkanMemory        *self);
void                    gsk_vulkan_memory_unmap                         (GskVulkanMemory        *self);

G_END_DECLS

#endif /* __GSK_VULKAN_MEMORY_PRIVATE_H__ */
