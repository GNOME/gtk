#include "config.h"

#include "gskvulkanimageprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanmemoryprivate.h"
#include "gskvulkanpipelineprivate.h"

#include <string.h>

struct _GskVulkanUploader
{
  GdkVulkanContext *vulkan;

  GskVulkanCommandPool *command_pool;

  GArray *before_barriers;
  VkCommandBuffer copy_buffer;
  GArray *after_barriers;

  GSList *staging_image_free_list;
  GSList *staging_buffer_free_list;
};

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

GskVulkanUploader *
gsk_vulkan_uploader_new (GdkVulkanContext     *context,
                         GskVulkanCommandPool *command_pool)
{
  GskVulkanUploader *self;

  self = g_slice_new0 (GskVulkanUploader);

  self->vulkan = g_object_ref (context);
  self->command_pool = command_pool;

  self->before_barriers = g_array_new (FALSE, FALSE, sizeof (VkImageMemoryBarrier));
  self->after_barriers = g_array_new (FALSE, FALSE, sizeof (VkImageMemoryBarrier));

  return self;
}

void
gsk_vulkan_uploader_free (GskVulkanUploader *self)
{
  gsk_vulkan_uploader_reset (self);

  g_array_unref (self->after_barriers);
  g_array_unref (self->before_barriers);

  g_object_unref (self->vulkan);

  g_slice_free (GskVulkanUploader, self);
}

static void
gsk_vulkan_uploader_add_image_barrier (GskVulkanUploader          *self,
                                       gboolean                    after,
                                       const VkImageMemoryBarrier *barrier)
{
  GArray *array;

  if (after)
    array = self->after_barriers;
  else
    array = self->before_barriers;

  g_array_append_val (array, *barrier);
}

static VkCommandBuffer
gsk_vulkan_uploader_get_copy_buffer (GskVulkanUploader *self)
{
  if (self->copy_buffer == VK_NULL_HANDLE)
    self->copy_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);

  return self->copy_buffer;
}

void
gsk_vulkan_uploader_upload (GskVulkanUploader *self)
{
  if (self->before_barriers->len > 0)
    {
      VkCommandBuffer command_buffer;

      command_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);
      vkCmdPipelineBarrier (command_buffer,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            0,
                            0, NULL,
                            0, NULL,
                            self->before_barriers->len, (VkImageMemoryBarrier *) self->before_barriers->data);
      gsk_vulkan_command_pool_submit_buffer (self->command_pool, command_buffer, VK_NULL_HANDLE);
      g_array_set_size (self->before_barriers, 0);
    }

  /* append these to existing buffer */
  if (self->after_barriers->len > 0)
    {
      VkCommandBuffer command_buffer = gsk_vulkan_uploader_get_copy_buffer (self);
      vkCmdPipelineBarrier (command_buffer,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            0,
                            0, NULL,
                            0, NULL,
                            self->after_barriers->len, (VkImageMemoryBarrier *) self->after_barriers->data);
      g_array_set_size (self->after_barriers, 0);
    }

  if (self->copy_buffer != VK_NULL_HANDLE)
    {
      gsk_vulkan_command_pool_submit_buffer (self->command_pool, self->copy_buffer, VK_NULL_HANDLE);
      self->copy_buffer = VK_NULL_HANDLE;
    }
}

