#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanBuffer GskVulkanBuffer;

typedef enum
{
  GSK_VULKAN_READ = (1 << 0),
  GSK_VULKAN_WRITE = (1 << 1),
  GSK_VULKAN_READWRITE = GSK_VULKAN_READ | GSK_VULKAN_WRITE
} GskVulkanMapMode;

GskVulkanBuffer *       gsk_vulkan_buffer_new                           (GdkVulkanContext       *context,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_storage                   (GdkVulkanContext       *context,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_map                       (GdkVulkanContext       *context,
                                                                         gsize                   size,
                                                                         GskVulkanMapMode        mode);
void                    gsk_vulkan_buffer_free                          (GskVulkanBuffer        *buffer);

VkBuffer                gsk_vulkan_buffer_get_buffer                    (GskVulkanBuffer        *self);
gsize                   gsk_vulkan_buffer_get_size                      (GskVulkanBuffer        *self);

guchar *                gsk_vulkan_buffer_map                           (GskVulkanBuffer        *self);
void                    gsk_vulkan_buffer_unmap                         (GskVulkanBuffer        *self);

G_END_DECLS

