#ifndef __GSK_VULKAN_RENDER_PRIVATE_H__
#define __GSK_VULKAN_RENDER_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanRender GskVulkanRender;

struct _GskVulkanRender
{
  GdkVulkanContext *vulkan;

  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
};

void                    gsk_vulkan_render_init                          (GskVulkanRender        *self,
                                                                         GdkVulkanContext       *context,
                                                                         VkCommandPool           command_pool);

void                    gsk_vulkan_render_submit                        (GskVulkanRender        *self,
                                                                         VkFence                 fence);

void                    gsk_vulkan_render_finish                        (GskVulkanRender        *self);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDER_PRIVATE_H__ */