void
gsk_vulkan_uploader_reset (GskVulkanUploader *self)
{
  g_array_set_size (self->before_barriers, 0);
  self->copy_buffer = VK_NULL_HANDLE;
  g_array_set_size (self->after_barriers, 0);

  g_slist_free_full (self->staging_image_free_list, g_object_unref);
  self->staging_image_free_list = NULL;
  g_slist_free_full (self->staging_buffer_free_list, (GDestroyNotify) gsk_vulkan_buffer_free);
  self->staging_buffer_free_list = NULL;
}

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
                                    .format = VK_FORMAT_B8G8R8A8_UNORM,
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
gsk_vulkan_image_ensure_view (GskVulkanImage *self,
                              VkFormat        format)
{
  GSK_VK_CHECK (vkCreateImageView, gdk_vulkan_context_get_device (self->vulkan),
                                   &(VkImageViewCreateInfo) {
                                       .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = self->vk_image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = format,
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

static GskVulkanImage *
gsk_vulkan_image_new_from_data_via_staging_buffer (GskVulkanUploader *uploader,
                                                   guchar            *data,
                                                   gsize              width,
                                                   gsize              height,
                                                   gsize              stride)
{
  GskVulkanImage *self;
  GskVulkanBuffer *staging;
  guchar *mem;

  staging = gsk_vulkan_buffer_new_staging (uploader->vulkan, width * height * 4);
  mem = gsk_vulkan_buffer_map (staging);

  if (stride == width * 4)
    {
      memcpy (mem, data, stride * height);
    }
  else
    {
      for (gsize i = 0; i < height; i++)
        {
          memcpy (mem + i * width * 4, data + i * stride, width * 4);
        }
    }

  gsk_vulkan_buffer_unmap (staging);

  self = gsk_vulkan_image_new (uploader->vulkan,
                               width,
                               height, 
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         FALSE,
                                         &(VkImageMemoryBarrier) {
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
                                         });

  vkCmdCopyBufferToImage (gsk_vulkan_uploader_get_copy_buffer (uploader),
                          gsk_vulkan_buffer_get_buffer (staging),
                          self->vk_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          1,
                          (VkBufferImageCopy[1]) {
                               {
                                   .bufferOffset = 0,
                                   .imageSubresource = {
                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1
                                   },
                                   .imageOffset = { 0, 0, 0 },
                                   .imageExtent = {
                                       .width = width,
                                       .height = height,
                                       .depth = 1
                                   }
                               }
                          });

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         &(VkImageMemoryBarrier) {
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
                                         });

  uploader->staging_buffer_free_list = g_slist_prepend (uploader->staging_buffer_free_list, staging);

  gsk_vulkan_image_ensure_view (self, VK_FORMAT_B8G8R8A8_UNORM);

  return self;
}

static GskVulkanImage *
gsk_vulkan_image_new_from_data_via_staging_image (GskVulkanUploader *uploader,
                                                  guchar            *data,
                                                  gsize              width,
                                                  gsize              height,
                                                  gsize              stride)
{
  GskVulkanImage *self, *staging;

  staging = gsk_vulkan_image_new (uploader->vulkan,
                                  width,
                                  height, 
                                  VK_IMAGE_TILING_LINEAR,
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  gsk_vulkan_image_upload_data (staging, data, width, height, stride);

  self = gsk_vulkan_image_new (uploader->vulkan,
                               width,
                               height, 
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         FALSE,
                                         &(VkImageMemoryBarrier) {
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
                                         });
  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         FALSE,
                                         &(VkImageMemoryBarrier) {
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
                                         });

  vkCmdCopyImage (gsk_vulkan_uploader_get_copy_buffer (uploader),
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

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         &(VkImageMemoryBarrier) {
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
                                         });

  uploader->staging_image_free_list = g_slist_prepend (uploader->staging_image_free_list, staging);

  gsk_vulkan_image_ensure_view (self, VK_FORMAT_B8G8R8A8_UNORM);

  return self;
}

static GskVulkanImage *
gsk_vulkan_image_new_from_data_directly (GskVulkanUploader *uploader,
                                         guchar            *data,
                                         gsize              width,
                                         gsize              height,
                                         gsize              stride)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (uploader->vulkan,
                               width,
                               height, 
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  gsk_vulkan_image_upload_data (self, data, width, height, stride);

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         TRUE,
                                         &(VkImageMemoryBarrier) {
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
                                         });

  gsk_vulkan_image_ensure_view (self, VK_FORMAT_B8G8R8A8_UNORM);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_from_data (GskVulkanUploader *uploader,
                                guchar            *data,
                                gsize              width,
                                gsize              height,
                                gsize              stride)
{
  if (GSK_RENDER_MODE_CHECK (STAGING_BUFFER))
    return gsk_vulkan_image_new_from_data_via_staging_buffer (uploader, data, width, height, stride);
  if (GSK_RENDER_MODE_CHECK (STAGING_IMAGE))
    return gsk_vulkan_image_new_from_data_via_staging_image (uploader, data, width, height, stride);
  else
    return gsk_vulkan_image_new_from_data_directly (uploader, data, width, height, stride);
}

GskVulkanImage *
gsk_vulkan_image_new_for_swapchain (GdkVulkanContext *context,
                                    VkImage           image,
                                    VkFormat          format,
                                    gsize             width,
                                    gsize             height)
{
  GskVulkanImage *self;

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->vulkan = g_object_ref (context);
  self->width = width;
  self->height = height;
  self->vk_image = image;

  gsk_vulkan_image_ensure_view (self, VK_FORMAT_B8G8R8A8_UNORM);

  return self;
}

GskVulkanImage *
gsk_vulkan_image_new_for_framebuffer (GdkVulkanContext *context,
                                      gsize             width,
                                      gsize             height)
{
  GskVulkanImage *self;


  self = gsk_vulkan_image_new (context,
                               width,
                               height, 
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  gsk_vulkan_image_ensure_view (self, VK_FORMAT_B8G8R8A8_UNORM);

  return self;
}

GskTexture *
gsk_vulkan_image_download (GskVulkanImage    *self,
                           GskVulkanUploader *uploader)
{
  GskVulkanBuffer *buffer;
  GskTexture *texture;
  guchar *mem;

  gsk_vulkan_uploader_add_image_barrier (uploader,
                                         FALSE,
                                         &(VkImageMemoryBarrier) {
                                             .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                             .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                             .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                             .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
                                         });

  buffer = gsk_vulkan_buffer_new_download (self->vulkan, self->width * self->height * 4);

  vkCmdCopyImageToBuffer (gsk_vulkan_uploader_get_copy_buffer (uploader),
                          self->vk_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          gsk_vulkan_buffer_get_buffer (buffer),
                          1,
                          (VkBufferImageCopy[1]) {
                               {
                                   .bufferOffset = 0,
                                   .imageSubresource = {
                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1
                                   },
                                   .imageOffset = { 0, 0, 0 },
                                   .imageExtent = {
                                       .width = self->width,
                                       .height = self->height,
                                       .depth = 1
                                   }
                               }
                          });

  gsk_vulkan_uploader_upload (uploader);

  GSK_VK_CHECK (vkQueueWaitIdle, gdk_vulkan_context_get_queue (self->vulkan));

  mem = gsk_vulkan_buffer_map (buffer);
  texture = gsk_texture_new_for_data (mem, self->width, self->height, self->width * 4);
  gsk_vulkan_buffer_unmap (buffer);
  gsk_vulkan_buffer_free (buffer);

  return texture;
}

static void
gsk_vulkan_image_finalize (GObject *object)
{
  GskVulkanImage *self = GSK_VULKAN_IMAGE (object);

  if (self->vk_image_view != VK_NULL_HANDLE)
    {
      vkDestroyImageView (gdk_vulkan_context_get_device (self->vulkan),
                          self->vk_image_view,
                          NULL);
    }

  /* memory is NULL for for_swapchain() images, where we don't own
   * the VkImage */
  if (self->memory)
    {
      gsk_vulkan_memory_free (self->memory);

      vkDestroyImage (gdk_vulkan_context_get_device (self->vulkan),
                      self->vk_image,
                      NULL);
    }

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

