#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanCommandPool GskVulkanCommandPool;

GskVulkanCommandPool *  gsk_vulkan_command_pool_new                     (GdkVulkanContext       *context);
void                    gsk_vulkan_command_pool_free                    (GskVulkanCommandPool   *self);

void                    gsk_vulkan_command_pool_reset                   (GskVulkanCommandPool   *self);

VkCommandBuffer         gsk_vulkan_command_pool_get_buffer              (GskVulkanCommandPool   *self);
void                    gsk_vulkan_command_pool_submit_buffer           (GskVulkanCommandPool   *self,
                                                                         VkCommandBuffer         buffer,
                                                                         gsize                   wait_semaphore_count,
                                                                         VkSemaphore            *wait_semaphores,
                                                                         gsize                   signal_semaphores_count,
                                                                         VkSemaphore            *signal_semaphores,
                                                                         VkFence                 fence);

G_END_DECLS

