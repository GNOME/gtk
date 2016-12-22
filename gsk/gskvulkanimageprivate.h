#ifndef __GSK_VULKAN_IMAGE_PRIVATE_H__
#define __GSK_VULKAN_IMAGE_PRIVATE_H__

#include <gdk/gdk.h>

#include "gsk/gsktexture.h"
#include "gsk/gskvulkancommandpoolprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanUploader GskVulkanUploader;

#define GSK_TYPE_VULKAN_IMAGE (gsk_vulkan_image_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanImage, gsk_vulkan_image, GSK, VULKAN_IMAGE, GObject)

GskVulkanUploader *     gsk_vulkan_uploader_new                         (GdkVulkanContext       *context,
                                                                         GskVulkanCommandPool   *command_pool);
void                    gsk_vulkan_uploader_free                        (GskVulkanUploader      *self);

void                    gsk_vulkan_uploader_reset                       (GskVulkanUploader      *self);
void                    gsk_vulkan_uploader_upload                      (GskVulkanUploader      *self);

GskVulkanImage *        gsk_vulkan_image_new_for_swapchain              (GdkVulkanContext       *context,
                                                                         VkImage                 image,
                                                                         VkFormat                format,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskVulkanImage *        gsk_vulkan_image_new_from_data                  (GskVulkanUploader      *uploader,
                                                                         guchar                 *data,
                                                                         gsize                   width,
                                                                         gsize                   height,
                                                                         gsize                   stride);
GskVulkanImage *        gsk_vulkan_image_new_for_framebuffer            (GdkVulkanContext       *context,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskTexture *            gsk_vulkan_image_download                       (GskVulkanImage         *self,
                                                                         GskVulkanUploader      *uploader);

gsize                   gsk_vulkan_image_get_width                      (GskVulkanImage         *self);
gsize                   gsk_vulkan_image_get_height                     (GskVulkanImage         *self);
VkImage                 gsk_vulkan_image_get_image                      (GskVulkanImage         *self);
VkImageView             gsk_vulkan_image_get_image_view                 (GskVulkanImage         *self);

G_END_DECLS

#endif /* __GSK_VULKAN_IMAGE_PRIVATE_H__ */
