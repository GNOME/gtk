#pragma once

#include "gskvulkandescriptorsprivate.h"
#include "gskvulkanframeprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"


G_BEGIN_DECLS

typedef struct _GskVulkanRealDescriptors GskVulkanRealDescriptors;

#define GSK_VULKAN_REAL_DESCRIPTORS(d) ((GskVulkanRealDescriptors *) (d))


GskVulkanRealDescriptors *      gsk_vulkan_real_descriptors_new                 (GskVulkanFrame                 *frame);

gboolean                        gsk_vulkan_real_descriptors_is_full             (GskVulkanRealDescriptors       *self);

void                            gsk_vulkan_real_descriptors_prepare             (GskVulkanRealDescriptors       *self,
                                                                                 gsize                          *n_images,
                                                                                 gsize                          *n_buffers);
void                            gsk_vulkan_real_descriptors_update_sets         (GskVulkanRealDescriptors       *self,
                                                                                 VkDescriptorPool                vk_descriptor_pool);

G_END_DECLS

