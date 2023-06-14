#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanMemory GskVulkanMemory;

GskVulkanMemory *       gsk_vulkan_memory_new                           (GdkVulkanContext       *context,
                                                                         uint32_t                allowed_types,
                                                                         VkMemoryPropertyFlags   properties,
                                                                         gsize                   size);
void                    gsk_vulkan_memory_free                          (GskVulkanMemory        *memory);

VkDeviceMemory          gsk_vulkan_memory_get_device_memory             (GskVulkanMemory        *self);

gboolean                gsk_vulkan_memory_can_map                       (GskVulkanMemory        *self,
                                                                         gboolean                fast);
guchar *                gsk_vulkan_memory_map                           (GskVulkanMemory        *self);
void                    gsk_vulkan_memory_unmap                         (GskVulkanMemory        *self);

G_END_DECLS

