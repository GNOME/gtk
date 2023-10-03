#include "config.h"

#include "gskvulkandescriptorsprivate.h"

#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"

struct _GskVulkanDescriptors
{
  GskGpuDescriptors parent_instance;

  GskVulkanFrame *frame; /* no reference, the frame owns us */
};

G_DEFINE_TYPE (GskVulkanDescriptors, gsk_vulkan_descriptors, GSK_TYPE_GPU_DESCRIPTORS)

static gboolean
gsk_vulkan_descriptors_add_image (GskGpuDescriptors *desc,
                                  GskGpuImage       *image,
                                  GskGpuSampler      sampler,
                                  guint32           *out_descriptor)
{
  GskVulkanDescriptors *self = GSK_VULKAN_DESCRIPTORS (desc);

  *out_descriptor = gsk_vulkan_frame_add_image (self->frame, image, sampler);
  return TRUE;
}

static void
gsk_vulkan_descriptors_class_init (GskVulkanDescriptorsClass *klass)
{
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);

  descriptors_class->add_image = gsk_vulkan_descriptors_add_image;
}

static void
gsk_vulkan_descriptors_init (GskVulkanDescriptors *self)
{
}

GskGpuDescriptors *
gsk_vulkan_descriptors_new (GskVulkanFrame *frame)
{
  GskVulkanDescriptors *self;

  self = g_object_new (GSK_TYPE_VULKAN_DESCRIPTORS, NULL);

  self->frame = frame;

  return GSK_GPU_DESCRIPTORS (self);
}

void
gsk_vulkan_descriptors_transition (GskVulkanDescriptors *self,
                                   VkCommandBuffer       command_buffer)
{
  GskGpuDescriptors *desc = GSK_GPU_DESCRIPTORS (self);
  gsize i;

  for (i = 0; i < gsk_gpu_descriptors_get_size (desc); i++)
    {
      gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (gsk_gpu_descriptors_get_image (desc, i)),
                                   command_buffer,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                   VK_ACCESS_SHADER_READ_BIT);
    }
}
