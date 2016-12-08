#include "config.h"

#include "gskvulkanimageprivate.h"
#include "gskvulkanmemoryprivate.h"
#include "gskvulkanpipelineprivate.h"

#include <string.h>

struct _GskVulkanImage
{
  GObject parent_instance;

  GdkVulkanContext *vulkan;

  gsize width;
  gsize height;
  VkImage vk_image;
  VkImageView vk_image_view;

  GskVulkanMemory *memory;
};

G_DEFINE_TYPE (GskVulkanImage, gsk_vulkan_image, G_TYPE_OBJECT)

static GskVulkanImage *
gsk_vulkan_image_new (GdkVulkanContext      *context,
                      gsize                  width,
                      gsize                  height,
                      VkImageTiling          tiling,
                      VkImageUsageFlags      usage,
                      VkMemoryPropertyFlags  memory)
{
  VkMemoryRequirements requirements;
  GskVulkanImage *self;

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->vulkan = g_object_ref (context);
  self->width = width;
  self->height = height;

  GSK_VK_CHECK (vkCreateImage, gdk_vulkan_context_get_device (context),
                                &(VkImageCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .flags = 0,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = VK_FORMAT_B8G8R8A8_SRGB,
                                    .extent = { width, height, 1 },
                                    .mipLevels = 1,
                                    .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = tiling,
                                    .usage = usage,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
                                },
                                NULL,
                                &self->vk_image);

  vkGetImageMemoryRequirements (gdk_vulkan_context_get_device (context),
                                self->vk_image,
                                &requirements);

  self->memory = gsk_vulkan_memory_new (context,
                                        requirements.memoryTypeBits,
                                        memory,
                                        requirements.size);

  GSK_VK_CHECK (vkBindImageMemory, gdk_vulkan_context_get_device (context),
                                   self->vk_image,
                                   gsk_vulkan_memory_get_device_memory (self->memory),
                                   0);
  return self;
}

static void
gsk_vulkan_image_upload_data (GskVulkanImage *self,
                              guchar         *data,
                              gsize           width,
                              gsize           height,
                              gsize           data_stride)
{
  VkMemoryRequirements requirements;
  gsize mem_stride;
  guchar *mem;

  mem_stride = width * 4;
  vkGetImageMemoryRequirements (gdk_vulkan_context_get_device (self->vulkan),
                                self->vk_image,
                                &requirements);
  if (mem_stride % requirements.alignment != 0)
    mem_stride = (mem_stride / requirements.alignment + 1) * requirements.alignment;

  mem = gsk_vulkan_memory_map (self->memory);

  if (data_stride == width * 4 && data_stride == mem_stride)
    {
      memcpy (mem, data, data_stride * height);
    }
  else
    {
      for (gsize i = 0; i < height; i++)
        {
          memcpy (mem + i * mem_stride, data + i * data_stride, width * 4);
        }
    }

  gsk_vulkan_memory_unmap (self->memory);
}

static void
gsk_vulkan_image_ensure_view (GskVulkanImage *self)
{
  GSK_VK_CHECK (vkCreateImageView, gdk_vulkan_context_get_device (self->vulkan),
                                   &(VkImageViewCreateInfo) {
                                       .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = self->vk_image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = VK_FORMAT_B8G8R8A8_SRGB,
                                       .components = {
                                           .r = VK_COMPONENT_SWIZZLE_R,
                                           .g = VK_COMPONENT_SWIZZLE_G,
                                           .b = VK_COMPONENT_SWIZZLE_B,
                                           .a = VK_COMPONENT_SWIZZLE_A,
                                       },
                                       .subresourceRange = {
                                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                           .baseMipLevel = 0,
                                           .levelCount = 1,
                                           .baseArrayLayer = 0,
                                           .layerCount = 1,
                                       },
                                   },
                                   NULL,
                                   &self->vk_image_view);
}

