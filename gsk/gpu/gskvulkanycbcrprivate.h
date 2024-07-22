#pragma once

#include "gskvulkandeviceprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

G_BEGIN_DECLS

GskVulkanYcbcr *        gsk_vulkan_ycbcr_new                            (GskVulkanDevice        *self,
                                                                         VkFormat                vk_format);
                                                                         
GskVulkanYcbcr *        gsk_vulkan_ycbcr_ref                            (GskVulkanYcbcr         *self);
void                    gsk_vulkan_ycbcr_unref                          (GskVulkanYcbcr         *self);

VkSamplerYcbcrConversion
                        gsk_vulkan_ycbcr_get_vk_conversion              (GskVulkanYcbcr         *self);
VkSampler               gsk_vulkan_ycbcr_get_vk_sampler                 (GskVulkanYcbcr         *self);
VkDescriptorSetLayout   gsk_vulkan_ycbcr_get_vk_descriptor_set_layout   (GskVulkanYcbcr         *self);
VkPipelineLayout        gsk_vulkan_ycbcr_get_vk_pipeline_layout         (GskVulkanYcbcr         *self,
                                                                         gsize                   id);


G_END_DECLS
