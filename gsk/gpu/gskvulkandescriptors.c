#include "config.h"

#include "gskvulkandescriptorsprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"

#define GDK_ARRAY_NAME gsk_descriptor_image_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorImageInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorImageInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 128
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_descriptor_buffer_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorBufferInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorBufferInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_samplers
#define GDK_ARRAY_TYPE_NAME GskSamplers
#define GDK_ARRAY_ELEMENT_TYPE VkSampler
#define GDK_ARRAY_PREALLOC 32
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanDescriptors
{
  GskGpuDescriptors parent_instance;

  GskVulkanDevice *device;

  GskVulkanPipelineLayout *pipeline_layout;

  GskSamplers immutable_samplers;
  GskDescriptorImageInfos descriptor_immutable_images;
  GskDescriptorImageInfos descriptor_images;
  GskDescriptorBufferInfos descriptor_buffers;

  VkDescriptorSet descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS];
};

G_DEFINE_TYPE (GskVulkanDescriptors, gsk_vulkan_descriptors, GSK_TYPE_GPU_DESCRIPTORS)

static gboolean
gsk_vulkan_descriptors_add_image (GskGpuDescriptors *desc,
                                  GskGpuImage       *image,
                                  GskGpuSampler      sampler,
                                  guint32           *out_descriptor)
{
  GskVulkanDescriptors *self = GSK_VULKAN_DESCRIPTORS (desc);
  GskVulkanImage *vulkan_image = GSK_VULKAN_IMAGE (image);
  VkSampler vk_sampler;
  guint32 result;

  vk_sampler = gsk_vulkan_image_get_vk_sampler (vulkan_image);

  if (vk_sampler)
    {
      if (gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) >=
          gsk_vulkan_device_get_max_immutable_samplers (self->device))
        return FALSE;

      result = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) << 1 | 1;

      gsk_samplers_append (&self->immutable_samplers, vk_sampler);
      gsk_descriptor_image_infos_append (&self->descriptor_immutable_images,
                                         &(VkDescriptorImageInfo) {
                                           .imageView = gsk_vulkan_image_get_vk_image_view (vulkan_image),
                                           .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         });
    }
  else
    {
      if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) >=
          gsk_vulkan_device_get_max_samplers (self->device))
        return FALSE;

      result = gsk_descriptor_image_infos_get_size (&self->descriptor_images) << 1;

      gsk_descriptor_image_infos_append (&self->descriptor_images,
                                         &(VkDescriptorImageInfo) {
                                           .sampler = gsk_vulkan_device_get_vk_sampler (self->device, sampler),
                                           .imageView = gsk_vulkan_image_get_vk_image_view (vulkan_image),
                                           .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         });
    }

  *out_descriptor = result;
  return TRUE;
}

static void
gsk_vulkan_descriptors_finalize (GObject *object)
{
  GskVulkanDescriptors *self = GSK_VULKAN_DESCRIPTORS (object);

  gsk_samplers_clear (&self->immutable_samplers);
  gsk_descriptor_image_infos_clear (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_clear (&self->descriptor_images);
  gsk_descriptor_buffer_infos_clear (&self->descriptor_buffers);

  gsk_vulkan_device_release_pipeline_layout (self->device, self->pipeline_layout);

  g_object_unref (self->device);

  G_OBJECT_CLASS (gsk_vulkan_descriptors_parent_class)->finalize (object);
}

static void
gsk_vulkan_descriptors_class_init (GskVulkanDescriptorsClass *klass)
{
  GskGpuDescriptorsClass *descriptors_class = GSK_GPU_DESCRIPTORS_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gsk_vulkan_descriptors_finalize;

  descriptors_class->add_image = gsk_vulkan_descriptors_add_image;
}

static void
gsk_vulkan_descriptors_init (GskVulkanDescriptors *self)
{
  gsk_samplers_init (&self->immutable_samplers);
  gsk_descriptor_image_infos_init (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_init (&self->descriptor_images);
  gsk_descriptor_buffer_infos_init (&self->descriptor_buffers);
}

GskVulkanDescriptors *
gsk_vulkan_descriptors_new (GskVulkanDevice *device)
{
  GskVulkanDescriptors *self;

  self = g_object_new (GSK_TYPE_VULKAN_DESCRIPTORS, NULL);

  self->device = g_object_ref (device);

  return self;
}

gboolean
gsk_vulkan_descriptors_is_full (GskVulkanDescriptors *self)
{
  return gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) >= gsk_vulkan_device_get_max_immutable_samplers (self->device) ||
         gsk_descriptor_image_infos_get_size (&self->descriptor_images) >= gsk_vulkan_device_get_max_samplers (self->device) ||
         gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) >= gsk_vulkan_device_get_max_buffers (self->device);
}

GskVulkanPipelineLayout *
gsk_vulkan_descriptors_get_pipeline_layout (GskVulkanDescriptors *self)
{
  return self->pipeline_layout;
}

