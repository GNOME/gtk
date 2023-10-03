#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskvulkanframeprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_DESCRIPTORS (gsk_vulkan_descriptors_get_type ())

G_DECLARE_FINAL_TYPE (GskVulkanDescriptors, gsk_vulkan_descriptors, GSK, VULKAN_DESCRIPTORS, GskGpuDescriptors)

GskGpuDescriptors *             gsk_vulkan_descriptors_new              (GskVulkanFrame                 *frame);

void                            gsk_vulkan_descriptors_transition       (GskVulkanDescriptors           *self,
                                                                         VkCommandBuffer                 command_buffer);

G_END_DECLS

