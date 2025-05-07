#pragma once

#include "gskgpuimageprivate.h"
#include "gskvulkandeviceprivate.h"

#include "gdk/gdkdmabufprivate.h"

#ifdef GDK_WINDOWING_WIN32
#include <gdk/win32/gdkwin32.h>
#endif

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_IMAGE (gsk_vulkan_image_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanImage, gsk_vulkan_image, GSK, VULKAN_IMAGE, GskGpuImage)

GskGpuImage *           gsk_vulkan_image_new_for_swapchain              (GskVulkanDevice        *device,
                                                                         VkImage                 image,
                                                                         VkFormat                format,
                                                                         GdkMemoryFormat         memory_format,
                                                                         gsize                   width,
                                                                         gsize                   height);

GskGpuImage *           gsk_vulkan_image_new_for_atlas                  (GskVulkanDevice        *device,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_vulkan_image_new_for_offscreen              (GskVulkanDevice        *device,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         preferred_format,
                                                                         gboolean                try_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_vulkan_image_new_for_upload                 (GskVulkanDevice        *device,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         GskGpuConversion        conv,
                                                                         gsize                   width,
                                                                         gsize                   height);
#ifdef HAVE_DMABUF
GskGpuImage *           gsk_vulkan_image_new_dmabuf                     (GskVulkanDevice        *device,
                                                                         GdkMemoryFormat         format,
                                                                         gboolean                try_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_vulkan_image_new_for_dmabuf                 (GskVulkanDevice        *device,
                                                                         gsize                   width,
                                                                         gsize                   height,
                                                                         const GdkDmabuf        *dmabuf,
                                                                         gboolean                premultiplied,
                                                                         GskGpuConversion        conv);
GdkTexture *            gsk_vulkan_image_to_dmabuf_texture              (GskVulkanImage         *self,
                                                                         GdkColorState          *color_state);
#endif
#ifdef GDK_WINDOWING_WIN32
GskGpuImage *           gsk_vulkan_image_new_for_d3d12resource          (GskVulkanDevice        *device,
                                                                         ID3D12Resource         *resource,
                                                                         HANDLE                  resource_handle,
                                                                         ID3D12Fence            *fence,
                                                                         HANDLE                  fence_handle,
                                                                         guint64                 fence_wait,
                                                                         gboolean                premultiplied);
#endif

guchar *                gsk_vulkan_image_get_data                       (GskVulkanImage         *self,
                                                                         GdkMemoryLayout        *out_layout);

GskVulkanYcbcr *        gsk_vulkan_image_get_ycbcr                      (GskVulkanImage         *self);
VkDescriptorSet         gsk_vulkan_image_get_vk_descriptor_set          (GskVulkanImage         *self,
                                                                         GskGpuSampler           sampler);
VkPipelineStageFlags    gsk_vulkan_image_get_vk_pipeline_stage          (GskVulkanImage         *self);
VkImageLayout           gsk_vulkan_image_get_vk_image_layout            (GskVulkanImage         *self);
VkAccessFlags           gsk_vulkan_image_get_vk_access                  (GskVulkanImage         *self);
void                    gsk_vulkan_image_set_vk_image_layout            (GskVulkanImage         *self,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
void                    gsk_vulkan_image_transition                     (GskVulkanImage         *self,
                                                                         GskVulkanSemaphores    *semaphores,
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

