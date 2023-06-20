#pragma once

#include <gdk/gdk.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkancommandpoolprivate.h"


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
GskVulkanImage *        gsk_vulkan_image_new_from_texture               (GskVulkanUploader      *uploader,
                                                                         GdkTexture             *texture);

typedef struct {
  guchar *data;
  gsize width;
  gsize height;
  gsize stride;
  gsize x;
  gsize y;
} GskImageRegion;

void                    gsk_vulkan_image_upload_regions                 (GskVulkanImage         *image,
                                                                         GskVulkanUploader      *uploader,
                                                                         guint                   num_regions,
                                                                         GskImageRegion         *regions);
GskVulkanImage *        gsk_vulkan_image_new_for_atlas                  (GdkVulkanContext       *context,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskVulkanImage *        gsk_vulkan_image_new_for_offscreen              (GdkVulkanContext       *context,
                                                                         GdkMemoryFormat         preferred_format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GdkTexture *            gsk_vulkan_image_download                       (GskVulkanImage         *self,
                                                                         GskVulkanUploader      *uploader);

typedef struct _GskVulkanImageMap GskVulkanImageMap;

struct _GskVulkanImageMap
{
  guchar *data;
  gsize stride;
  GskVulkanMapMode mode;

  /* private */
  GskVulkanBuffer *staging_buffer;
};

GskVulkanImage *        gsk_vulkan_image_new_for_upload                 (GskVulkanUploader      *uploader,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
void                    gsk_vulkan_image_map_memory                     (GskVulkanImage         *self,
                                                                         GskVulkanUploader      *uploader,
                                                                         GskVulkanMapMode        mode,
                                                                         GskVulkanImageMap      *map);
void                    gsk_vulkan_image_unmap_memory                   (GskVulkanImage         *self,
                                                                         GskVulkanUploader      *uploader,
                                                                         GskVulkanImageMap      *map);

gsize                   gsk_vulkan_image_get_width                      (GskVulkanImage         *self);
gsize                   gsk_vulkan_image_get_height                     (GskVulkanImage         *self);
VkImage                 gsk_vulkan_image_get_image                      (GskVulkanImage         *self);
VkImageView             gsk_vulkan_image_get_image_view                 (GskVulkanImage         *self);
VkFormat                gsk_vulkan_image_get_vk_format                  (GskVulkanImage         *self);

G_END_DECLS

