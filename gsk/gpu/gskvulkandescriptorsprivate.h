#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskvulkanframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_DESCRIPTORS (gsk_vulkan_descriptors_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanDescriptors, gsk_vulkan_descriptors, GSK, VULKAN_DESCRIPTORS, GskGpuDescriptors)

GskVulkanDescriptors *          gsk_vulkan_descriptors_new              (GskVulkanDevice                *device);

gboolean                        gsk_vulkan_descriptors_is_full          (GskVulkanDescriptors           *self);
GskVulkanPipelineLayout *       gsk_vulkan_descriptors_get_pipeline_layout
                                                                        (GskVulkanDescriptors           *self);
VkPipelineLayout                gsk_vulkan_descriptors_get_vk_pipeline_layout
                                                                        (GskVulkanDescriptors           *self);
guint32                         gsk_vulkan_descriptors_get_buffer_descriptor
                                                                        (GskVulkanDescriptors           *self,
                                                                         GskGpuBuffer                   *buffer);

void                            gsk_vulkan_descriptors_transition       (GskVulkanDescriptors           *self,
                                                                         VkCommandBuffer                 command_buffer);
void                            gsk_vulkan_descriptors_prepare          (GskVulkanDescriptors           *self,
                                                                         gsize                          *n_images,
                                                                         gsize                          *n_buffers);
void                            gsk_vulkan_descriptors_update_sets      (GskVulkanDescriptors           *self,
                                                                         VkDescriptorPool                vk_descriptor_pool);
void                            gsk_vulkan_descriptors_bind             (GskVulkanDescriptors           *self,
                                                                         VkCommandBuffer                 vk_command_buffer);

G_END_DECLS

