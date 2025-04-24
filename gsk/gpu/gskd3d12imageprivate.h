#pragma once

#include "gskgpuimageprivate.h"
#include "gskd3d12deviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_D3D12_IMAGE (gsk_d3d12_image_get_type ())

G_DECLARE_FINAL_TYPE (GskD3d12Image, gsk_d3d12_image, GSK, D3D12_IMAGE, GskGpuImage)

GskGpuImage *           gsk_d3d12_image_new                             (ID3D12Device           *device,
                                                                         GdkMemoryFormat         format,
                                                                         gsize                   width,
                                                                         gsize                   height,
                                                                         D3D12_RESOURCE_STATES   initial_state,
                                                                         D3D12_HEAP_FLAGS        heap_flags,
                                                                         D3D12_RESOURCE_FLAGS    resource_flags);
GskGpuImage *           gsk_d3d12_image_new_for_resource                (ID3D12Resource         *resource,
                                                                         gboolean                premultiplied);

ID3D12Resource *        gsk_d3d12_image_get_resource                    (GskD3d12Image          *self);

#if 0
GskGpuImage *           gsk_d3d12_image_new_for_atlas                   (GskD3d12Device         *device,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_d3d12_image_new_for_offscreen               (GskD3d12Device         *device,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         preferred_format,
                                                                         gboolean                try_srgb,
                                                                         gsize                   width,
                                                                         gsize                   height);
GskGpuImage *           gsk_d3d12_image_new_for_upload                  (GskD3d12Device         *device,
                                                                         gboolean                with_mipmap,
                                                                         GdkMemoryFormat         format,
                                                                         GskGpuConversion        conv,
                                                                         gsize                   width,
                                                                         gsize                   height);
#ifdef GDK_WINDOWING_WIN32
GskGpuImage *           gsk_d3d12_image_new_for_d3d12resource          (GskD3d12Device        *device,
                                                                         ID3D12Resource         *resource,
                                                                         HANDLE                  resource_handle,
                                                                         ID3D12Fence            *fence,
                                                                         HANDLE                  fence_handle,
                                                                         guint64                 fence_wait,
                                                                         gboolean                premultiplied);
#endif

VkDescriptorSet         gsk_d3d12_image_get_vk_descriptor_set          (GskD3d12Image         *self,
                                                                         GskGpuSampler           sampler);
VkPipelineStageFlags    gsk_d3d12_image_get_vk_pipeline_stage          (GskD3d12Image         *self);
VkImageLayout           gsk_d3d12_image_get_vk_image_layout            (GskD3d12Image         *self);
VkAccessFlags           gsk_d3d12_image_get_vk_access                  (GskD3d12Image         *self);
void                    gsk_d3d12_image_set_vk_image_layout            (GskD3d12Image         *self,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
void                    gsk_d3d12_image_transition                     (GskD3d12Image         *self,
                                                                         GskD3d12Semaphores    *semaphores,
                                                                         VkCommandBuffer         command_buffer,
                                                                         VkPipelineStageFlags    stage,
                                                                         VkImageLayout           image_layout,
                                                                         VkAccessFlags           access);
#define gdk_d3d12_image_transition_shader(image) \
  gsk_d3d12_image_transition ((image), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, \
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT)

VkImage                 gsk_d3d12_image_get_vk_image                    (GskD3d12Image         *self);
VkImageView             gsk_d3d12_image_get_vk_image_view               (GskD3d12Image         *self);
VkFormat                gsk_d3d12_image_get_vk_format                   (GskD3d12Image         *self);
VkFramebuffer           gsk_d3d12_image_get_vk_framebuffer              (GskD3d12Image         *self,
                                                                         VkRenderPass            pass);
#endif

G_END_DECLS