GskVulkanImage *
gsk_vulkan_image_new_from_data_via_staging_image (GdkVulkanContext  *context,
                                                  VkCommandBuffer    command_buffer,
                                                  guchar            *data,
                                                  gsize              width,
                                                  gsize              height,
                                                  gsize              stride)
{
  GskVulkanImage *self, *staging;

  staging = gsk_vulkan_image_new (context,
                                  width,
                                  height, 
                                  VK_IMAGE_TILING_LINEAR,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  gsk_vulkan_image_upload_data (staging, data, width, height, stride);

  self = gsk_vulkan_image_new (context,
                               width,
                               height, 
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vkCmdPipelineBarrier (command_buffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0,
                        0, NULL,
                        0, NULL,
                        2, (VkImageMemoryBarrier[2]) {
                            {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = staging->vk_image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            },
                            {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = self->vk_image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            }
                        });

  vkCmdCopyImage (command_buffer,
                  staging->vk_image,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  self->vk_image,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1,
                  &(VkImageCopy) {
                      .srcSubresource = {
                          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                          .mipLevel = 0,
                          .baseArrayLayer = 0,
                          .layerCount = 1
                      },
                      .srcOffset = { 0, 0, 0 },
                      .dstSubresource = {
                          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                          .mipLevel = 0,
                          .baseArrayLayer = 0,
                          .layerCount = 1
                      },
                      .dstOffset = { 0, 0, 0 },
                      .extent = { 
                          .width = width,
                          .height = height,
                          .depth = 1
                      }
                  });

  vkCmdPipelineBarrier (command_buffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0,
                        0, NULL,
                        0, NULL,
                        1, (VkImageMemoryBarrier[1]) {
                            {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = self->vk_image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            }
                        });

  /* XXX: Is this okay or do we need to keep the staging image around until the commands execute */
  g_object_unref (staging);

  gsk_vulkan_image_ensure_view (self);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_from_data_directly (GdkVulkanContext  *context,
                                         VkCommandBuffer    command_buffer,
                                         guchar            *data,
                                         gsize              width,
                                         gsize              height,
                                         gsize              stride)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (context,
                               width,
                               height, 
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  gsk_vulkan_image_upload_data (self, data, width, height, stride);

  vkCmdPipelineBarrier (command_buffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        0,
                        0, NULL,
                        0, NULL,
                        1, (VkImageMemoryBarrier[1]) {
                            {
                                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
                                .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                .image = self->vk_image,
                                .subresourceRange = {
                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1
                                }
                            }
                        });

  gsk_vulkan_image_ensure_view (self);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_from_data (GdkVulkanContext  *context,
                                VkCommandBuffer    command_buffer,
                                guchar            *data,
                                gsize              width,
                                gsize              height,
                                gsize              stride)
{
  if (GSK_RENDER_MODE_CHECK (STAGING_IMAGE))
    return gsk_vulkan_image_new_from_data_via_staging_image (context, command_buffer, data, width, height, stride);
  else
    return gsk_vulkan_image_new_from_data_directly (context, command_buffer, data, width, height, stride);
}

void
gsk_vulkan_image_finalize (GObject *object)
{
  GskVulkanImage *self = GSK_VULKAN_IMAGE (object);

  if (self->vk_image_view != VK_NULL_HANDLE)
    {
      vkDestroyImageView (gdk_vulkan_context_get_device (self->vulkan),
                          self->vk_image_view,
                          NULL);
    }

  gsk_vulkan_memory_free (self->memory);

  vkDestroyImage (gdk_vulkan_context_get_device (self->vulkan),
                  self->vk_image,
                  NULL);

  g_object_unref (self->vulkan);

  G_OBJECT_CLASS (gsk_vulkan_image_parent_class)->finalize (object);
}

static void
gsk_vulkan_image_class_init (GskVulkanImageClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gsk_vulkan_image_finalize;
}

static void
gsk_vulkan_image_init (GskVulkanImage *self)
{
}

gsize
gsk_vulkan_image_get_width (GskVulkanImage *self)
{
  return self->width;
}

gsize
gsk_vulkan_image_get_height (GskVulkanImage *self)
{
  return self->height;
}

VkImage
gsk_vulkan_image_get_image (GskVulkanImage *self)
{
  return self->vk_image;
}

VkImageView
gsk_vulkan_image_get_image_view (GskVulkanImage *self)
{
  return self->vk_image_view;
}

