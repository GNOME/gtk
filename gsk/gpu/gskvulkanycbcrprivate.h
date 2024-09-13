#pragma once

#include "gskvulkandeviceprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanYcbcrInfo GskVulkanYcbcrInfo;

struct _GskVulkanYcbcrInfo
{
  VkFormat vk_format;
  VkComponentMapping vk_components;
  VkSamplerYcbcrModelConversion vk_ycbcr_model;
  VkSamplerYcbcrRange vk_ycbcr_range;
};


void                            gsk_vulkan_ycbcr_init_cache                     (GskGpuCache                    *cache);
void                            gsk_vulkan_ycbcr_finish_cache                   (GskGpuCache                    *cache);

GskVulkanYcbcr *                gsk_vulkan_ycbcr_get                            (GskVulkanDevice                *self,
                                                                                 const GskVulkanYcbcrInfo       *info);
                                                                         
GskVulkanYcbcr *                gsk_vulkan_ycbcr_ref                            (GskVulkanYcbcr                 *self);
void                            gsk_vulkan_ycbcr_unref                          (GskVulkanYcbcr                 *self);

VkSamplerYcbcrConversion        gsk_vulkan_ycbcr_get_vk_conversion              (GskVulkanYcbcr                 *self);
VkSampler                       gsk_vulkan_ycbcr_get_vk_sampler                 (GskVulkanYcbcr                 *self);
VkDescriptorSetLayout           gsk_vulkan_ycbcr_get_vk_descriptor_set_layout   (GskVulkanYcbcr                 *self);
VkPipelineLayout                gsk_vulkan_ycbcr_get_vk_pipeline_layout         (GskVulkanYcbcr                 *self,
                                                                                 gsize                           id);


G_END_DECLS
