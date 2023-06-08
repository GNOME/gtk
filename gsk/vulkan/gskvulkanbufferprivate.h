#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanBuffer GskVulkanBuffer;

GskVulkanBuffer *       gsk_vulkan_buffer_new                           (GdkVulkanContext       *context,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_storage                   (GdkVulkanContext       *context,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_staging                   (GdkVulkanContext       *context,
                                                                         gsize                   size);
GskVulkanBuffer *       gsk_vulkan_buffer_new_download                  (GdkVulkanContext       *context,
                                                                         gsize                   size);
void                    gsk_vulkan_buffer_free                          (GskVulkanBuffer        *buffer);

VkBuffer                gsk_vulkan_buffer_get_buffer                    (GskVulkanBuffer        *self);

guchar *                gsk_vulkan_buffer_map                           (GskVulkanBuffer        *self);
void                    gsk_vulkan_buffer_unmap                         (GskVulkanBuffer        *self);

G_END_DECLS

