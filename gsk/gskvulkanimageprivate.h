#ifndef __GSK_VULKAN_IMAGE_PRIVATE_H__
#define __GSK_VULKAN_IMAGE_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GskVulkanImage GskVulkanImage;

GskVulkanImage *        gsk_vulkan_image_new_from_data                  (GdkVulkanContext       *context,
                                                                         VkCommandBuffer         command_buffer,
                                                                         guchar                 *data,
                                                                         gsize                   width,
                                                                         gsize                   height,
                                                                         gsize                   stride);
void                    gsk_vulkan_image_free                           (GskVulkanImage         *image);

VkImage                 gsk_vulkan_image_get_image                      (GskVulkanImage         *self);
VkImageView             gsk_vulkan_image_get_image_view                 (GskVulkanImage         *self);

G_END_DECLS

#endif /* __GSK_VULKAN_IMAGE_PRIVATE_H__ */
