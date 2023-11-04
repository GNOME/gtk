#include "config.h"

#include "gskvulkanframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkandescriptorsprivate.h"
#include "gskvulkandeviceprivate.h"
#include "gskvulkanimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"

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

struct _GskVulkanFrame
{
  GskGpuFrame parent_instance;

  GskVulkanPipelineLayout *pipeline_layout;
  GskSamplers immutable_samplers;
  GskDescriptorImageInfos descriptor_immutable_images;
  GskDescriptorImageInfos descriptor_images;
  GskDescriptorBufferInfos descriptor_buffers;

  VkFence vk_fence;
  VkCommandBuffer vk_command_buffer;
  VkDescriptorPool vk_descriptor_pool;
  gsize pool_n_sets;
  gsize pool_n_samplers;
  gsize pool_n_buffers;
};

struct _GskVulkanFrameClass
{
  GskGpuFrameClass parent_class;
};

G_DEFINE_TYPE (GskVulkanFrame, gsk_vulkan_frame, GSK_TYPE_GPU_FRAME)

static gboolean
gsk_vulkan_frame_is_busy (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  VkDevice device;

  device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)));

  return vkGetFenceStatus (device, self->vk_fence) == VK_NOT_READY;
}

static void
gsk_vulkan_frame_setup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  GSK_VK_CHECK (vkAllocateCommandBuffers, vk_device,
                                          &(VkCommandBufferAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = vk_command_pool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1,
                                          },
                                          &self->vk_command_buffer);

  GSK_VK_CHECK (vkCreateFence, vk_device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->vk_fence);
}

static void
gsk_vulkan_frame_cleanup (GskGpuFrame *frame)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  GSK_VK_CHECK (vkWaitForFences, vk_device,
                                 1,
                                 &self->vk_fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, vk_device,
                               1,
                               &self->vk_fence);

  GSK_VK_CHECK (vkResetCommandBuffer, self->vk_command_buffer,
                                      0);

  if (self->vk_descriptor_pool != VK_NULL_HANDLE)
    {
      GSK_VK_CHECK (vkResetDescriptorPool, vk_device,
                                           self->vk_descriptor_pool,
                                           0);
    }
  gsk_samplers_set_size (&self->immutable_samplers, 0);
  gsk_descriptor_image_infos_set_size (&self->descriptor_immutable_images, 0);
  gsk_descriptor_image_infos_set_size (&self->descriptor_images, 0);
  gsk_descriptor_buffer_infos_set_size (&self->descriptor_buffers, 0);

  gsk_vulkan_device_release_pipeline_layout (device, self->pipeline_layout);
  self->pipeline_layout = NULL;

  GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->cleanup (frame);
}

static GskGpuImage *
gsk_vulkan_frame_upload_texture (GskGpuFrame  *frame,
                                 gboolean      with_mipmap,
                                 GdkTexture   *texture)
{
#ifdef HAVE_DMABUF
  if (GDK_IS_DMABUF_TEXTURE (texture))
    {
      GskGpuImage *image = gsk_vulkan_image_new_for_dmabuf (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                            texture);
      if (image)
        return image;
    }
#endif

  return GSK_GPU_FRAME_CLASS (gsk_vulkan_frame_parent_class)->upload_texture (frame, with_mipmap, texture);
}

guint32
gsk_vulkan_frame_add_image (GskVulkanFrame *self,
                            GskGpuImage    *image,
                            GskGpuSampler   sampler)
{
  GskVulkanImage *vulkan_image = GSK_VULKAN_IMAGE (image);
  GskVulkanDevice *device;
  VkSampler vk_sampler;
  guint32 result;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_sampler = gsk_vulkan_image_get_vk_sampler (vulkan_image);

  if (vk_sampler)
    {
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
      result = gsk_descriptor_image_infos_get_size (&self->descriptor_images);
      result = result << 1;

      gsk_descriptor_image_infos_append (&self->descriptor_images,
                                         &(VkDescriptorImageInfo) {
                                           .sampler = gsk_vulkan_device_get_vk_sampler (device, sampler),
                                           .imageView = gsk_vulkan_image_get_vk_image_view (vulkan_image),
                                           .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                         });
    }

  return result;
}

static guint32
gsk_vulkan_frame_get_buffer_descriptor (GskVulkanFrame *self,
                                        GskGpuBuffer   *buffer)
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

