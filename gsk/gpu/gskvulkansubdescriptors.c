#include "config.h"

#include "gskvulkansubdescriptorsprivate.h"


struct _GskVulkanSubDescriptors
{
  GskVulkanDescriptors parent_instance;

  GskVulkanDescriptors *parent;
};


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
}

GskVulkanSubDescriptors *
gsk_vulkan_sub_descriptors_new (GskVulkanDescriptors *parent)
{
  GskVulkanSubDescriptors *self;
  GskVulkanDescriptors *desc;

  desc = gsk_vulkan_descriptors_new (&GSK_VULKAN_SUB_DESCRIPTORS_CLASS,
                                     sizeof (GskVulkanSubDescriptors));

  self = GSK_VULKAN_SUB_DESCRIPTORS (desc);
  self->parent = GSK_VULKAN_DESCRIPTORS (gsk_gpu_descriptors_ref (GSK_GPU_DESCRIPTORS (parent)));

  return self;
}
