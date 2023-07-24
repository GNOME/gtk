#pragma once

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanBuffer GskVulkanBuffer;

typedef enum
{
  GSK_VULKAN_READ = (1 << 0),
  GSK_VULKAN_WRITE = (1 << 1),
  GSK_VULKAN_READWRITE = GSK_VULKAN_READ | GSK_VULKAN_WRITE
} GskVulkanMapMode;

GskVulkanBuffer *       gsk_vulkan_buffer_new                           (GskVulkanDevice        *device,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_storage                   (GskVulkanDevice        *device,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_map                       (GskVulkanDevice        *device,
                                                                         gsize                   size,
                                                                         GskVulkanMapMode        mode);
void                    gsk_vulkan_buffer_free                          (GskVulkanBuffer        *buffer);

VkBuffer                gsk_vulkan_buffer_get_buffer                    (GskVulkanBuffer        *self);
gsize                   gsk_vulkan_buffer_get_size                      (GskVulkanBuffer        *self);
guchar *                gsk_vulkan_buffer_get_data                      (GskVulkanBuffer        *self);

G_END_DECLS

