#pragma once

#include "gskgpuframeprivate.h"

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_FRAME (gsk_vulkan_frame_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK, VULKAN_FRAME, GskGpuFrame)

VkFence                 gsk_vulkan_frame_get_vk_fence                   (GskVulkanFrame         *self) G_GNUC_PURE;
GskVulkanPipelineLayout *
                        gsk_vulkan_frame_get_pipeline_layout            (GskVulkanFrame         *self);

guint32                 gsk_vulkan_frame_add_image                      (GskVulkanFrame         *self,
                                                                         GskGpuImage            *image,
                                                                         GskGpuSampler           sampler);

G_END_DECLS
