#pragma once

#include <gdk/gdk.h>

#include "gskvulkanbufferprivate.h"
#include "gskvulkancommandpoolprivate.h"


G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_IMAGE (gsk_vulkan_image_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanImage, gsk_vulkan_image, GSK, VULKAN_IMAGE, GObject)

GskVulkanImage *        gsk_vulkan_image_new_for_swapchain              (GdkVulkanContext       *context,
                                                                         VkImage                 image,
                                                                         VkFormat                format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskVulkanImage *        gsk_vulkan_image_new_for_atlas                  (GdkVulkanContext       *context,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskVulkanImage *        gsk_vulkan_image_new_for_offscreen              (GdkVulkanContext       *context,
                                                                         GdkMemoryFormat         preferred_format,
                                                                         gsize                   width,
                                                                         gsize                   height);

typedef struct _GskVulkanImageMap GskVulkanImageMap;

struct _GskVulkanImageMap
{
  guchar *data;
  gsize stride;
  GskVulkanMapMode mode;

  /* private */
  GskVulkanBuffer *staging_buffer;
};

GskVulkanImage *        gsk_vulkan_image_new_for_upload                 (GdkVulkanContext       *context,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
guchar *                gsk_vulkan_image_try_map                        (GskVulkanImage         *self,
                                                                         gsize                  *out_stride);
void                    gsk_vulkan_image_unmap                          (GskVulkanImage         *self);

gsize                   gsk_vulkan_image_get_width                      (GskVulkanImage         *self);
gsize                   gsk_vulkan_image_get_height                     (GskVulkanImage         *self);
VkPipelineStageFlags    gsk_vulkan_image_get_vk_pipeline_stage          (GskVulkanImage         *self);
VkImageLayout           gsk_vulkan_image_get_vk_image_layout            (GskVulkanImage         *self);
VkAccessFlags           gsk_vulkan_image_get_vk_access                  (GskVulkanImage         *self);
void                    gsk_vulkan_image_set_vk_image_layout            (GskVulkanImage         *self,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
VkImage                 gsk_vulkan_image_get_vk_image                   (GskVulkanImage         *self);
VkImageView             gsk_vulkan_image_get_image_view                 (GskVulkanImage         *self);
VkFormat                gsk_vulkan_image_get_vk_format                  (GskVulkanImage         *self);
GdkMemoryFormat         gsk_vulkan_image_get_format                     (GskVulkanImage         *self);
VkFramebuffer           gsk_vulkan_image_get_framebuffer                (GskVulkanImage         *self,
                                                                         VkRenderPass            pass);

static inline void
print_image (GString        *string,
             GskVulkanImage *image)
{
  g_string_append_printf (string, "%zux%zu ",
                          gsk_vulkan_image_get_width (image),
                          gsk_vulkan_image_get_height (image));
}

G_END_DECLS