static void
gsk_vulkan_frame_prepare_descriptor_sets (GskVulkanFrame *self)
{
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkWriteDescriptorSet write_descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS + 1];
  gsize n_descriptor_sets;
  VkDescriptorSet descriptor_sets[GSK_VULKAN_N_DESCRIPTOR_SETS];
  gsize n_samplers, n_buffers;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  n_samplers = gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) * 3 +
               gsk_descriptor_image_infos_get_size (&self->descriptor_images);
  n_buffers = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);

  if (n_samplers > self->pool_n_samplers ||
      n_buffers > self->pool_n_buffers)
    {
      if (self->vk_descriptor_pool != VK_NULL_HANDLE)
        {
          vkDestroyDescriptorPool (vk_device,
                                   self->vk_descriptor_pool,
                                   NULL);
          self->vk_descriptor_pool = VK_NULL_HANDLE;
        }
      if (n_samplers > self->pool_n_samplers)
        self->pool_n_samplers = 2 << g_bit_nth_msf (n_samplers - 1, -1);
      if (n_buffers > self->pool_n_buffers)
        self->pool_n_buffers = 2 << g_bit_nth_msf (n_buffers - 1, -1);
    }

  if (self->vk_descriptor_pool == VK_NULL_HANDLE)
    {
      GSK_VK_CHECK (vkCreateDescriptorPool, vk_device,
                                            &(VkDescriptorPoolCreateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                                .maxSets = self->pool_n_sets,
                                                .poolSizeCount = 2,
                                                .pPoolSizes = (VkDescriptorPoolSize[2]) {
                                                    {
                                                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        .descriptorCount = self->pool_n_samplers,
                                                    },
                                                    {
                                                        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        .descriptorCount = self->pool_n_buffers,
                                                    }
                                                }
                                            },
                                            NULL,
                                            &self->vk_descriptor_pool);
    }

  GSK_VK_CHECK (vkAllocateDescriptorSets, vk_device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->vk_descriptor_pool,
                                              .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                              .pSetLayouts = (VkDescriptorSetLayout[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                gsk_vulkan_device_get_vk_image_set_layout (device, self->pipeline_layout),
                                                gsk_vulkan_device_get_vk_buffer_set_layout (device, self->pipeline_layout),
                                              },
                                              .pNext = &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = GSK_VULKAN_N_DESCRIPTOR_SETS,
                                                .pDescriptorCounts = (uint32_t[GSK_VULKAN_N_DESCRIPTOR_SETS]) {
                                                  MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) + 
                                                          gsk_descriptor_image_infos_get_size (&self->descriptor_images)),
                                                  MAX (1, gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers))
                                                }
                                              }
                                          },
                                          descriptor_sets);

  n_descriptor_sets = 0;
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_immutable_images) > 0)
    {
      write_descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
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
          .dstSet = descriptor_sets[GSK_VULKAN_IMAGE_SET_LAYOUT],
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
          .dstSet = descriptor_sets[GSK_VULKAN_BUFFER_SET_LAYOUT],
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

  vkCmdBindDescriptorSets (self->vk_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           gsk_vulkan_device_get_vk_pipeline_layout (device, self->pipeline_layout),
                           0,
                           GSK_VULKAN_N_DESCRIPTOR_SETS,
                           descriptor_sets,
                           0,
                           NULL);
}

static GskGpuDescriptors *
gsk_vulkan_frame_create_descriptors (GskGpuFrame *frame)
{
  return GSK_GPU_DESCRIPTORS (gsk_vulkan_descriptors_new (GSK_VULKAN_FRAME (frame)));
}

static GskGpuBuffer *
gsk_vulkan_frame_create_vertex_buffer (GskGpuFrame *frame,
                                       gsize        size)
{
  return gsk_vulkan_buffer_new_vertex (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static GskGpuBuffer *
gsk_vulkan_frame_create_storage_buffer (GskGpuFrame *frame,
                                        gsize        size)
{
  return gsk_vulkan_buffer_new_storage (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)), size);
}

