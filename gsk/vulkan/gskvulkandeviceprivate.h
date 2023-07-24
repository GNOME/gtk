#pragma once

#include "gskvulkanmemoryprivate.h"

#include <gdk/gdkvulkancontext.h>

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_DEVICE (gsk_vulkan_device_get_type ())

G_DECLARE_FINAL_TYPE(GskVulkanDevice, gsk_vulkan_device, GSK, VULKAN_DEVICE, GObject)

GskVulkanDevice *       gsk_vulkan_device_get_for_display               (GdkDisplay             *display,
                                                                         GError                **error);

VkDevice                gsk_vulkan_device_get_vk_device                 (GskVulkanDevice        *self);
VkPhysicalDevice        gsk_vulkan_device_get_vk_physical_device        (GskVulkanDevice        *self);

GskVulkanAllocator *    gsk_vulkan_device_find_allocator                (GskVulkanDevice        *self,
                                                                         uint32_t                allowed_types,
                                                                         VkMemoryPropertyFlags   required_flags,
                                                                         VkMemoryPropertyFlags   desired_flags);

G_END_DECLS
