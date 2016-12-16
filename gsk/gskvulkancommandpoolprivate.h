#ifndef __GSK_VULKAN_COMMAND_POOL_PRIVATE_H__
#define __GSK_VULKAN_COMMAND_POOL_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanCommandPool GskVulkanCommandPool;

GskVulkanCommandPool *  gsk_vulkan_command_pool_new                     (GdkVulkanContext       *context);
void                    gsk_vulkan_command_pool_free                    (GskVulkanCommandPool   *self);

void                    gsk_vulkan_command_pool_reset                   (GskVulkanCommandPool   *self);

VkCommandBuffer         gsk_vulkan_command_pool_get_buffer              (GskVulkanCommandPool   *self);
void                    gsk_vulkan_command_pool_submit_buffer           (GskVulkanCommandPool   *self,
                                                                         VkCommandBuffer         buffer,
                                                                         VkFence                 fence);

G_END_DECLS

#endif /* __GSK_VULKAN_COMMAND_POOL_PRIVATE_H__ */
