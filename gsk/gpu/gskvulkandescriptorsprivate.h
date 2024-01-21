#pragma once

#include "gskgpudescriptorsprivate.h"

#include "gskvulkandeviceprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanDescriptors GskVulkanDescriptors;
typedef struct _GskVulkanDescriptorsClass GskVulkanDescriptorsClass;

#define GSK_VULKAN_DESCRIPTORS(d) ((GskVulkanDescriptors *) (d))
#define GSK_VULKAN_DESCRIPTORS_CLASS(d) ((GskVulkanDescriptorsClass *) (d))

struct _GskVulkanDescriptors
{
  GskGpuDescriptors parent_instance;
};

struct _GskVulkanDescriptorsClass
{
  GskGpuDescriptorsClass parent_class;

  GskVulkanPipelineLayout *     (* get_pipeline_layout)                         (GskVulkanDescriptors   *self);
  void                          (* bind)                                        (GskVulkanDescriptors   *self,
                                                                                 GskVulkanDescriptors   *previous,
                                                                                 VkCommandBuffer         vk_command_buffer);
};

GskVulkanPipelineLayout *       gsk_vulkan_descriptors_get_pipeline_layout      (GskVulkanDescriptors   *self);

void                            gsk_vulkan_descriptors_transition               (GskVulkanDescriptors   *self,
                                                                                 GskVulkanSemaphores    *semaphores,
                                                                                 VkCommandBuffer         vk_command_buffer);
void                            gsk_vulkan_descriptors_bind                     (GskVulkanDescriptors   *self,
                                                                                 GskVulkanDescriptors   *previous,
                                                                                 VkCommandBuffer         vk_command_buffer);

GskVulkanDescriptors *         gsk_vulkan_descriptors_new                       (GskVulkanDescriptorsClass *desc_class,
                                                                                 gsize                      child_size);


G_END_DECLS

