#pragma once

#include "gskgpuimageprivate.h"
#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

/* required postprocessing steps before the image van be used */
typedef enum
{
  GSK_VULKAN_IMAGE_PREMULTIPLY = (1 << 0),
} GskVulkanImagePostprocess;

#define GSK_TYPE_VULKAN_IMAGE (gsk_vulkan_image_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanImage, gsk_vulkan_image, GSK, VULKAN_IMAGE, GskGpuImage)

GskGpuImage *           gsk_vulkan_image_new_for_swapchain              (GskVulkanDevice        *device,
                                                                         VkImage                 image,
                                                                         VkFormat                format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskGpuImage *           gsk_vulkan_image_new_for_atlas                  (GskVulkanDevice        *device,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_vulkan_image_new_for_offscreen              (GskVulkanDevice        *device,
                                                                         GdkMemoryFormat         preferred_format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskGpuImage *           gsk_vulkan_image_new_for_upload                 (GskVulkanDevice        *device,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height);
guchar *                gsk_vulkan_image_get_data                       (GskVulkanImage         *self,
                                                                         gsize                  *out_stride);

GskVulkanImagePostprocess
                        gsk_vulkan_image_get_postprocess                (GskVulkanImage         *self);
VkSampler               gsk_vulkan_image_get_vk_sampler                 (GskVulkanImage         *self);
VkPipelineStageFlags    gsk_vulkan_image_get_vk_pipeline_stage          (GskVulkanImage         *self);
VkImageLayout           gsk_vulkan_image_get_vk_image_layout            (GskVulkanImage         *self);
VkAccessFlags           gsk_vulkan_image_get_vk_access                  (GskVulkanImage         *self);
void                    gsk_vulkan_image_set_vk_image_layout            (GskVulkanImage         *self,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
void                    gsk_vulkan_image_transition                     (GskVulkanImage         *self,
                                                                         VkCommandBuffer         command_buffer,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
#define gdk_vulkan_image_transition_shader(image) \
  gsk_vulkan_image_transition ((image), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, \
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT)

VkImage                 gsk_vulkan_image_get_vk_image                   (GskVulkanImage         *self);
VkImageView             gsk_vulkan_image_get_vk_image_view              (GskVulkanImage         *self);
VkFormat                gsk_vulkan_image_get_vk_format                  (GskVulkanImage         *self);
VkFramebuffer           gsk_vulkan_image_get_vk_framebuffer             (GskVulkanImage         *self,
                                                                         VkRenderPass            pass);

G_END_DECLS

