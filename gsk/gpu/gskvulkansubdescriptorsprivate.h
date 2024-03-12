#pragma once

#include "gskvulkandescriptorsprivate.h"

G_BEGIN_DECLS

typedef struct _GskVulkanSubDescriptors GskVulkanSubDescriptors;

#define GSK_VULKAN_SUB_DESCRIPTORS(d) ((GskVulkanSubDescriptors *) (d))

struct _GskVulkanSubDescriptors
{
  GskVulkanDescriptors parent_instance;

  GskVulkanDescriptors *parent;
};

GskVulkanSubDescriptors *       gsk_vulkan_sub_descriptors_new          (GskVulkanDescriptors           *parent);

G_END_DECLS