VkPipelineLayout
gsk_vulkan_descriptors_get_vk_pipeline_layout (GskVulkanDescriptors *self)
{
  return gsk_vulkan_device_get_vk_pipeline_layout (self->device, self->pipeline_layout);
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

static void
gsk_vulkan_descriptors_fill_sets (GskVulkanDescriptors *self)
{
  gsize n_immutable_samplers, n_samplers, n_buffers;

  if (gsk_vulkan_device_has_feature (self->device, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING))
    return;

  /* If descriptor indexing isn't supported, all descriptors in the shaders
   * must be properly setup. And that means we need to have
   * descriptors for all of them.
   */
  gsk_vulkan_device_get_pipeline_sizes (self->device,
                                        self->pipeline_layout,
                                        &n_immutable_samplers,
                                        &n_samplers,
                                        &n_buffers);

  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) == 0)
    {
      guint32 ignored;
      /* We have no image, find any random image and attach it */
      if (!gsk_gpu_descriptors_add_image (GSK_GPU_DESCRIPTORS (self),
                                          gsk_gpu_device_get_atlas_image (GSK_GPU_DEVICE (self->device)),
                                          GSK_GPU_SAMPLER_DEFAULT,
                                          &ignored))
        {
          g_assert_not_reached ();
        }
    }
  while (n_immutable_samplers > gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images))
    {
      gsk_descriptor_image_infos_append (&self->descriptor_immutable_images, gsk_descriptor_image_infos_get (&self->descriptor_images, 0));
    }
  while (n_samplers > gsk_descriptor_image_infos_get_size (&self->descriptor_images))
    {
      gsk_descriptor_image_infos_append (&self->descriptor_images, gsk_descriptor_image_infos_get (&self->descriptor_images, 0));
    }
  /* That should be the storage buffer */
  g_assert (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) > 0);
  while (n_buffers > gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers))
    {
      gsk_descriptor_buffer_infos_append (&self->descriptor_buffers, gsk_descriptor_buffer_infos_get (&self->descriptor_buffers, 0));
    }
}

void
gsk_vulkan_descriptors_prepare (GskVulkanDescriptors *self,
                                gsize                *n_images,
                                gsize                *n_buffers)
{
  self->pipeline_layout = gsk_vulkan_device_acquire_pipeline_layout (self->device,
                                                                     gsk_samplers_get_data (&self->immutable_samplers),
                                                                     gsk_samplers_get_size (&self->immutable_samplers),
                                                                     gsk_descriptor_image_infos_get_size (&self->descriptor_images),
                                                                     gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers));
  gsk_vulkan_descriptors_fill_sets (self);

  *n_images = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) + 
              gsk_descriptor_image_infos_get_size (&self->descriptor_images);
  *n_buffers = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
}

void
gsk_vulkan_descriptors_update_sets (GskVulkanDescriptors *self,
                                    VkDescriptorPool      vk_descriptor_pool)
{
  VkWriteDescriptorSet write_descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS + 1];
  gsize n_descriptor_sets;
  VkDevice vk_device;
  gboolean descriptor_indexing;

  descriptor_indexing = gsk_vulkan_device_has_feature (self->device, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING);
  vk_device = gsk_vulkan_device_get_vk_device (self->device);

  GSK_VK_CHECK (vkAllocateDescriptorSets, vk_device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = vk_descriptor_pool,
                                              .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                              .pSetLayouts = (VkDescriptorSetLayout[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                gsk_vulkan_device_get_vk_image_set_layout (self->device, self->pipeline_layout),
                                                gsk_vulkan_device_get_vk_buffer_set_layout (self->device, self->pipeline_layout),
                                              },
                                              .pNext = !descriptor_indexing ? NULL : &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                                .pDescriptorCounts = (uint32_t[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                  gsk_descriptor_image_infos_get_size (&self->descriptor_images),
                                                  gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers)
                                                }
                                              }
                                          },
                                          self->descriptor_sets);

  n_descriptor_sets = 0;
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_immutable_images)
      };
    }
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_images)
      };
    }
  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[GSK_VULKAN_BUFFER_SET_LAYOUT],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = gsk_descriptor_buffer_infos_get_data (&self->descriptor_buffers)
      };
    }

  vkUpdateDescriptorSets (vk_device,
                          n_descriptor_sets,
                          write_descriptor_sets,
                          0, NULL);
}

guint32
gsk_vulkan_descriptors_get_buffer_descriptor (GskVulkanDescriptors *self,
                                              GskGpuBuffer         *buffer)
{
  guint32 result;

  result = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
  gsk_descriptor_buffer_infos_append (&self->descriptor_buffers,
                                      &(VkDescriptorBufferInfo) {
                                        .buffer = gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (buffer)),
                                        .offset = 0,
                                        .range = VK_WHOLE_SIZE
                                      });

  return result;
}

void
gsk_vulkan_descriptors_bind (GskVulkanDescriptors *self,
                             VkCommandBuffer       vk_command_buffer)
{
  vkCmdBindDescriptorSets (vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           gsk_vulkan_device_get_vk_pipeline_layout (self->device, self->pipeline_layout),
                           0,
                           G_N_ELEMENTS (self->descriptor_sets),
                           self->descriptor_sets,
                           0,
                           NULL);
}