static void
gsk_vulkan_frame_submit (GskGpuFrame  *frame,
                         GskGpuBuffer *vertex_buffer,
                         GskGpuBuffer *storage_buffer,
                         GskGpuOp     *op)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (frame);
  GskVulkanCommandState state;

  if (storage_buffer)
    {
      G_GNUC_UNUSED guint32 descriptor;
      descriptor = gsk_vulkan_frame_get_buffer_descriptor (self, storage_buffer);
      g_assert (descriptor == 0);
    }

  self->pipeline_layout = gsk_vulkan_device_acquire_pipeline_layout (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                                                     gsk_samplers_get_data (&self->immutable_samplers),
                                                                     gsk_samplers_get_size (&self->immutable_samplers),
                                                                     gsk_descriptor_image_infos_get_size (&self->descriptor_images),
                                                                     gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers));

  GSK_VK_CHECK (vkBeginCommandBuffer, self->vk_command_buffer,
                                      &(VkCommandBufferBeginInfo) {
                                          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                      });

  gsk_vulkan_frame_prepare_descriptor_sets (self);

  if (vertex_buffer)
    vkCmdBindVertexBuffers (self->vk_command_buffer,
                            0,
                            1,
                            (VkBuffer[1]) {
                                gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (vertex_buffer))
                            },
                            (VkDeviceSize[1]) { 0 });

  state.vk_command_buffer = self->vk_command_buffer;
  state.vk_render_pass = VK_NULL_HANDLE;
  state.vk_format = VK_FORMAT_UNDEFINED;

  while (op)
    {
      op = gsk_gpu_op_vk_command (op, frame, &state);
    }

  GSK_VK_CHECK (vkEndCommandBuffer, self->vk_command_buffer);

  GSK_VK_CHECK (vkQueueSubmit, gsk_vulkan_device_get_vk_queue (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame))),
                               1,
                               &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &self->vk_command_buffer,
                               },
                               self->vk_fence);
}

static void
gsk_vulkan_frame_finalize (GObject *object)
{
  GskVulkanFrame *self = GSK_VULKAN_FRAME (object);
  GskVulkanDevice *device;
  VkDevice vk_device;
  VkCommandPool vk_command_pool;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  vk_command_pool = gsk_vulkan_device_get_vk_command_pool (device);

  if (self->vk_descriptor_pool != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool (vk_device,
                               self->vk_descriptor_pool,
                               NULL);
    }
  gsk_samplers_clear (&self->immutable_samplers);
  gsk_descriptor_image_infos_clear (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_clear (&self->descriptor_images);
  gsk_descriptor_buffer_infos_clear (&self->descriptor_buffers);

  vkFreeCommandBuffers (vk_device,
                        vk_command_pool,
                        1, &self->vk_command_buffer);
  vkDestroyFence (vk_device,
                  self->vk_fence,
                  NULL);

  G_OBJECT_CLASS (gsk_vulkan_frame_parent_class)->finalize (object);
}

static void
gsk_vulkan_frame_class_init (GskVulkanFrameClass *klass)
{
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_frame_class->is_busy = gsk_vulkan_frame_is_busy;
  gpu_frame_class->setup = gsk_vulkan_frame_setup;
  gpu_frame_class->cleanup = gsk_vulkan_frame_cleanup;
  gpu_frame_class->upload_texture = gsk_vulkan_frame_upload_texture;
  gpu_frame_class->create_descriptors = gsk_vulkan_frame_create_descriptors;
  gpu_frame_class->create_vertex_buffer = gsk_vulkan_frame_create_vertex_buffer;
  gpu_frame_class->create_storage_buffer = gsk_vulkan_frame_create_storage_buffer;
  gpu_frame_class->submit = gsk_vulkan_frame_submit;

  object_class->finalize = gsk_vulkan_frame_finalize;
}

static void
gsk_vulkan_frame_init (GskVulkanFrame *self)
{
  gsk_samplers_init (&self->immutable_samplers);
  gsk_descriptor_image_infos_init (&self->descriptor_immutable_images);
  gsk_descriptor_image_infos_init (&self->descriptor_images);
  gsk_descriptor_buffer_infos_init (&self->descriptor_buffers);

  self->pool_n_sets = 4;
  self->pool_n_samplers = 8;
  self->pool_n_buffers = 8;
}

VkFence
gsk_vulkan_frame_get_vk_fence (GskVulkanFrame *self)
{
  return self->vk_fence;
}

GskVulkanPipelineLayout *
gsk_vulkan_frame_get_pipeline_layout (GskVulkanFrame *self)
{
  return self->pipeline_layout;
}

