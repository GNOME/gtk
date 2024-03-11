#pragma once

#include "gskvulkandescriptorsprivate.h"
#include "gskvulkanframeprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"

#define INCLUDE_DECL 1

#define GDK_ARRAY_NAME gsk_descriptor_image_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorImageInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorImageInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 128
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define INCLUDE_DECL 1

#define GDK_ARRAY_NAME gsk_descriptor_buffer_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorBufferInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorBufferInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define INCLUDE_DECL 1

#define GDK_ARRAY_NAME gsk_samplers
#define GDK_ARRAY_TYPE_NAME GskSamplers
#define GDK_ARRAY_ELEMENT_TYPE VkSampler
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"


G_BEGIN_DECLS

typedef struct _GskVulkanRealDescriptors GskVulkanRealDescriptors;

#define GSK_VULKAN_REAL_DESCRIPTORS(d) ((GskVulkanRealDescriptors *) (d))

struct _GskVulkanRealDescriptors
{
  GskVulkanDescriptors parent_instance;

  GskVulkanFrame *frame; /* no reference, the frame owns us */

  GskVulkanPipelineLayout *pipeline_layout;

  GskSamplers immutable_samplers;
  GskDescriptorImageInfos descriptor_immutable_images;
  GskDescriptorImageInfos descriptor_images;
  GskDescriptorBufferInfos descriptor_buffers;

  VkDescriptorSet descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS];
};

GskVulkanRealDescriptors *      gsk_vulkan_real_descriptors_new                 (GskVulkanFrame                 *frame);

gboolean                        gsk_vulkan_real_descriptors_is_full             (GskVulkanRealDescriptors       *self);

void                            gsk_vulkan_real_descriptors_prepare             (GskVulkanRealDescriptors       *self,
                                                                                 gsize                          *n_images,
                                                                                 gsize                          *n_buffers);
void                            gsk_vulkan_real_descriptors_update_sets         (GskVulkanRealDescriptors       *self,
                                                                                 VkDescriptorPool                vk_descriptor_pool);

G_END_DECLS

