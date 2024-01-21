#include "config.h"

#include "gskvulkansubdescriptorsprivate.h"


static void     gsk_vulkan_sub_descriptors_finalize   (GskGpuDescriptors *desc);
static gboolean gsk_vulkan_sub_descriptors_add_buffer (GskGpuDescriptors *desc,
                                                       GskGpuBuffer      *buffer,
                                                       guint32           *out_descriptor);
static gboolean gsk_vulkan_sub_descriptors_add_image  (GskGpuDescriptors *desc,
                                                       GskGpuImage       *image,
                                                       GskGpuSampler      sampler,
                                                       guint32           *out_descriptor);
static GskVulkanPipelineLayout *
                gsk_vulkan_sub_descriptors_get_pipeline_layout (GskVulkanDescriptors *desc);
static void     gsk_vulkan_sub_descriptors_bind       (GskVulkanDescriptors *desc,
                                                       GskVulkanDescriptors *previous,
                                                       VkCommandBuffer       vk_command_buffer);

static GskVulkanDescriptorsClass GSK_VULKAN_SUB_DESCRIPTORS_CLASS =
{
  .parent_class = (GskGpuDescriptorsClass) {
    .finalize = gsk_vulkan_sub_descriptors_finalize,
    .add_image = gsk_vulkan_sub_descriptors_add_image,
    .add_buffer = gsk_vulkan_sub_descriptors_add_buffer,
  },
  .get_pipeline_layout = gsk_vulkan_sub_descriptors_get_pipeline_layout,
  .bind = gsk_vulkan_sub_descriptors_bind,
};

static GskVulkanPipelineLayout *
gsk_vulkan_sub_descriptors_get_pipeline_layout (GskVulkanDescriptors *desc)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_vulkan_descriptors_get_pipeline_layout (self->parent);
}

static void
gsk_vulkan_sub_descriptors_bind (GskVulkanDescriptors *desc,
                                 GskVulkanDescriptors *previous,
                                 VkCommandBuffer       vk_command_buffer)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  if (GSK_GPU_DESCRIPTORS (previous)->desc_class == (GskGpuDescriptorsClass *) &GSK_VULKAN_SUB_DESCRIPTORS_CLASS)
    previous = GSK_VULKAN_SUB_DESCRIPTORS (previous)->parent;

  if (self->parent == previous)
    return;

  gsk_vulkan_descriptors_bind (self->parent, previous, vk_command_buffer);
}

static gboolean
gsk_vulkan_sub_descriptors_add_image (GskGpuDescriptors *desc,
                                      GskGpuImage       *image,
                                      GskGpuSampler      sampler,
                                      guint32           *out_descriptor)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_gpu_descriptors_add_image (GSK_GPU_DESCRIPTORS (self->parent),
                                        image,
                                        sampler,
                                        out_descriptor);
}

static gboolean
gsk_vulkan_sub_descriptors_add_buffer (GskGpuDescriptors *desc,
                                       GskGpuBuffer      *buffer,
                                       guint32           *out_descriptor)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  return gsk_gpu_descriptors_add_buffer (GSK_GPU_DESCRIPTORS (self->parent),
                                         buffer,
                                         out_descriptor);
}

static void
gsk_vulkan_sub_descriptors_finalize (GskGpuDescriptors *desc)
{
  GskVulkanSubDescriptors *self = GSK_VULKAN_SUB_DESCRIPTORS (desc);

  gsk_gpu_descriptors_unref (GSK_GPU_DESCRIPTORS (self->parent));

  gsk_vulkan_descriptors_finalize (GSK_VULKAN_DESCRIPTORS (self));
}

static void
gsk_vulkan_sub_descriptors_init (GskVulkanSubDescriptors *self)
{
  gsk_vulkan_descriptors_init (GSK_VULKAN_DESCRIPTORS (self));
}

GskVulkanSubDescriptors *
gsk_vulkan_sub_descriptors_new (GskVulkanDescriptors *parent)
{
  GskVulkanSubDescriptors *self;
  GskGpuDescriptors *desc;

  self = g_new0 (GskVulkanSubDescriptors, 1);
  desc = GSK_GPU_DESCRIPTORS (self);

  desc->ref_count = 1;
  desc->desc_class = (GskGpuDescriptorsClass *) &GSK_VULKAN_SUB_DESCRIPTORS_CLASS;
  gsk_vulkan_sub_descriptors_init (self);

  self->parent = GSK_VULKAN_DESCRIPTORS (gsk_gpu_descriptors_ref (GSK_GPU_DESCRIPTORS (parent)));

  return self;
}
